/*
 * ==== PREF — system preferences ====
 * M4KK1 4P1 - usr/src/cmd/pref_gui.c
 * Description: Copland client with a left tab column (Wallpaper,
 *              Mouse, Date/Time, Account) and a right settings
 *              pane.  Persisted under /export/cfg/pref/*.conf.
 *
 *   Wallpaper: 6 theme swatches (writes theme=N to
 *              /export/cfg/pref/wallpaper.conf AND applies live
 *              via copland_shm->wallpaper_theme — sprach reads
 *              the file at boot for persistence).
 *   Mouse: speed multiplier 1..8 (persisted, informational until
 *              the driver consumes it).
 *   Date/Time: shows RTC, +/- minute adjust buttons (root only,
 *              writes back via S_RTCWRITE).
 *   Account: current uid, username prompt for the login banner
 *              (persisted).
 *
 * Keys:  q/Esc close, 1..4 switch tabs, Left/Right adjust the
 *       focused item, s save now.
 * Click: tab select, swatch select, buttons.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"
#include "../lib/libcopland.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define PF_W    384
#define PF_H    300

#define PF_MB_ADDR    0x006D0000u
#define PF_MB_MAGIC   0x50524631u   /* "PRF1" */

#define PF_TAB_W    92
#define PF_TABS     4

#define PF_NTHEMES  6

static uint32_t pf_buf[PF_W * PF_H];

static struct ga_app app = {
    .w = PF_W, .h = PF_H,
    .mb_addr = PF_MB_ADDR, .mb_magic = PF_MB_MAGIC,
    .title = "Preferences",
};

static int tab;                    /* 0..3 */
static int wp_theme;               /* 0..5 */
static int mouse_speed = 2;        /* 1..8 */
static char account_name[16] = "makk1";
static char input_tmp[16];         /* rename scratch */

static const char *tab_names[PF_TABS] = {
    "Wallpaper", "Mouse", "Time", "Account"
};

/* theme colors must mirror libgui gui_wallpapers[] */
static const uint32_t theme_top[PF_NTHEMES] = {
    0x00000044, 0x00224488, 0x00331166,
    0x001A1A2E, 0x00663300, 0x00104010
};
static const uint32_t theme_bot[PF_NTHEMES] = {
    0x0066FF, 0x0044AACC, 0x00AA44CC,
    0x00444466, 0x00CCAA33, 0x0030A030
};

/* ── persistence ── */
static void conf_write(const char *key, const char *val)
{
    /* single-line conf: truncate + rewrite */
    char path[48];
    const char *p = "/export/cfg/pref/";
    int o = 0;
    while (*p) path[o++] = *p++;
    const char *k = key;
    while (*k && o < 40) path[o++] = *k++;
    p = ".conf";
    while (*p) path[o++] = *p++;
    path[o] = 0;

    int fd = musr_sc_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        ser_puts("[PREF] write failed ");
        ser_puts(path);
        ser_puts("\n");
        return;
    }
    musr_sc_write(fd, val, ga_strlen(val));
    musr_sc_write(fd, "\n", 1);
    musr_sc_close(fd);
    ser_puts("[PREF] SAVED ");
    ser_puts(path);
    ser_puts(" = ");
    ser_puts(val);
    ser_puts("\n");
}

static int conf_read(const char *key, char *out, int max)
{
    char path[48];
    const char *p = "/export/cfg/pref/";
    int o = 0;
    while (*p) path[o++] = *p++;
    const char *k = key;
    while (*k && o < 40) path[o++] = *k++;
    p = ".conf";
    while (*p) path[o++] = *p++;
    path[o] = 0;

    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    int n = musr_sc_read(fd, out, max - 1);
    musr_sc_close(fd);
    if (n <= 0)
        return -1;
    /* trim trailing newline */
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
        n--;
    out[n] = 0;
    return n;
}

static void load_all(void)
{
    char v[8];
    if (conf_read("wallpaper", v, sizeof(v)) > 0
        && v[0] >= '0' && v[0] <= '5')
        wp_theme = v[0] - '0';
    if (conf_read("mouse", v, sizeof(v)) > 0
        && v[0] >= '1' && v[0] <= '8')
        mouse_speed = v[0] - '0';
    char an[16];
    int n = conf_read("account", an, sizeof(an));
    if (n > 0 && n < 16) {
        for (int i = 0; i < n; i++)
            account_name[i] = an[i];
        account_name[n] = 0;
    }
}

/* ── apply wallpaper live ── */
static void apply_wallpaper(void)
{
    struct copland_shm *shm = copland_shm_get();
    if (shm && shm->magic == COPLAND_SHM_MAGIC)
        shm->wallpaper_theme = (uint32_t)wp_theme;
    char v[2] = { (char)('0' + wp_theme), 0 };
    conf_write("wallpaper", v);
}

/* ── RTC helpers ── */
static uint32_t rtc[8];

static void rtc_read(void)
{
    if (musr_sc_rtcread(rtc) != 1) {
        rtc[0] = rtc[1] = rtc[2] = 0;
        rtc[3] = 4; rtc[4] = 9; rtc[5] = 2026;
    }
}

/* ── render ── */
static void render(void)
{
    ga_rect(&app, 0, 0, PF_W, PF_H, 0x00E8E8EC);
    ga_chrome(&app, "Preferences");

    /* left tab column */
    ga_rect(&app, 0, GA_TITLE_H, PF_TAB_W,
            PF_H - GA_TITLE_H, 0x00C8C8D0);
    for (int i = 0; i < PF_TABS; i++) {
        int y = GA_TITLE_H + 6 + i * 26;
        if (i == tab)
            ga_rect(&app, 2, y, PF_TAB_W - 4, 22, 0x00FFFFFF);
        char b[12];
        b[0] = '1' + i;
        b[1] = ' ';
        b[2] = 0;
        ga_str(&app, 10, y + 7, b, 0x00202020);
        ga_str(&app, 22, y + 7, tab_names[i], 0x00202020);
    }

    int px = PF_TAB_W + 14;
    int pw = PF_W - px - 10;

    if (tab == 0) {
        ga_str2(&app, px, GA_TITLE_H + 10, "Wallpaper",
                0x00202020);
        ga_str(&app, px, GA_TITLE_H + 34,
               "Click a theme (applies + saves):", 0x00606060);
        /* 3x2 swatches, painted with each theme's gradient */
        for (int i = 0; i < PF_NTHEMES; i++) {
            int sx = px + (i % 3) * ((pw - 20) / 3 + 6);
            int sy = GA_TITLE_H + 50 + (i / 3) * 62;
            int sw = (pw - 20) / 3, sh = 50;
            for (int yy = 0; yy < sh; yy++) {
                uint32_t c;
                uint32_t a = theme_top[i], b2 = theme_bot[i];
                /* crude vertical lerp per scanline */
                uint32_t dr = ((b2 >> 16) & 0xFF)
                            - ((a >> 16) & 0xFF);
                uint32_t dg = ((b2 >> 8) & 0xFF)
                            - ((a >> 8) & 0xFF);
                uint32_t db = (b2 & 0xFF) - (a & 0xFF);
                c = (((((a >> 16) & 0xFF)
                     + dr * yy / sh) & 0xFF) << 16)
                  | (((((a >> 8) & 0xFF)
                     + dg * yy / sh) & 0xFF) << 8)
                  | (((a & 0xFF)
                     + db * yy / sh) & 0xFF);
                ga_rect(&app, sx, sy + yy, sw, 1, c);
            }
            if (i == wp_theme)
                ga_rect(&app, sx - 2, sy - 2, sw + 4, sh + 4,
                        0x00FFFFFF);
            char n[2] = { (char)('0' + i), 0 };
            ga_str(&app, sx + sw / 2 - 3, sy + sh + 4, n,
                   0x00202020);
        }
    } else if (tab == 1) {
        ga_str2(&app, px, GA_TITLE_H + 10, "Mouse", 0x00202020);
        ga_str(&app, px, GA_TITLE_H + 34,
               "Pointer speed (Left/Right):", 0x00606060);
        for (int i = 0; i < 8; i++) {
            int bx = px + i * ((pw - 20) / 8 + 2);
            int bw2 = (pw - 20) / 8 - 2;
            ga_rect(&app, bx, GA_TITLE_H + 52, bw2, 22,
                    i < mouse_speed ? 0x003060C0 : 0x00A0A0A8);
        }
        char v[4];
        v[0] = '0' + mouse_speed;
        v[1] = 0;
        ga_str2(&app, px + pw - 30, GA_TITLE_H + 84, v, 0x00202020);
        ga_button(&app, px, GA_TITLE_H + 120, 70, 18, "Save", 0);
    } else if (tab == 2) {
        ga_str2(&app, px, GA_TITLE_H + 10, "Date & Time",
                0x00202020);
        rtc_read();
        char b[40];
        int o = 0;
        const char *p;
        p = "RTC: "; while (*p) b[o++] = *p++;
        char t[8];
        ga_itoa((int)rtc[5], t);
        for (int i = 0; t[i]; i++) b[o++] = t[i];
        b[o++] = '-';
        b[o++] = '0' + rtc[4] / 10;
        b[o++] = '0' + rtc[4] % 10;
        b[o++] = '-';
        b[o++] = '0' + rtc[3] / 10;
        b[o++] = '0' + rtc[3] % 10;
        b[o++] = ' ';
        b[o++] = '0' + rtc[2] / 10;
        b[o++] = '0' + rtc[2] % 10;
        b[o++] = ':';
        b[o++] = '0' + rtc[1] / 10;
        b[o++] = '0' + rtc[1] % 10;
        b[o] = 0;
        ga_str(&app, px, GA_TITLE_H + 40, b, 0x00202020);
        ga_str(&app, px, GA_TITLE_H + 56,
               "Adjust (root): +/- 1 minute", 0x00606060);
        ga_button(&app, px, GA_TITLE_H + 72, 40, 18, "+1m", 0);
        ga_button(&app, px + 48, GA_TITLE_H + 72, 40, 18, "-1m", 0);
    } else {
        ga_str2(&app, px, GA_TITLE_H + 10, "Account", 0x00202020);
        char b[32];
        int o = 0;
        const char *p = "uid: ";
        while (*p) b[o++] = *p++;
        char t[8];
        ga_itoa(m4k_getuid(), t);
        for (int i = 0; t[i]; i++) b[o++] = t[i];
        b[o] = 0;
        ga_str(&app, px, GA_TITLE_H + 40, b, 0x00202020);
        p = "name: ";
        o = 0;
        while (*p) b[o++] = *p++;
        for (int i = 0; account_name[i]; i++) b[o++] = account_name[i];
        b[o] = 0;
        ga_str(&app, px, GA_TITLE_H + 54, b, 0x00202020);
        ga_str(&app, px, GA_TITLE_H + 76,
               "n = rename account name", 0x00606060);
        ga_str(&app, px, GA_TITLE_H + 88,
               "(saved to /export/cfg/pref/)", 0x00A0A0A0);
    }
    (void)px; (void)pw;
}

/* ── click at window-local (lx,ly) ── */
static void click(int lx, int ly)
{
    /* tab column */
    if (lx < PF_TAB_W) {
        for (int i = 0; i < PF_TABS; i++) {
            int y = GA_TITLE_H + 6 + i * 26;
            if (ga_in(lx, ly, 2, y, PF_TAB_W - 4, 22)) {
                tab = i;
                return;
            }
        }
        return;
    }
    int px = PF_TAB_W + 14;
    int pw = PF_W - px - 10;

    if (tab == 0) {
        for (int i = 0; i < PF_NTHEMES; i++) {
            int sx = px + (i % 3) * ((pw - 20) / 3 + 6);
            int sy = GA_TITLE_H + 50 + (i / 3) * 62;
            int sw = (pw - 20) / 3;
            if (ga_in(lx, ly, sx, sy, sw, 50)) {
                wp_theme = i;
                apply_wallpaper();
                ser_puts("[PREF] WALLPAPER ");
                char v[2] = { (char)('0' + i), 0 };
                ser_puts(v);
                ser_puts("\n");
                return;
            }
        }
    } else if (tab == 1) {
        if (ga_in(lx, ly, px, GA_TITLE_H + 52, pw - 20, 22)) {
            int i = (lx - px) / ((pw - 20) / 8 + 2) + 1;
            if (i < 1) i = 1;
            if (i > 8) i = 8;
            mouse_speed = i;
            char v[2] = { (char)('0' + i), 0 };
            conf_write("mouse", v);
        }
        if (ga_in(lx, ly, px, GA_TITLE_H + 120, 70, 18)) {
            char v[2] = { (char)('0' + mouse_speed), 0 };
            conf_write("mouse", v);
        }
    } else if (tab == 2) {
        int plus = ga_in(lx, ly, px, GA_TITLE_H + 72, 40, 18);
        int minus = ga_in(lx, ly, px + 48, GA_TITLE_H + 72,
                          40, 18);
        if ((plus || minus) && m4k_getuid() == 0) {
            rtc_read();
            if (plus) {
                rtc[1] += 1;
                if (rtc[1] >= 60) { rtc[1] -= 60; rtc[2] += 1; }
            } else {
                if (rtc[1] == 0) rtc[1] = 59;
                else rtc[1] -= 1;
            }
            musr_sc_rtcwrite(rtc);
            ser_puts("[PREF] RTC adjusted\n");
        }
    }
}

void _start(void)
{
    ser_puts("[PREF] starting\n");
    app.buf = pf_buf;
    if (ga_init(&app) != 0) {
        ser_puts("[PREF] copland not ready\n");
        m4k_exit(1);
    }
    musr_sc_mkdir("/export/cfg");
    musr_sc_mkdir("/export/cfg/pref");
    load_all();
    ser_puts("[PREF] surface ready\n");

    render();
    ga_flip(&app);

    int rename_mode = 0;
    int rename_len = 0;

    for (;;) {
        if (ga_dead(&app))
            break;
        int ch;
        int dirty = 0;
        int pend = 0, cx = 0;
        while ((ch = ga_getkey(&app)) >= 0) {
            if (pend == 1) { cx = ch; pend = 2; continue; }
            if (pend == 2) {
                click(cx, ch);
                dirty = 1;
                pend = 0;
                continue;
            }
            if (ch == 0xF1 || ch == 0xF2) { pend = 1; continue; }
            if (ch == 'q' || ch == 27) {
                if (rename_mode) {
                    rename_mode = 0;
                    rename_len = 0;
                    dirty = 1;
                } else {
                    ser_puts("[PREF] exit\n");
                    m4k_exit(0);
                }
                continue;
            }
            if (rename_mode) {
                if (ch == '\n' || ch == '\r') {
                    if (rename_len > 0) {
                        for (int i = 0; i < rename_len; i++)
                            account_name[i] = input_tmp[i];
                        account_name[rename_len] = 0;
                        conf_write("account", account_name);
                    }
                    rename_mode = 0;
                    rename_len = 0;
                    dirty = 1;
                } else if (ch == 8) {
                    if (rename_len) rename_len--;
                    dirty = 1;
                } else if (ch >= 32 && ch < 127 && rename_len < 15) {
                    input_tmp[rename_len++] = (char)ch;
                    dirty = 1;
                }
                continue;
            }
            if (ch >= '1' && ch <= '4') {
                tab = ch - '1';
                dirty = 1;
            } else if (tab == 0 && ch >= '0' && ch <= '5') {
                wp_theme = ch - '0';
                apply_wallpaper();
                dirty = 1;
            } else if (tab == 1 && (ch == '-' || ch == '+')) {
                if (ch == '-' && mouse_speed > 1) mouse_speed--;
                if (ch == '+' && mouse_speed < 8) mouse_speed++;
                char v[2] = { (char)('0' + mouse_speed), 0 };
                conf_write("mouse", v);
                dirty = 1;
            } else if (tab == 3 && ch == 'n') {
                rename_mode = 1;
                rename_len = 0;
                dirty = 1;
            }
        }
        if (dirty) {
            render();
            if (rename_mode) {
                input_tmp[rename_len] = 0;
                ga_str(&app, PF_TAB_W + 14, PF_H - 20,
                       input_tmp, 0x00202020);
                ga_char(&app, PF_TAB_W + 14 + rename_len * 6,
                        PF_H - 20, '_', 0x00C03030);
            }
            ga_flip(&app);
        }
        m4k_sleep(120);
    }
    m4k_exit(0);
}
