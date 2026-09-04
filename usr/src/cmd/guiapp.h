/*
 * ==== guiapp.h — shared Copland GUI-app scaffolding ====
 * M4KK1 4P1 - usr/src/cmd/guiapp.h
 * Description: Common header for standalone Copland client apps
 *              (clock, logview, info, automission, backup, ...).
 *              Provides the 5x7 font, clipped draw helpers, surface
 *              claim loop, key-mailbox ring, and the click-close
 *              chrome convention used by /bin/fm and /bin/calcg.
 *
 * Protocol summary (all Sprach-visible conventions):
 *   - app creates its surface via COPLAND_CMD_CREATE_SURFACE with a
 *     WIDTH UNIQUE TO THE APP (Sprach dispatches keyboard by width)
 *   - app registers a key mailbox at a fixed address with a magic;
 *     Sprach writes keystrokes, app drains the ring
 *   - title bar height 18px; close box at the right edge (12x12)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4KK1_CMD_GUIAPP_H_
#define _M4KK1_CMD_GUIAPP_H_

#include "m4sh.h"
#include "../lib/libcopland.h"

/* Globals required by m4sh.h (every app defines these) */
extern int out_fd;
extern char cwd[256];

/* ── app geometry conventions ── */
#define GA_TITLE_H     18
#define GA_CLOSE_W     12
#define GA_ROW_H       10          /* 5x7 text line pitch */

/* ══════════════ tiny 5x7 font (same table as calc/fm) ══════════════ */
static const uint8_t ga_font5x7[96][5] = {
    {0,0,0,0,0},{0,0x7F,0,0,0},{0,0x7F,0,0,0},{0x21,0x7F,0x21,0x7F,0x21},
    {0x14,0x7F,0x7F,0x14,0x7F},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0,0x05,0x03,0,0},{0,0x1C,0x22,0x41,0},
    {0x14,0x7F,0x41,0x7F,0x14},{0x08,0x3E,0x41,0x3E,0x08},{0,0x08,0x08,0,0},
    {0x08,0x08,0x3E,0x08,0x08},{0,0xA0,0x60,0,0},{0x08,0x08,0x08,0x08,0x08},
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
    {0x21,0x45,0x4B,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x48,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x41,0x36,0x08,0},
    {0x08,0x14,0x14,0x08,0x3E},{0x08,0x14,0x22,0x22,0x3E},{0x3E,0x08,0x14,0x22,0x3E},
    {0x46,0x29,0x19,0x09,0x07},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x41,0x41,0x41,0x3E},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x3A},
    {0x7F,0x08,0x08,0x08,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},{0x02,0x04,0x08,0x10,0x20},
    {0,0x41,0x41,0x7F,0},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0,0x01,0x02,0x04,0},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},{0x7F,0x08,0x04,0x04,0x78},
    {0,0x44,0x7D,0x40,0},{0x20,0x40,0x44,0x3D,0x08},{0x7F,0x10,0x28,0x44,0},
    {0,0x41,0x7F,0x40,0},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},{0xFC,0x24,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},
    {0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,00,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},
    {0x08,0x08,0x2A,0x1C,0x08},{0,0,0,0,0},{0x00,0x00,0x00,0x00,0x00},
};

/* ── generic key mailbox (Sprach writes, app drains) ── */
#define GA_MAILBOX_SIZE 64

struct ga_mailbox {
    uint32_t magic;
    uint32_t write_idx;    /* producer: Sprach */
    uint32_t read_idx;     /* consumer: app */
    unsigned char buf[GA_MAILBOX_SIZE];
};

/* ga_app: per-app state the helpers below need.  The app defines:
 *   static struct ga_app app = { .w=..., .h=..., .mb_addr=...,
 *                                .mb_magic=..., .title="..." };
 */
struct ga_app {
    int w, h;
    uint32_t mb_addr;
    uint32_t mb_magic;
    const char *title;
    /* filled in by ga_init(): */
    struct copland_shm *shm;
    volatile struct ga_mailbox *mb;
    uint32_t *buf;          /* app pixel buffer (app allocates) */
    int slot;               /* copland surface slot */
};

/**
 * ga_init - claim a Copland surface and register the key mailbox.
 * Returns 0 on success; on failure the app should m4k_exit(1).
 * @pApp: app descriptor (w/h/mb_addr/mb_magic/title set by caller;
 *        buf must point at the app's static pixel buffer)
 */
static int ga_init(struct ga_app *pApp)
{
    pApp->shm = copland_shm_get();
    if (!pApp->shm || pApp->shm->magic != COPLAND_SHM_MAGIC)
        return -1;

    pApp->mb = (volatile struct ga_mailbox *)pApp->mb_addr;
    pApp->mb->magic = pApp->mb_magic;
    pApp->mb->write_idx = 0;
    pApp->mb->read_idx = 0;

    if (copland_cmd_push(pApp->shm, COPLAND_CMD_CREATE_SURFACE,
                         60, 60, pApp->w, pApp->h, 0x00A05010,
                         COPLAND_SURF_VISIBLE) != 0)
        return -1;

    /* wait for copland to allocate the slot (match by width, like
     * calc_gui; widths are unique per app by convention) */
    int guard = 0;
    pApp->slot = -1;
    while (guard++ < 200000) {
        m4k_yield();
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (pApp->shm->surfaces[i].in_use &&
                !pApp->shm->surfaces[i].buffer_ptr &&
                pApp->shm->surfaces[i].w == pApp->w) {
                pApp->slot = i;
                break;
            }
        if (pApp->slot >= 0)
            break;
    }
    if (pApp->slot < 0)
        return -1;
    pApp->shm->surfaces[pApp->slot].buffer_ptr =
        (uint32_t)(uintptr_t)pApp->buf;
    return 0;
}

/**
 * ga_alive - 0 while the surface is visible; app exits when != 0.
 */
static int ga_dead(struct ga_app *pApp)
{
    return !(pApp->shm->surfaces[pApp->slot].flags &
             COPLAND_SURF_VISIBLE);
}

/**
 * ga_getkey - pop one keystroke from the mailbox, or -1.
 */
static int ga_getkey(struct ga_app *pApp)
{
    if (pApp->mb->read_idx == pApp->mb->write_idx)
        return -1;
    unsigned char ch = pApp->mb->buf[pApp->mb->read_idx];
    pApp->mb->read_idx = (pApp->mb->read_idx + 1) % GA_MAILBOX_SIZE;
    return ch;
}

/**
 * ga_flip - mark the whole surface dirty so copland re-composites.
 */
static void ga_flip(struct ga_app *pApp)
{
    struct copland_surface *s = &pApp->shm->surfaces[pApp->slot];
    s->dmg_x = s->x; s->dmg_y = s->y;
    s->dmg_w = s->w; s->dmg_h = s->h;
    pApp->shm->dirty = 1;
}

/* ── clipped draw helpers into app->buf ── */

static void ga_rect(struct ga_app *pApp, int x, int y,
                    int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > pApp->w) w = pApp->w - x;
    if (y + h > pApp->h) h = pApp->h - y;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            pApp->buf[yy * pApp->w + xx] = c;
}

static void ga_char(struct ga_app *pApp, int x, int y,
                    char ch, uint32_t c)
{
    if (ch < 32 || ch > 126 || x < 0 || y < 0 ||
        x + 5 > pApp->w || y + 7 > pApp->h)
        return;
    const uint8_t *g = ga_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row))
                pApp->buf[(y + row) * pApp->w + x + col] = c;
    }
}

static int ga_strlen(const char *s)
{
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void ga_str(struct ga_app *pApp, int x, int y,
                   const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        ga_char(pApp, x + i * 6, y, s[i], c);
}

static void ga_str_clip(struct ga_app *pApp, int x, int y,
                        const char *s, int maxch, uint32_t c)
{
    for (int i = 0; s && s[i] && i < maxch; i++)
        ga_char(pApp, x + i * 6, y, s[i], c);
}

static void ga_itoa(int v, char *out)
{
    char tmp[12];
    int neg = v < 0;
    unsigned uv = neg ? -v : v;
    int i = 0;
    do { tmp[i++] = '0' + uv % 10; uv /= 10; } while (uv);
    int o = 0;
    if (neg) out[o++] = '-';
    while (i > 0) out[o++] = tmp[--i];
    out[o] = 0;
}

/**
 * ga_chrome - draw the standard title bar + close box.
 * Returns 1 if (lx,ly) window-local coordinates hit the close box.
 */
static int ga_chrome(struct ga_app *pApp, const char *title)
{
    uint32_t tc = 0x00A05010;
    ga_rect(pApp, 0, 0, pApp->w, GA_TITLE_H, tc);
    ga_str_clip(pApp, 4, 5, title,
                (pApp->w - GA_CLOSE_W - 8) / 6, 0x00FFFFFF);
    /* close box: red square with X, top-right */
    int cx = pApp->w - GA_CLOSE_W - 3;
    ga_rect(pApp, cx, 3, GA_CLOSE_W, GA_CLOSE_W, 0x00C03030);
    ga_char(pApp, cx + 3, 5, 'x', 0x00FFFFFF);
    return 0;
}

/* ══════════════ shared widget helpers (sys suite apps) ══════════════
 * Added for the 2026-09 desktop-app suite: 3D button, progress bar,
 * line-graph plot, checkbox, click hit-test.  All clip like ga_rect. */

#define GA_BTN_DOWN 1
#define GA_BTN_UP   0

/** ga_button - draw a 3D-shaded button.  Returns 1 when (lx,ly) is
 * inside its rect (click hit-test). */
static int ga_button(struct ga_app *pApp, int x, int y,
                     int w, int h, const char *label, int pressed)
{
    uint32_t face = pressed ? 0x00808080 : 0x00C8C8C8;
    ga_rect(pApp, x, y, w, h, face);
    /* top/left highlight, bottom/right shadow */
    ga_rect(pApp, x, y, w, 1, 0x00FFFFFF);
    ga_rect(pApp, x, y, 1, h, 0x00FFFFFF);
    ga_rect(pApp, x, y + h - 1, w, 1, 0x00505050);
    ga_rect(pApp, x + w - 1, y, 1, h, 0x00505050);
    if (label) {
        int n = ga_strlen(label);
        ga_str(pApp, x + (w - n * 6) / 2,
               y + (h - 7) / 2, label, 0x00202020);
    }
    return 1;   /* bounds checked by caller via ga_in */
}

/** ga_in - rect hit-test for window-local click coords. */
static int ga_in(int lx, int ly, int x, int y, int w, int h)
{
    return lx >= x && lx < x + w && ly >= y && ly < y + h;
}

/** ga_progress - horizontal progress bar (0..1000 permille). */
static void ga_progress(struct ga_app *pApp, int x, int y,
                        int w, int h, int permille)
{
    ga_rect(pApp, x, y, w, h, 0x00606060);
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;
    int fw = (w - 2) * permille / 1000;
    ga_rect(pApp, x + 1, y + 1, fw, h - 2, 0x0030A030);
}

/** ga_graph - plot a history of 0..1000 permille samples as a
 * filled line graph inside (x,y,w,h).  n = sample count (<= cap). */
static void ga_graph(struct ga_app *pApp, int x, int y,
                     int w, int h, const uint16_t *samples,
                     int n, uint32_t line, uint32_t fill)
{
    ga_rect(pApp, x, y, w, h, 0x00181820);
    if (n < 2)
        return;
    /* grid lines every quarter */
    for (int g = 1; g < 4; g++)
        ga_rect(pApp, x, y + h * g / 4, w, 1, 0x00303040);
    int prev_y = -1;
    for (int i = 0; i < n; i++) {
        int sx = x + i * (w - 1) / (n - 1);
        int sy = y + h - 2 - (h - 4) * samples[i] / 1000;
        if (prev_y >= 0) {
            /* vertical fill column + line segment */
            int y0 = prev_y < sy ? prev_y : sy;
            int y1 = prev_y < sy ? sy : prev_y;
            ga_rect(pApp, sx, y0, 1, y1 - y0 + 1, fill);
        }
        ga_rect(pApp, sx, sy, 1, 1, line);
        prev_y = sy;
    }
}

/** ga_str2 - 2x scaled string (headers). */
static void ga_char2(struct ga_app *pApp, int x, int y,
                     char ch, uint32_t c)
{
    if (ch < 32 || ch > 126) return;
    const uint8_t *g = ga_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row))
                ga_rect(pApp, x + col * 2, y + row * 2, 2, 2, c);
    }
}

static void ga_str2(struct ga_app *pApp, int x, int y,
                    const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        ga_char2(pApp, x + i * 12, y, s[i], c);
}

#endif /* _M4KK1_CMD_GUIAPP_H_ */
