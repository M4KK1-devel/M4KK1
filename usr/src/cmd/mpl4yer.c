/*
 * ==== MPL4YER — media player ====
 * M4KK1 4P1 - usr/src/cmd/mpl4yer.c
 * Description: Copland client playing 8-bit unsigned mono 22050Hz
 *              RAW/WAV files via m4k_play_pcm (SB16 single-cycle
 *              DMA, 32KB chunks).  Video: none (audio spectrum
 *              bars animated from the chunk being played).
 *              With no file argument a built-in demo melody is
 *              synthesized (square waves → PCM) so the app is
 *              testable with zero assets.
 *
 * Keys:  q/Esc close, Space play/pause, s stop,
 *       Left/Right seek ±5s, Up/Down volume (0..8, scales
 *       samples), o open /etc/motd-style path? no — o reloads
 *       the file argument.
 * Click: [Play/Pause] [Stop] buttons, progress bar seek.
 *
 * The file argument comes via argv — m4k_spawn does not pass
 * argv, so the player ALSO polls /export/cfg/mpl4yer/now.txt
 * (one line path); fm writes it before spawning us.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define MP_W    382
#define MP_H    240

#define MP_MB_ADDR    0x006A0000u
#define MP_MB_MAGIC   0x4D503100u   /* "MP1 " */

#define MP_CHUNK      8192
#define MP_MAX_PCM    (22050 * 30)   /* 30 s ceiling */

static uint32_t mp_buf[MP_W * MP_H];

static struct ga_app app = {
    .w = MP_W, .h = MP_H,
    .mb_addr = MP_MB_ADDR, .mb_magic = MP_MB_MAGIC,
    .title = "mpl4yer",
};

/* playback state */
static uint8_t pcm[MP_MAX_PCM];
static int pcm_len;
static int pos;                   /* bytes played */
static int playing;
static int volume = 6;            /* 0..8 */
static char cur_name[28] = "demo melody";
static int is_demo;

/* chunk staging (volume-scaled) */
static uint8_t chunk[MP_CHUNK];

/* spectrum bars: 12 bands, each 0..1000 */
static uint16_t bars[12];

/* ── demo melody synth ── */
static void synth_demo(void)
{
    /* C major arpeggio + scale, 8 notes x 0.32s */
    static const uint16_t freqs[8] = {
        262, 330, 392, 523, 392, 330, 262, 523
    };
    int sr = 22050;
    int note = sr * 32 / 100;
    int o = 0;
    for (int n = 0; n < 8 && o < MP_MAX_PCM - note; n++) {
        uint16_t f = freqs[n];
        for (int i = 0; i < note; i++) {
            /* square wave with mild decay envelope */
            int half = sr / (2 * (int)f);
            int phase = (i / (half ? half : 1)) % 2;
            uint32_t env = (uint32_t)(note - i) * 16u
                         / (uint32_t)note;
            int v = phase ? 128 + 60 * (int)env / 16
                          : 128 - 60 * (int)env / 16;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            pcm[o++] = (uint8_t)v;
        }
    }
    pcm_len = o;
    is_demo = 1;
}

/* ── file load (raw or WAV with fmt bypass) ── */
static void load_file(const char *path)
{
    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0) {
        ser_puts("[MPL] open failed ");
        ser_puts(path);
        ser_puts("\n");
        return;
    }
    int n = musr_sc_read(fd, pcm, MP_MAX_PCM);
    musr_sc_close(fd);
    if (n <= 44) {
        ser_puts("[MPL] empty file\n");
        return;
    }
    /* RIFF header: skip 44-byte canonical WAV header */
    int off = 0;
    if (pcm[0] == 'R' && pcm[1] == 'I' && pcm[2] == 'F'
        && pcm[3] == 'F')
        off = 44;
    int len = n - off;
    if (len > MP_MAX_PCM)
        len = MP_MAX_PCM;
    for (int i = 0; i < len; i++)
        pcm[i] = pcm[i + off];
    pcm_len = len;
    pos = 0;
    is_demo = 0;
    /* name: after last slash */
    const char *nm = path, *p2 = path;
    while (*p2) {
        if (*p2 == '/')
            nm = p2 + 1;
        p2++;
    }
    int i = 0;
    while (nm[i] && i < 27) {
        cur_name[i] = nm[i];
        i++;
    }
    cur_name[i] = 0;
    ser_puts("[MPL] LOADED ");
    ser_puts(cur_name);
    ser_puts("\n");
}

/* poll fm's now-playing handoff file */
static void poll_now(void)
{
    char path[96];
    int fd = musr_sc_open("/export/cfg/mpl4yer/now.txt",
                          O_RDONLY);
    if (fd < 0)
        return;
    char b[96];
    int n = musr_sc_read(fd, b, sizeof(b) - 1);
    musr_sc_close(fd);
    if (n <= 0)
        return;
    /* consume: read once, then unlink */
    musr_sc_unlink("/export/cfg/mpl4yer/now.txt");
    while (n > 0 && (b[n - 1] == '\n' || b[n - 1] == '\r'))
        n--;
    b[n] = 0;
    if (n > 0)
        load_file(b);
}

/* ── play one chunk; returns bytes consumed (0 = done) ── */
static int play_chunk(void)
{
    int remain = pcm_len - pos;
    if (remain <= 0)
        return 0;
    int n = remain < MP_CHUNK ? remain : MP_CHUNK;
    int scale = volume * 32;      /* 6*32 ≈ unity */
    for (int i = 0; i < n; i++) {
        int v = ((pcm[pos + i] - 128) * scale >> 8) + 128;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        chunk[i] = (uint8_t)v;
    }
    m4k_play_pcm(chunk, (uint32_t)n);
    pos += n;
    return n;
}

/* spectrum: 12 bands from the upcoming chunk (cheap DFT-free
 * energy per band via average absolute deviation) */
static void compute_bars(void)
{
    int start = pos;
    for (int b = 0; b < 12; b++) {
        int span = 128;
        int acc = 0, cnt = 0;
        for (int i = 0; i < span; i++) {
            int idx = start + b * span + i;
            if (idx >= pcm_len)
                break;
            int d = pcm[idx] - 128;
            acc += d < 0 ? -d : d;
            cnt++;
        }
        int lvl = cnt ? acc / cnt : 0;
        /* 0..63 deviation → 0..1000 permille, boosted */
        int pm = lvl * 24;
        if (pm > 1000)
            pm = 1000;
        /* smooth toward new value */
        int target = pm;
        int cur = bars[b];
        bars[b] = (uint16_t)((cur * 2 + target) / 3);
    }
}

/* ── render ── */
static void render(void)
{
    ga_rect(&app, 0, 0, MP_W, MP_H, 0x00181820);
    ga_chrome(&app, "mpl4yer");

    /* track name */
    ga_str2(&app, 12, GA_TITLE_H + 12, cur_name, 0x00FFFFFF);
    const char *st = playing ? "PLAYING" : (pos ? "PAUSED" : "STOP");
    ga_str(&app, MP_W - 70, GA_TITLE_H + 34, st, 0x0030C030);

    /* spectrum bars */
    int bx = 12, bw = (MP_W - 24 - 11 * 4) / 12;
    for (int b = 0; b < 12; b++) {
        int h = bars[b] * 100 / 1000;
        int bh = h * 90 / 100;
        ga_rect(&app, bx + b * (bw + 4), 150 - bh, bw, bh,
                0x0030A0C0);
    }

    /* progress bar (click to seek) */
    int pb_y = 162;
    ga_rect(&app, 12, pb_y, MP_W - 24, 12, 0x00303040);
    if (pcm_len) {
        int fw = (MP_W - 24) * pos / pcm_len;
        ga_rect(&app, 12, pb_y, fw, 12, 0x00C08020);
    }
    char b[32];
    int o = 0;
    const char *p = "vol ";
    while (*p) b[o++] = *p++;
    b[o++] = '0' + volume;
    p = "  ";
    while (*p) b[o++] = *p++;
    ga_itoa(pos / 22050, b + o);
    o = ga_strlen(b);
    b[o++] = '/';
    ga_itoa(pcm_len / 22050, b + o);
    o = ga_strlen(b);
    b[o++] = 's';
    b[o] = 0;
    ga_str(&app, 12, pb_y + 18, b, 0x00A0A0A0);

    /* buttons */
    ga_button(&app, MP_W - 150, pb_y + 16, 66, 20,
              playing ? "Pause" : "Play", 0);
    ga_button(&app, MP_W - 76, pb_y + 16, 60, 20, "Stop", 0);
}

static void toggle(void)
{
    if (!pcm_len)
        return;
    playing = !playing;
    ser_puts(playing ? "[MPL] PLAY\n" : "[MPL] PAUSE\n");
}

static void stop(void)
{
    playing = 0;
    pos = 0;
    ser_puts("[MPL] STOP\n");
}

static void click(int lx, int ly)
{
    if (ga_in(lx, ly, MP_W - 150, 178, 66, 20)) {
        toggle();
        return;
    }
    if (ga_in(lx, ly, MP_W - 76, 178, 60, 20)) {
        stop();
        return;
    }
    if (ga_in(lx, ly, 12, 162, MP_W - 24, 12) && pcm_len) {
        pos = (lx - 12) * pcm_len / (MP_W - 24);
        pos -= pos % MP_CHUNK;
        if (pos < 0) pos = 0;
        ser_puts("[MPL] SEEK\n");
    }
}

void _start(void)
{
    ser_puts("[MPL] starting\n");
    app.buf = mp_buf;
    if (ga_init(&app) != 0) {
        ser_puts("[MPL] copland not ready\n");
        m4k_exit(1);
    }
    musr_sc_mkdir("/export/cfg");
    musr_sc_mkdir("/export/cfg/mpl4yer");
    poll_now();
    if (pcm_len == 0)
        synth_demo();
    ser_puts("[MPL] surface ready\n");

    render();
    ga_flip(&app);

    uint32_t next_poll = 0;

    for (;;) {
        if (ga_dead(&app))
            break;
        int ch;
        int dirty = 0;
        int pend = 0, cx = 0;
        while ((ch = ga_getkey(&app)) >= 0) {
            if (pend == 1) { cx = ch; pend = 2; continue; }
            if (pend == 2) { click(cx, ch); dirty = 1; pend = 0; continue; }
            if (ch == 0xF1 || ch == 0xF2) { pend = 1; continue; }
            if (ch == 'q' || ch == 27) {
                ser_puts("[MPL] exit\n");
                m4k_exit(0);
            }
            if (ch == ' ') { toggle(); dirty = 1; }
            if (ch == 's') { stop(); dirty = 1; }
            if (ch == 0x6C) {           /* 'l' reload handoff */
                poll_now();
                dirty = 1;
            }
            if (ch == '+' || ch == 0x75) {   /* 'u'/Up */
                if (volume < 8) volume++;
                dirty = 1;
            }
            if (ch == '-' || ch == 0x64) {   /* 'd'/Down */
                if (volume > 0) volume--;
                dirty = 1;
            }
            if (ch == 0x2C) {           /* ',' seek -5s */
                pos -= 5 * 22050;
                if (pos < 0) pos = 0;
                dirty = 1;
            }
            if (ch == 0x2E) {           /* '.' seek +5s */
                pos += 5 * 22050;
                if (pos > pcm_len) pos = pcm_len;
                dirty = 1;
            }
        }

        uint32_t now = musr_sc_uptime();
        if (playing && now >= next_poll) {
            int n = play_chunk();
            compute_bars();
            if (n == 0) {
                playing = 0;
                pos = 0;
                ser_puts("[MPL] END\n");
            }
            next_poll = now + 350;
            dirty = 1;
        }
        if (!playing) {
            /* decay bars */
            for (int b = 0; b < 12; b++)
                if (bars[b] > 4)
                    bars[b] -= 4;
            dirty = 1;
        }

        if (dirty) {
            render();
            ga_flip(&app);
        }
        m4k_sleep(100);
    }
    m4k_exit(0);
}
