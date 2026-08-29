/*
 * M4KK1 4P1 - sprach.c
 * Description: Sprach window manager - core (mode-independent)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * REFACTOR (2026-08-05):
 *   - Conditional compositing: m4k_flip() only on structural changes
 *     (window create/destroy, move, resize, Z-order change).
 *   - Top menubar (24 px, Mac OS style) with system name + clock.
 *   - Bottom taskbar (30 px) with window buttons.
 *   - Work area: y = MENUBAR_H .. SCREEN_H - TASKBAR_H.
 *   - Arrow cursor bitmap defined for future software-cursor use.
 *   - m4k_yield() + spin-wait for cooperative frame pacing.
 *
 * REFACTOR (2026-08-12) — Desktop redraft (bar + dock):
 *   - Bottom dock is 48 px (Mac OS style); 1 px bright top edge.
 *   - Dock shows one button per live window plus a "Terminal" launch
 *     button at the far right (spawns /bin/terminal, or focuses it).
 *   - Top menubar draws real "M4KK1" text and a HH:MM:SS clock that
 *     repaints every second (previously HH:MM on a 60-tick cadence).
 *   - Frame pacing via m4k_sleep(20) (~50 FPS) instead of spin-wait.
 *
 * BUGFIX (2026-08-12) — mouse Y axis, dock focus, dock clock:
 *   - Mouse Y: PS/2 dy > 0 = DOWN (toward the operator), screen Y grows
 *     down, so the accumulator ADDS (y += dy * 2).  The earlier strict
 *     inversion mirrored hit-test Y against the visible cursor.  Every
 *     move event is logged (x, y, dx, dy) on serial.
 *   - Dock buttons: active-window caching no longer pins "-2" while a
 *     terminal runs, so button clicks switch focus + repaint the dock;
 *     the click path also invalidates the highlight cache directly.
 *   - Dock clock: right-aligned HH:MM, repainted only on minute change.
 */

#include "../lib/libgui.h"
#include "../lib/icons.h"
#include "../lib/icons_data.h"
#include "../lib/musr_inline.h"
#include "../../../sys/src/copland/copland_proto.h"
#include "sprach.h"

/* Globals required by the m4sh.h ABI */
int out_fd = 1;
char cwd[256] = "/";

/* ── Zig hot-path pixel primitives (docs/zig-standard.md) ──
 * Build with -DUSE_ZIG_DRAW routes sp_fill/sp_rect/sp_draw_char/
 * sp_draw_str/sp_draw_str_bold to the Zig exports in spr_draw.zig
 * (byte-equivalent, verified by tools/build/zig_equivalence_test.py).
 * Without the define the pure-C originals below stay in use. */
#ifdef USE_ZIG_DRAW
extern void zsp_fill(uint32_t *buf, int n, uint32_t c);
extern void zsp_rect(uint32_t *buf, int bw, int bh, int x, int y,
                     int rw, int rh, uint32_t c);
extern void zsp_px(uint32_t *buf, int bw, int bh, int x, int y,
                   uint32_t c);
extern void zsp_draw_char(uint32_t *buf, int bw, int bh, int x, int y,
                          unsigned char ch, uint32_t fg);
extern void zsp_draw_str(uint32_t *buf, int bw, int bh, int x, int y,
                         const char *s, uint32_t fg);
extern void zsp_draw_str_bold(uint32_t *buf, int bw, int bh, int x, int y,
                              const char *s, uint32_t fg);
extern void zsp_gradient(uint32_t *buf, int bw, int bh, uint32_t top,
                         uint32_t bot, int gbase, int gmax);
extern void zsp_icon_blit32(uint32_t *buf, int bw, int x, int y,
                            const uint32_t *icon);
extern void zsp_icon_blit(uint32_t *buf, int bw, int bh, int x, int y,
                          const uint32_t *icon);
extern void zsp_draw_circle(uint32_t *buf, int bw, int bh, int cx, int cy,
                            int r, uint32_t c);
#endif

/* ── Copland new-protocol client state (phase 3: WM identity) ──
 *
 * Sprach connects through the same connection table as cptest, but
 * binds copland_compositor FIRST and thereby claims the WM privilege
 * (spec §15).  Its chrome surfaces (menubar, taskbar) live on the
 * new protocol; app windows stay on the legacy shm cmd ring until
 * they are migrated. */
static struct cp_conn sp_conn;
static struct cp_objmap sp_map;
static uint8_t sp_reqbuf[CP_MSG_MAX];
static struct cp_msgbuf sp_mb;

static uint32_t sp_id_comp;       /* compositor object id */
static uint32_t sp_id_shm;        /* shm object id */
static uint32_t sp_id_pool;       /* pool covering all chrome buffers */
static uint32_t sp_id_mb_surf;    /* menubar surface */
static uint32_t sp_id_tb_surf;    /* taskbar surface */
static uint32_t sp_id_mb_buf;     /* menubar buffer */
static uint32_t sp_id_tb_buf;     /* taskbar buffer */
static uint8_t  sp_proto_ready;   /* handshake done */

static void sp_send(uint32_t obj, uint32_t op)
{
    struct cp_ring r;

    if (cp_msg_finish(&sp_mb) < 0)
        return;
    cp_ring_init(&r, &sp_conn.req_head, &sp_conn.req_tail,
                 sp_conn.req_data, sp_conn.req_size);
    cp_ring_write(&r, sp_reqbuf, sp_mb.len);
}

static void sp_req(uint32_t obj, uint32_t op)
{
    cp_msg_start(&sp_mb, sp_reqbuf, sizeof(sp_reqbuf), obj, op);
}

static int sp_evt_pop(uint32_t *obj, uint32_t *op, uint32_t *a0)
{
    uint8_t buf[CP_MSG_MAX];
    struct cp_ring r;
    int rc;

    cp_ring_init(&r, &sp_conn.evt_head, &sp_conn.evt_tail,
                 sp_conn.evt_data, sp_conn.evt_size);
    rc = cp_ring_read(&r, buf, sizeof(buf));
    if (rc <= 0)
        return rc;
    *obj = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8)
        | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    *op = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8);
    if (a0) {
        struct cp_msg_reader rd;
        cp_reader_init(&rd, buf, (uint32_t)rc);
        *a0 = cp_get_u32(&rd);
    }
    return rc;
}

static int sp_wait_evt(uint32_t obj, uint32_t op, int spins)
{
    uint32_t eo, ep, ea;
    int s;

    for (s = 0; s < spins; s++) {
        while (sp_evt_pop(&eo, &ep, &ea) > 0) {
            if (eo == obj && ep == op)
                return 1;
        }
        m4k_yield();
    }
    return 0;
}

/* Click handler defined below sprach_handle_mouse; forward-declared so
 * the whole file compiles cleanly under -Werror=implicit-function-
 * declaration (GCC >= 14 treats implicit declarations as errors). */
static void sprach_handle_click(struct sprach_ctx *ctx);

/* ── Pixel buffers (BSS, aligned for Copland blit) ── */

static uint32_t sprach_bufs[SPRACH_WINDOW_COUNT]
                          [SPRACH_WIN_W * SPRACH_WIN_H]
    __attribute__((aligned(16)));

/* Per-window maximize backing stores.  A single shared buffer let two
 * maximized windows alias each other's pixels — the second MAX
 * silently repainted the first window's content. */
static uint32_t maximize_bufs[SPRACH_WINDOW_COUNT]
                             [SCREEN_W * SCREEN_H]
    __attribute__((aligned(16)));

static uint32_t taskbar_buf[SCREEN_W * TASKBAR_H]
    __attribute__((aligned(16)));
/* menubar_buf stays static; its address is passed directly. */
#define SP_TASKBAR_BUF taskbar_buf

static uint32_t menubar_buf[SCREEN_W * MENUBAR_H]
    __attribute__((aligned(16)));

static uint32_t maximize_buf[SCREEN_W * SCREEN_H]
    __attribute__((aligned(16)));

/* ── 16×16 Arrow cursor bitmap (BGRA) ──
 *
 * Each pixel: 0x00FFFFFF = white arrow, 0x00000000 = background.
 * Used when/if software cursor rendering is needed (currently the
 * kernel handles cursor drawing via m4k_update_cursor). */

static const uint32_t cursor_arrow[16 * 16] = {
    0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* ── 5×7 bitmap font, full printable ASCII (0x20..0x7E) ── */
/* Each row: bits 4..0 = 5 horizontal pixels (bit4 = leftmost). */

static const unsigned char font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* ! */
    {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00}, /* " */
    {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}, /* # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /* $ */
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, /* % */
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, /* & */
    {0x0C,0x04,0x08,0x00,0x00,0x00,0x00}, /* ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /* ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, /* * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}, /* , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10}, /* / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}, /* 9 */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, /* : */
    {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}, /* ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /* < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /* > */
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* ? */
    {0x0E,0x11,0x01,0x0D,0x15,0x15,0x0E}, /* @ */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, /* [ */
    {0x10,0x08,0x08,0x04,0x02,0x02,0x01}, /* \ */
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, /* ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, /* _ */
    {0x08,0x04,0x02,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, /* a */
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E}, /* b */
    {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E}, /* c */
    {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F}, /* d */
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, /* e */
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08}, /* f */
    {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E}, /* g */
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x11}, /* h */
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, /* i */
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, /* j */
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, /* k */
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, /* l */
    {0x00,0x00,0x1A,0x15,0x15,0x15,0x15}, /* m */
    {0x00,0x00,0x1E,0x11,0x11,0x11,0x11}, /* n */
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, /* o */
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, /* p */
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01}, /* q */
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, /* r */
    {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E}, /* s */
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06}, /* t */
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, /* u */
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, /* v */
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, /* w */
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, /* x */
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, /* y */
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, /* z */
    {0x06,0x08,0x08,0x10,0x08,0x08,0x06}, /* { */
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, /* | */
    {0x0C,0x02,0x02,0x01,0x02,0x02,0x0C}, /* } */
    {0x08,0x15,0x02,0x00,0x00,0x00,0x00}, /* ~ */
};

/* ── Generic pixel helpers (into arbitrary buffer) ── */

static void sp_px(uint32_t *buf, int bw, int bh, int x, int y, uint32_t c)
{
#ifdef USE_ZIG_DRAW
    zsp_px(buf, bw, bh, x, y, c);
#else
    if (x >= 0 && x < bw && y >= 0 && y < bh)
        buf[y * bw + x] = c;
#endif
}

static void sp_fill(uint32_t *buf, int n, uint32_t c)
{
#ifdef USE_ZIG_DRAW
    zsp_fill(buf, n, c);
#else
    musr_fill32(buf, (size_t)n, c);
#endif
}

static void sp_rect(uint32_t *buf, int bw, int bh, int x, int y,
                    int rw, int rh, uint32_t c)
{
#ifdef USE_ZIG_DRAW
    zsp_rect(buf, bw, bh, x, y, rw, rh, c);
#else
    /* Per-row dword fill with full clipping: (bw,bh) is the buffer's
     * true geometry — never SCREEN_W/SCREEN_H, which are only correct
     * for full-screen buffers (taskbar/menubar pass bigger bh on
     * purpose; popups pass smaller). */
    int x0 = x > 0 ? x : 0;
    int y0 = y > 0 ? y : 0;
    int x1 = x + rw;
    int y1 = y + rh;
    if (x1 > bw)
        x1 = bw;
    if (y1 > bh)
        y1 = bh;
    for (int py = y0; py < y1; py++)
        musr_fill32(buf + (size_t)py * bw + x0,
                    (size_t)(x1 - x0), c);
#endif
}

/* ── Window-buffer helpers (type-safe) ── */

static void sp_buf_px(struct sprach_window *w, int x, int y, uint32_t c)
{
    if (x >= 0 && x < w->w && y >= 0 && y < w->h)
        w->buf[y * w->w + x] = c;
}

static void sp_buf_fill(struct sprach_window *w, uint32_t c)
{
    /* rep stosd bulk fill instead of a per-pixel C loop */
    musr_fill32(w->buf, (size_t)(w->w * w->h), c);
}

static void sp_buf_rect(struct sprach_window *w, int x, int y,
                        int rw, int rh, uint32_t c)
{
    /* Per-row bulk fill with clipping — sp_buf_px did a bounds check
     * per pixel (35 ops per pixel where 1 dword store suffices). */
    int x0 = x > 0 ? x : 0;
    int y0 = y > 0 ? y : 0;
    int x1 = x + rw;
    int y1 = y + rh;
    if (x1 > w->w)
        x1 = w->w;
    if (y1 > w->h)
        y1 = w->h;
    for (int yy = y0; yy < y1; yy++)
        musr_fill32(w->buf + (size_t)yy * w->w + x0,
                    (size_t)(x1 - x0), c);
}

/* ── Draw one 5×7 font character into a pixel buffer ── */

static void sp_draw_char(uint32_t *buf, int bw, int bh, int x, int y,
                         char ch, uint32_t fg)
{
#ifdef USE_ZIG_DRAW
    zsp_draw_char(buf, bw, bh, x, y, (unsigned char)ch, fg);
#else
    int idx;
    if (ch >= 0x20 && ch <= 0x7E)
        idx = ch - 0x20;
    else
        return;

    for (int row = 0; row < 7; row++) {
        unsigned char bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col)))
                sp_px(buf, bw, bh, x + col, y + row, fg);
        }
    }
#endif
}

/* Draw a NUL-terminated string, 6px advance per char (5 + 1 gap). */
static void sp_draw_str(uint32_t *buf, int bw, int bh, int x, int y,
                        const char *s, uint32_t fg)
{
#ifdef USE_ZIG_DRAW
    zsp_draw_str(buf, bw, bh, x, y, s, fg);
#else
    if (!s)
        return;
    while (*s) {
        sp_draw_char(buf, bw, bh, x, y, *s, fg);
        x += 6;
        s++;
    }
#endif
}

/* Draw bold by double-striking: 1px right + 1px down offset. */
static void sp_draw_char_bold(uint32_t *buf, int bw, int bh, int x, int y,
                              char ch, uint32_t fg)
{
    sp_draw_char(buf, bw, bh, x, y, ch, fg);
    sp_draw_char(buf, bw, bh, x + 1, y, ch, fg);
    sp_draw_char(buf, bw, bh, x, y + 1, ch, fg);
}

static void sp_draw_str_bold(uint32_t *buf, int bw, int bh, int x, int y,
                             const char *s, uint32_t fg)
{
#ifdef USE_ZIG_DRAW
    zsp_draw_str_bold(buf, bw, bh, x, y, s, fg);
#else
    if (!s)
        return;
    while (*s) {
        sp_draw_char_bold(buf, bw, bh, x, y, *s, fg);
        x += 7;
        s++;
    }
#endif
}

/* ── Dock icons (32x32, pixel-drawn) ── */

/* Blit a real 32x32 RGBA bitmap (from icons_data.c) with transparency;
 * falls back to nothing when the named asset is missing.  The lookup
 * result is cached per name (taskbar repaints hit the same handful of
 * names every second — the old per-call linear scan walked the whole
 * table for every icon on every repaint). */
static uint32_t *sp_icon_lookup(const char *name)
{
    /* tiny 8-entry direct-mapped cache: key = name pointer identity +
     * strcmp fallback (callers pass static literals, so pointer hits
     * are the common case and strcmp only validates first miss). */
    static const char *nk[8];
    static uint32_t *nv[8];
    static uint8_t init;
    if (!init) {
        for (int i = 0; i < 8; i++)
            nv[i] = (uint32_t *)0;
        init = 1;
    }
    uint8_t h = (uint8_t)(((uintptr_t)name >> 4) & 7);
    if (nk[h] == name && name)
        return nv[h];
    uint32_t *ic = icons_data_find(name);
    nk[h] = name;
    nv[h] = ic;
    return ic;
}

static void sp_icon_blit32(uint32_t *buf, int bw, int bh, int x, int y,
                           const char *name)
{
    uint32_t *ic = sp_icon_lookup(name);
    if (!ic)
        return;
#ifdef USE_ZIG_DRAW
    zsp_icon_blit(buf, bw, bh, x, y, ic);
#else
    for (int yy = 0; yy < 32; yy++)
        for (int xx = 0; xx < 32; xx++) {
            uint32_t px = ic[yy * 32 + xx];
            if (!(px >> 24))
                continue;
            if (x + xx < 0 || x + xx >= bw || y + yy < 0 || y + yy >= bh)
                continue;
            buf[(y + yy) * bw + x + xx] = px;
        }
#endif
}

/* Terminal icon: dark screen with a ">_" prompt, Mac-ish rounded top. */
static void sp_icon_terminal(uint32_t *buf, int bw, int bh, int x, int y,
                             uint32_t body_col)
{
    int s = DOCK_ICON_SIZE;
    /* Frame */
    sp_rect(buf, bw, bh, x, y, s, s, 0x00202020);
    sp_rect(buf, bw, bh, x + 1, y + 1, s - 2, s - 2, body_col);
    /* Screen inset */
    sp_rect(buf, bw, bh, x + 4, y + 8, s - 8, s - 14, 0x00080818);
    /* Prompt glyph "> " + "_" */
    sp_draw_char(buf, bw, bh, x + 6, y + 12, '>', 0x0030E070);
    sp_draw_char(buf, bw, bh, x + 14, y + 12, '_', 0x00E0E0E0);
    /* Title-bar dots row */
    sp_rect(buf, bw, bh, x + 4, y + 3, 4, 3, 0x00E05050);
    sp_rect(buf, bw, bh, x + 10, y + 3, 4, 3, 0x00E0C040);
    sp_rect(buf, bw, bh, x + 16, y + 3, 4, 3, 0x0040C040);
}

/* Generic app-window icon: colored title bar over a doc body. */
static void sp_icon_window(uint32_t *buf, int bw, int bh, int x, int y,
                           uint32_t title_col)
{
    int s = DOCK_ICON_SIZE;
    sp_rect(buf, bw, bh, x, y, s, s, 0x00202020);
    sp_rect(buf, bw, bh, x + 1, y + 1, s - 2, s - 2, 0x00E8E8E8);
    sp_rect(buf, bw, bh, x + 3, y + 3, s - 6, 8, title_col);
    /* fake text lines */
    for (int l = 0; l < 3; l++)
        sp_rect(buf, bw, bh, x + 5, y + 16 + l * 4,
                s - 10 - l * 4, 2, 0x00808080);
}

/* Clock icon for the menubar popup: simple analog face. */
static void sp_icon_clock(uint32_t *buf, int bw, int bh, int x, int y,
                          uint32_t face_col)
{
    int s = DOCK_ICON_SIZE;
    sp_rect(buf, bw, bh, x, y, s, s, 0x00202020);
    sp_rect(buf, bw, bh, x + 1, y + 1, s - 2, s - 2, face_col);
    /* hands at 10:09-ish */
    sp_rect(buf, bw, bh, x + 15, y + 8, 2, 9, 0x00101010);
    sp_rect(buf, bw, bh, x + 17, y + 15, 8, 2, 0x00101010);
    sp_rect(buf, bw, bh, x + 14, y + 14, 4, 4, 0x00101010);
}

/* ── Wait helper: poll keyboard until Copland creates a surface ── */

/* System tray glyphs: 16x16 hand-drawn shapes for the menubar.
 * volume: speaker + arcs; network: connected dots (up) or an X
 * (down); bluetooth: the rune, dim when off. */
#define TRAY_W     56           /* 3 glyphs * 16px + 2 * 8px spacing */
#define TRAY_ICON  16

static void sp_icon_volume(uint32_t *buf, int bw, int bh, int x, int y,
                           uint32_t col)
{
    /* speaker box */
    sp_rect(buf, bw, bh, x + 2, y + 6, 4, 5, col);
    sp_rect(buf, bw, bh, x, y + 7, 2, 3, col);
    /* sound arcs */
    sp_rect(buf, bw, bh, x + 8, y + 4, 1, 1, col);
    sp_rect(buf, bw, bh, x + 9, y + 5, 1, 2, col);
    sp_rect(buf, bw, bh, x + 9, y + 9, 1, 2, col);
    sp_rect(buf, bw, bh, x + 8, y + 11, 1, 1, col);
    sp_rect(buf, bw, bh, x + 11, y + 2, 1, 2, col);
    sp_rect(buf, bw, bh, x + 12, y + 4, 1, 8, col);
    sp_rect(buf, bw, bh, x + 11, y + 12, 1, 2, col);
}

static void sp_icon_network(uint32_t *buf, int bw, int bh, int x, int y,
                            uint32_t col, int up)
{
    if (up) {
        /* three ascending link bars */
        sp_rect(buf, bw, bh, x + 1, y + 9, 3, 5, col);
        sp_rect(buf, bw, bh, x + 6, y + 5, 3, 9, col);
        sp_rect(buf, bw, bh, x + 11, y + 1, 3, 13, col);
    } else {
        /* crossed-out bars: dim base + X */
        uint32_t dim = 0x00A0A0A0;
        sp_rect(buf, bw, bh, x + 1, y + 9, 3, 5, dim);
        sp_rect(buf, bw, bh, x + 6, y + 5, 3, 9, dim);
        sp_rect(buf, bw, bh, x + 11, y + 1, 3, 13, dim);
        for (int i = 0; i < 12; i++) {
            sp_rect(buf, bw, bh, x + 2 + i, y + 2 + i, 1, 1, col);
            sp_rect(buf, bw, bh, x + 13 - i, y + 2 + i, 1, 1, col);
        }
    }
}

static void sp_icon_bluetooth(uint32_t *buf, int bw, int bh, int x, int y,
                              uint32_t col, int on)
{
    uint32_t c = on ? col : 0x00A0A0A0;
    /* the bluetooth rune: vertical stem + diagonals */
    sp_rect(buf, bw, bh, x + 7, y + 1, 2, 14, c);
    for (int i = 0; i < 5; i++) {
        sp_rect(buf, bw, bh, x + 9 + i, y + 2 + i, 1, 1, c);  /* / hi */
        sp_rect(buf, bw, bh, x + 9 + i, y + 13 - i, 1, 1, c); /* \ lo */
        sp_rect(buf, bw, bh, x + 6 - i, y + 3 + i, 1, 1, c);  /* \ hi */
        sp_rect(buf, bw, bh, x + 6 - i, y + 12 - i, 1, 1, c); /* / lo */
    }
    if (!on) {
        /* small "off" slash across */
        for (int i = 0; i < 10; i++)
            sp_rect(buf, bw, bh, x + 3 + i, y + 3 + i, 1, 1, col);
    }
}

static int sprach_wait_slot(struct sprach_ctx *ctx, int before)
{
    int guard = 0;
    for (;;) {
        struct m4k_keyboard_event ev;
        m4k_get_keyboard_event(&ev);          /* cooperatively yields */
        if (++guard > 200000)
            return -1;
        int after = 0;
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (ctx->shm->surfaces[i].in_use)
                after++;
        if (after > before)
            return 0;
    }
}

/* ── Window creation ── */

int sprach_create_window(struct sprach_ctx *ctx, int idx, int x, int y,
                         uint32_t title, uint32_t body)
{
    struct sprach_window *w = &ctx->wins[idx];

    w->slot = -1;
    w->x = x;
    w->y = y;
    w->w = SPRACH_WIN_W;
    w->h = SPRACH_WIN_H;
    w->normal_x = x;
    w->normal_y = y;
    w->normal_w = SPRACH_WIN_W;
    w->normal_h = SPRACH_WIN_H;
    w->title = title;
    w->body = body;
    w->buf = sprach_bufs[idx];
    w->hidden = 0;
    w->maximized = 0;
    w->btn_clicked = 0;
    w->click_tick = 0;

    /* Short title shown in the taskbar button */
    static const char *win_titles[SPRACH_WINDOW_COUNT] = {
        "Win 1", "Win 2", "Win 3"
    };
    w->title_str = (idx >= 0 && idx < SPRACH_WINDOW_COUNT)
                       ? win_titles[idx] : "Window";

    int before = 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            before++;

    if (copland_cmd_push(ctx->shm, COPLAND_CMD_CREATE_SURFACE,
                         x, y, w->w, w->h, (int32_t)body,
                         COPLAND_SURF_VISIBLE) != 0)
        return -1;

    if (sprach_wait_slot(ctx, before) != 0)
        return -1;

    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (ctx->shm->surfaces[i].in_use &&
            !ctx->shm->surfaces[i].buffer_ptr) {
            w->slot = i;
            ctx->shm->surfaces[i].buffer_ptr = (uint32_t)(uintptr_t)w->buf;
            ctx->shm->dirty = 1;
            return 0;
        }
    }
    return -1;
}

/* ── Window painting (into pixel buffer, NOT to framebuffer) ── */

static void sp_draw_circle(struct sprach_window *w, int cx, int cy,
                            int r, uint32_t c)
{
#ifdef USE_ZIG_DRAW
    zsp_draw_circle(w->buf, w->w, w->h, cx, cy, r, c);
#else
    /* Per-row span fill: the old loop pushed every pixel through
     * sp_buf_px's bounds check (r^2 calls for one button).  Here
     * each scanline costs one clip check plus one contiguous fill;
     * the half-width is derived by walking dx while dx^2 fits the
     * remaining radius (buttons are small — this stays cheaper
     * than linking a sqrt). */
    for (int dy = -r; dy <= r; dy++) {
        int yy = cy + dy;
        if (yy < 0 || yy >= w->h)
            continue;
        int rem = r * r - dy * dy;
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= rem)
            dx++;
        int x0 = cx - dx;
        int x1 = cx + dx + 1;
        if (x0 < 0)
            x0 = 0;
        if (x1 > w->w)
            x1 = w->w;
        if (x0 < x1)
            musr_fill32(w->buf + (size_t)yy * w->w + x0,
                        (size_t)(x1 - x0), c);
    }
#endif
}

void sprach_paint_window(struct sprach_ctx *ctx, struct sprach_window *w)
{
    int is_max = w->maximized;
    int cw = is_max ? SCREEN_W : w->w;
    int ch = is_max ? WORK_AREA_H : w->h;

    sp_buf_fill(w, w->body);

    /* Title bar */
    sp_buf_rect(w, 0, 0, cw, SPRACH_TITLE_H, w->title);

    /* Mac OS 9 Platinum window controls */
    int one_frame = (ctx->tick - w->click_tick <= 1) ? 1 : 0;

    uint32_t col_close = (one_frame && w->btn_clicked == 1)
                             ? SPRACH_COL_BTN_CLOSE_D : SPRACH_COL_BTN_CLOSE;
    uint32_t col_min   = (one_frame && w->btn_clicked == 2)
                             ? SPRACH_COL_BTN_MIN_D   : SPRACH_COL_BTN_MIN;
    uint32_t col_max   = (one_frame && w->btn_clicked == 3)
                             ? SPRACH_COL_BTN_MAX_D   : SPRACH_COL_BTN_MAX;

    sp_draw_circle(w, CTRL_CLOSE_X + 1, CTRL_Y + 1, CTRL_SIZE / 2, SPRACH_COL_BTN_SHADOW);
    sp_draw_circle(w, CTRL_MIN_X + 1, CTRL_Y + 1, CTRL_SIZE / 2, SPRACH_COL_BTN_SHADOW);
    sp_draw_circle(w, CTRL_MAX_X + 1, CTRL_Y + 1, CTRL_SIZE / 2, SPRACH_COL_BTN_SHADOW);

    sp_draw_circle(w, CTRL_CLOSE_X, CTRL_Y, CTRL_SIZE / 2, col_close);
    sp_draw_circle(w, CTRL_MIN_X,   CTRL_Y, CTRL_SIZE / 2, col_min);
    sp_draw_circle(w, CTRL_MAX_X,   CTRL_Y, CTRL_SIZE / 2, col_max);

    /* Pseudo-title bar text */
    sp_buf_rect(w, CTRL_MAX_X + CTRL_SIZE + 8, 6, 48, 4, SPRACH_COL_TEXTBAR);

    /* Progress bar (tick-driven) */
    int pw = (int)((ctx->tick * 3) % (uint32_t)(cw - 24));
    sp_buf_rect(w, 12, ch - 20, pw, 8, SPRACH_COL_ACCENT);

    /* Bouncing dot */
    int bx = (int)((ctx->tick * 5 + (uint32_t)(w->slot + 1) * 37)
                   % (uint32_t)(cw - 24)) + 12;
    int by = (int)((ctx->tick * 7 + (uint32_t)(w->slot + 1) * 53)
                   % (uint32_t)(ch - 44)) + SPRACH_TITLE_H + 8;
    sp_buf_rect(w, bx, by, 6, 6, SPRACH_COL_DOT);

    /* Border */
    sp_buf_rect(w, 0, 0, cw, 1, SPRACH_COL_BORDER);
    sp_buf_rect(w, 0, 0, 1, ch, SPRACH_COL_BORDER);
    sp_buf_rect(w, 0, ch - 1, cw, 1, SPRACH_COL_BORDER);
    sp_buf_rect(w, cw - 1, 0, 1, ch, SPRACH_COL_BORDER);

    if (one_frame)
        w->btn_clicked = 0;
}

/* ── Layout commit: push MOVE commands for all surfaces ── */

void sprach_commit_layout(struct sprach_ctx *ctx)
{
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        struct sprach_window *w = &ctx->wins[i];
        if (w->slot < 0 || w->hidden)
            continue;
        copland_cmd_push(ctx->shm, COPLAND_CMD_MOVE_SURFACE,
                         w->slot, w->x, w->y, 0, 0, 0);
    }
    /* Taskbar: always at bottom */
    if (ctx->taskbar_slot >= 0)
        copland_cmd_push(ctx->shm, COPLAND_CMD_MOVE_SURFACE,
                         ctx->taskbar_slot, 0, SCREEN_H - TASKBAR_H, 0, 0, 0);
    /* Menubar: always at top */
    if (ctx->menubar_slot >= 0)
        copland_cmd_push(ctx->shm, COPLAND_CMD_MOVE_SURFACE,
                         ctx->menubar_slot, 0, 0, 0, 0, 0);
}

/* ── Taskbar (bottom) ── */

/* ── New-protocol WM bootstrap (phase 3) ──
 *
 * Connects through the connection table, wins the WM identity by
 * binding copland_compositor first, then creates the chrome
 * surfaces (menubar + taskbar) with buffers covering the existing
 * static pixel buffers.  On failure the caller falls back to the
 * legacy shm surfaces (graceful degradation, not a boot failure). */
static int sprach_proto_connect(void)
{
    uint32_t eo, ep, ea;
    uint32_t id_reg;

    /* sp_conn lives in BSS (zeroed): without conn_init it never
     * carries COPLAND_CONN_MAGIC and cp_client_connect refuses it
     * ("new protocol unavailable, legacy surfaces"). */
    cp_client_conn_init(&sp_conn);
    if (cp_client_connect(&sp_conn, (uint32_t)m4k_getpid()) < 0)
        return -1;
    cp_objmap_init(&sp_map);

    /* registry */
    id_reg = cp_objmap_new_id(&sp_map);
    sp_req(CP_ID_DISPLAY, CP_DISPLAY_GET_REGISTRY);
    cp_put_u32(&sp_mb, id_reg);
    sp_send(CP_ID_DISPLAY, CP_DISPLAY_GET_REGISTRY);
    if (!sp_wait_evt(id_reg, CP_REGISTRY_GLOBAL, 4000))
        return -1;
    while (sp_evt_pop(&eo, &ep, &ea) > 0)
        ;   /* drain remaining globals */

    /* bind compositor FIRST: claims the WM privilege (spec §15) */
    sp_id_comp = cp_objmap_new_id(&sp_map);
    sp_req(2, CP_REGISTRY_BIND);
    cp_put_u32(&sp_mb, CP_IF_COMPOSITOR);
    cp_put_u32(&sp_mb, sp_id_comp);
    sp_send(2, CP_REGISTRY_BIND);

    /* bind shm + wait for the format event */
    sp_id_shm = cp_objmap_new_id(&sp_map);
    sp_req(2, CP_REGISTRY_BIND);
    cp_put_u32(&sp_mb, CP_IF_SHM);
    cp_put_u32(&sp_mb, sp_id_shm);
    sp_send(2, CP_REGISTRY_BIND);
    if (!sp_wait_evt(sp_id_shm, CP_SHM_FORMAT, 4000))
        return -1;
    while (sp_evt_pop(&eo, &ep, &ea) > 0)
        ;

    return 0;
}

/* Create one chrome surface on the new protocol:
 * pool + buffer + surface + set_position + attach + damage + commit.
 * buf points at the caller's static pixel store (must stay alive).
 * Returns 0 on success; the surface id lands in *surf_out. */
static int sprach_proto_chrome(uint32_t *surf_out, uint32_t *buf_out,
                               uint32_t buf_addr, int32_t sx, int32_t sy,
                               int32_t w, int32_t h)
{
    /* The pool is created once and resized per chrome surface to
     * cover [0, accumulated size); buffers address it by offset.
     * v1 keeps it simple: one pool spanning both chrome buffers,
     * created with the final size on the first call. */
    static uint32_t pool_created;
    uint32_t id_surf, id_buf;

    if (!pool_created) {
        /* Pool covers both chrome pixel stores: taskbar_buf then
         * menubar_buf (flat BSS addresses, page-less granularity
         * allowed by spec §6).  Size = distance from taskbar_buf
         * start to menubar_buf end.  Both are static in this
         * file, so the linker keeps them in BSS order. */
        uint32_t base = (uint32_t)(uintptr_t)taskbar_buf;
        uint32_t end = (uint32_t)(uintptr_t)menubar_buf
                     + SCREEN_W * MENUBAR_H * 4;
#ifdef SPRACH_POOL_DEBUG
        ser_puts("[SPRACH] pool base=0x");
        print_u32(base);
        ser_puts(" end=0x");
        print_u32(end);
        ser_puts("\n");
#endif
        sp_id_pool = cp_objmap_new_id(&sp_map);
        sp_req(sp_id_shm, CP_SHM_CREATE_POOL);
        cp_put_u32(&sp_mb, sp_id_pool);
        cp_put_u32(&sp_mb, base);
        cp_put_i32(&sp_mb, (int32_t)(end - base));
        sp_send(sp_id_shm, CP_SHM_CREATE_POOL);
        pool_created = 1;
    }

    id_buf = cp_objmap_new_id(&sp_map);
    sp_req(sp_id_pool, CP_POOL_CREATE_BUFFER);
    cp_put_u32(&sp_mb, id_buf);
    cp_put_i32(&sp_mb, buf_addr - (uint32_t)(uintptr_t)taskbar_buf);
    cp_put_i32(&sp_mb, w);
    cp_put_i32(&sp_mb, h);
    cp_put_i32(&sp_mb, w * 4);
    cp_put_i32(&sp_mb, 0);   /* xrgb8888 */
    sp_send(sp_id_pool, CP_POOL_CREATE_BUFFER);
    *buf_out = id_buf;

    id_surf = cp_objmap_new_id(&sp_map);
    sp_req(sp_id_comp, CP_COMPOSITOR_CREATE_SURFACE);
    cp_put_u32(&sp_mb, id_surf);
    sp_send(sp_id_comp, CP_COMPOSITOR_CREATE_SURFACE);

    /* WM privilege: place the surface before first commit */
    sp_req(id_surf, CP_SURFACE_SET_POSITION);
    cp_put_i32(&sp_mb, sx);
    cp_put_i32(&sp_mb, sy);
    sp_send(id_surf, CP_SURFACE_SET_POSITION);

    sp_req(id_surf, CP_SURFACE_ATTACH);
    cp_put_u32(&sp_mb, id_buf);
    sp_send(id_surf, CP_SURFACE_ATTACH);

    sp_req(id_surf, CP_SURFACE_DAMAGE);
    cp_put_i32(&sp_mb, 0);
    cp_put_i32(&sp_mb, 0);
    cp_put_i32(&sp_mb, w);
    cp_put_i32(&sp_mb, h);
    sp_send(id_surf, CP_SURFACE_DAMAGE);

    sp_req(id_surf, CP_SURFACE_COMMIT);
    sp_send(id_surf, CP_SURFACE_COMMIT);
    *surf_out = id_surf;
    return 0;
}

/* Push one full-surface damage+commit for a chrome surface. */
static void sprach_proto_repaint(uint32_t surf, int32_t w, int32_t h)
{
    sp_req(surf, CP_SURFACE_DAMAGE);
    cp_put_i32(&sp_mb, 0);
    cp_put_i32(&sp_mb, 0);
    cp_put_i32(&sp_mb, w);
    cp_put_i32(&sp_mb, h);
    sp_send(surf, CP_SURFACE_DAMAGE);
    sp_req(surf, CP_SURFACE_COMMIT);
    sp_send(surf, CP_SURFACE_COMMIT);
}

int sprach_create_taskbar(struct sprach_ctx *ctx)
{
    ctx->taskbar_slot = -1;
    int before = 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            before++;

    if (copland_cmd_push(ctx->shm, COPLAND_CMD_CREATE_SURFACE,
                         0, SCREEN_H - TASKBAR_H, SCREEN_W, TASKBAR_H,
                         (int32_t)SPRACH_COL_TASKBAR_BG,
                         COPLAND_SURF_VISIBLE) != 0)
        return -1;

    if (sprach_wait_slot(ctx, before) != 0)
        return -1;

    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (ctx->shm->surfaces[i].in_use &&
            !ctx->shm->surfaces[i].buffer_ptr) {
            ctx->taskbar_slot = i;
            ctx->shm->surfaces[i].buffer_ptr = (uint32_t)(uintptr_t)taskbar_buf;
            return 0;
        }
    }
    return -1;
}

void sprach_draw_taskbar(struct sprach_ctx *ctx)
{
    sp_fill(taskbar_buf, SCREEN_W * TASKBAR_H, SPRACH_COL_TASKBAR_BG);

    int icon_y = (TASKBAR_H - DOCK_ICON_SIZE) / 2;
    int bx = DOCK_PAD;

    /* Far-left: the Launchpad grid icon (nine-square).  Opens the
     * app launcher overlay.  Uses the PNG grid icon when available,
     * pixel-drawn fallback otherwise. */
    {
        if (icons_data_find("launchpad")) {
            sp_icon_blit32(taskbar_buf, SCREEN_W, TASKBAR_H, bx, icon_y,
                           "launchpad");
        } else {
            sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, bx, icon_y,
                    DOCK_ICON_SIZE, DOCK_ICON_SIZE, 0x00506070);
            sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, bx + 1, icon_y + 1,
                    DOCK_ICON_SIZE - 2, DOCK_ICON_SIZE - 2, 0x0080A0B0);
            for (int gy = 0; gy < 3; gy++)
                for (int gx = 0; gx < 3; gx++) {
                    int c = bx + 6 + gx * 8;
                    int r = icon_y + 6 + gy * 8;
                    sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, c, r, 5, 5,
                            ctx->lp_open ? 0x00FFFFFF : 0x00203040);
                }
        }
        bx += DOCK_ICON_PITCH;
    }

    /* One 32x32 icon per live window, left to right (icon only — no
     * text buttons; the active window gets a bright indicator bar
     * under its icon). */
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot < 0)
            continue;   /* closed: dock icon removed automatically */

        int active = (i == ctx->active && ctx->term_slot < 0);
        if (active)
            sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, bx - 2, icon_y + DOCK_ICON_SIZE + 2,
                    DOCK_ICON_SIZE + 4, 3, SPRACH_COL_ACCENT);
        sp_icon_window(taskbar_buf, SCREEN_W, TASKBAR_H, bx, icon_y,
                       ctx->wins[i].title);
        bx += DOCK_ICON_PITCH;
    }

    /* Terminal client window icon (when the terminal is alive) */
    if (ctx->term_slot >= 0) {
        int active = (ctx->active < 0);
        if (active)
            sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, bx - 2, icon_y + DOCK_ICON_SIZE + 2,
                    DOCK_ICON_SIZE + 4, 3, SPRACH_COL_ACCENT);
        sp_icon_terminal(taskbar_buf, SCREEN_W, TASKBAR_H, bx, icon_y, 0x006060A0);
        bx += DOCK_ICON_PITCH;
    }

    /* Far right: the fixed terminal launcher icon.  Launches
     * /bin/terminal when none runs; otherwise focuses/restores it. */
    {
        int lx = SCREEN_W - DOCK_ICON_SIZE - DOCK_PAD;
        int launcher_active = (ctx->term_slot >= 0 && ctx->active < 0);
        if (launcher_active)
            sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, lx - 2, icon_y + DOCK_ICON_SIZE + 2,
                    DOCK_ICON_SIZE + 4, 3, SPRACH_COL_ACCENT);
        sp_icon_terminal(taskbar_buf, SCREEN_W, TASKBAR_H, lx, icon_y, 0x00306030);
    }


    /* Top highlight edge (1px light line) */
    sp_rect(taskbar_buf, SCREEN_W, TASKBAR_H, 0, 0, SCREEN_W, 1,
            SPRACH_COL_TASKBAR_TOP);
}

/* ── Taskbar invalidation ──
 * Returns 1 when the dock buffer must be repainted: window set or
 * active-window state changed.  Lets the main loop skip the fill+icon
 * pass on the vast majority of frames.  active is used EXACTLY as
 * stored (window index, or -1 = terminal focused). */

int sprach_taskbar_dirty(struct sprach_ctx *ctx)
{
    int dirty = (ctx->active != ctx->last_tbar_active);

    uint32_t wstate = 0;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot >= 0)
            wstate |= (1u << (i * 2));
        if (ctx->wins[i].hidden)
            wstate |= (1u << (i * 2 + 1));
    }
    if (ctx->term_slot >= 0)
        wstate |= (1u << 16);
    dirty |= (wstate != ctx->last_tbar_win);

    if (dirty) {
        ctx->last_tbar_active = ctx->active;
        ctx->last_tbar_win = wstate;
        ctx->last_tbar_minute = 0;
    }
    return dirty;
}

/* ── Top Menubar (Mac OS style) ── */

int sprach_create_menubar(struct sprach_ctx *ctx)
{
    ctx->menubar_slot = -1;
    int before = 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            before++;

    /* Created AFTER taskbar → gets a higher slot → composited on top */
    if (copland_cmd_push(ctx->shm, COPLAND_CMD_CREATE_SURFACE,
                         0, 0, SCREEN_W, MENUBAR_H,
                         (int32_t)SPRACH_COL_MENUBAR_BG,
                         COPLAND_SURF_VISIBLE) != 0)
        return -1;

    if (sprach_wait_slot(ctx, before) != 0)
        return -1;

    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (ctx->shm->surfaces[i].in_use &&
            !ctx->shm->surfaces[i].buffer_ptr) {
            ctx->menubar_slot = i;
            ctx->shm->surfaces[i].buffer_ptr = (uint32_t)(uintptr_t)menubar_buf;
            return 0;
        }
    }
    return -1;
}

/* Left: bold "M4KK1" brand, center: "Desktop N" (multi-desktop
 * reserved), right: HH:MM:SS clock (repainted once per second by the
 * main loop; see menu_last_second).  Clicking the clock area toggles
 * a popup clock window (see sprach_handle_click / clock_open). */
void sprach_draw_menubar(struct sprach_ctx *ctx)
{
    sp_fill(menubar_buf, SCREEN_W * MENUBAR_H, SPRACH_COL_MENUBAR_BG);

    /* Bottom separator */
    sp_rect(menubar_buf, SCREEN_W, MENUBAR_H, 0, MENUBAR_H - 1, SCREEN_W, 1, SPRACH_COL_BORDER);

    /* Left: system name — bold (double-strike), black */
    sp_draw_str_bold(menubar_buf, SCREEN_W, MENUBAR_H, 8, (MENUBAR_H - 7) / 2,
                     "M4KK1", SPRACH_COL_MENUBAR_FG);

    /* Center: current desktop index ("Desktop 1"; future workspaces
     * will change desktop_idx). */
    {
        char desk[16];
        musr_strncpy(desk, "Desktop ", sizeof(desk) - 1);
        int n = 8;
        int v = ctx->desktop_idx + 1;
        char tmp[8];
        int t = 0;
        do { tmp[t++] = '0' + v % 10; v /= 10; } while (v);
        while (t && n < (int)sizeof(desk) - 1)
            desk[n++] = tmp[--t];
        desk[n] = '\0';
        int dw = 0;
        for (const char *p = desk; *p; p++)
            dw += 6;
        sp_draw_str(menubar_buf, SCREEN_W, MENUBAR_H, (SCREEN_W - dw) / 2,
                    (MENUBAR_H - 7) / 2, desk, SPRACH_COL_MENUBAR_TXT);
    }

    /* Right: clock HH:MM:SS from system time (S_TIME → kernel RTC) */
    int epoch = musr_sc_time();
    if (epoch < 0)
        epoch = 0;   /* unset RTC → fall back to 00:00:00 */
    int total_sec = epoch % 86400;
    int hours = total_sec / 3600;
    int minutes = (total_sec % 3600) / 60;
    int seconds = total_sec % 60;

    char time_str[9];
    time_str[0] = '0' + (char)(hours / 10);
    time_str[1] = '0' + (char)(hours % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (char)(minutes / 10);
    time_str[4] = '0' + (char)(minutes % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (char)(seconds / 10);
    time_str[7] = '0' + (char)(seconds % 10);
    time_str[8] = '\0';

    int tx = SCREEN_W - 8 * 7 - 8;
    int ty = 4;
    for (int i = 0; i < 8; i++)
        sp_draw_char(menubar_buf, SCREEN_W, MENUBAR_H, tx + i * 7, ty,
                     time_str[i], SPRACH_COL_MENUBAR_FG);

    /* ── System tray: three 16x16 glyphs left of the clock ──
     * (volume, network, bluetooth).  Icons are simple hand-drawn
     * shapes (sp_icon_* helpers below); the network glyph reflects
     * the REAL interface state via S_NETINFO.  Click handlers live
     * in sprach_handle_click (TRAY_* hit zones). */
    {
        int tray_x = tx - TRAY_W - 8;
        uint32_t ip, mask, gw;
        uint32_t net_up = musr_sc3(S_NETINFO, (uint32_t)&ip,
                                   (uint32_t)&mask, (uint32_t)&gw);
        sp_icon_volume(menubar_buf, SCREEN_W, MENUBAR_H,
                       tray_x, 4, SPRACH_COL_MENUBAR_FG);
        sp_icon_network(menubar_buf, SCREEN_W, MENUBAR_H,
                        tray_x + 20, 4, SPRACH_COL_MENUBAR_FG,
                        net_up != 0);
        sp_icon_bluetooth(menubar_buf, SCREEN_W, MENUBAR_H,
                          tray_x + 40, 4, SPRACH_COL_MENUBAR_FG,
                          ctx->bt_on);
    }

    /* Clock popup (toggled by clicking the clock area) is drawn on its
     * OWN surface (clock_popup_buf) below the menubar — the menubar
     * buffer is only 24px tall, painting a popup into it would run
     * off the end.  See sprach_draw_clock_popup(). */
}

/* ── Clock popup window (own surface under the menubar) ── */

#define CLOCK_POP_W     168
#define CLOCK_POP_H     64
#define ABOUT_POP_H     160         /* About panel + Settings picker */
static uint32_t clock_popup_buf[CLOCK_POP_W * ABOUT_POP_H];

/* Epoch → YYYY-MM-DD (proleptic Gregorian, good to 9999). */
static void clock_fmt_date(int epoch, char out[11])
{
    int days = epoch / 86400;
    int year = 1970, month = 1;
    static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (;;) {
        int leap = (year % 4 == 0 && year % 100 != 0)
                   || year % 400 == 0;
        int ylen = leap ? 366 : 365;
        if (days >= ylen) {
            days -= ylen;
            year++;
            continue;
        }
        int md = dim[month - 1];
        if (month == 2 && leap)
            md = 29;
        if (days >= md) {
            days -= md;
            month++;
            continue;
        }
        break;
    }
    int day = days + 1;
    out[0] = '0' + (year / 1000) % 10;
    out[1] = '0' + (year / 100) % 10;
    out[2] = '0' + (year / 10) % 10;
    out[3] = '0' + year % 10;
    out[4] = '-';
    out[5] = '0' + month / 10;
    out[6] = '0' + month % 10;
    out[7] = '-';
    out[8] = '0' + day / 10;
    out[9] = '0' + day % 10;
    out[10] = '\0';
}

/* Create the popup surface hidden; shown on clock-area click. */
int sprach_create_clock_popup(struct sprach_ctx *ctx)
{
    if (ctx->clock_slot >= 0)
        return 0;
    int before = 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            before++;
    /* Create hidden (no COPLAND_SURF_VISIBLE) */
    if (copland_cmd_push(ctx->shm, COPLAND_CMD_CREATE_SURFACE,
                         SCREEN_W - CLOCK_POP_W - 8, MENUBAR_H,
                         CLOCK_POP_W, CLOCK_POP_H,
                         (int32_t)0x00E8E8E8, 0) != 0)
        return -1;
    if (sprach_wait_slot(ctx, before) != 0)
        return -1;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (ctx->shm->surfaces[i].in_use &&
            !ctx->shm->surfaces[i].buffer_ptr) {
            ctx->clock_slot = i;
            ctx->shm->surfaces[i].buffer_ptr =
                (uint32_t)(uintptr_t)clock_popup_buf;
            return 0;
        }
    }
    return -1;
}

/* ── Desktop settings persistence (/etc/desktop.cfg) ──
 *
 * Plain-text key=value store, one line each:
 *   theme=N vol=N bt=N
 * Read once at boot; rewritten on every change (small file, VFS
 * truncates on O_TRUNC reopen).  Missing file = defaults.
 */
#define SPRACH_CFG_PATH "/etc/desktop.cfg"

static void sprach_cfg_save(struct sprach_ctx *ctx)
{
    char b[48];
    int k = 0;
    const char *s;

    s = "theme="; while (*s) b[k++] = *s++;
    b[k++] = (char)('0' + gui_wallpaper_get_theme());
    b[k++] = '\n';
    s = "vol=";   while (*s) b[k++] = *s++;
    b[k++] = (char)('0' + ctx->vol_level / 100);
    b[k++] = (char)('0' + (ctx->vol_level / 10) % 10);
    b[k++] = (char)('0' + ctx->vol_level % 10);
    b[k++] = '\n';
    s = "bt=";    while (*s) b[k++] = *s++;
    b[k++] = ctx->bt_on ? '1' : '0';
    b[k++] = '\n';

    int fd = musr_sc_open(SPRACH_CFG_PATH, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        ser_puts("[SPRACH] cfg save: open failed\n");
        return;
    }
    musr_sc_write(fd, b, k);
    musr_sc_close(fd);
}

static void sprach_cfg_load(struct sprach_ctx *ctx)
{
    char b[64];
    int fd = musr_sc_open(SPRACH_CFG_PATH, O_RDONLY);
    if (fd < 0)
        return;                     /* first boot: keep defaults */
    int n = musr_sc_read(fd, b, sizeof(b) - 1);
    musr_sc_close(fd);
    if (n <= 0)
        return;
    b[n] = '\0';

    /* Minimal key=value scan (no sscanf in this environment). */
    for (int i = 0; i + 6 < n; i++) {
        if (b[i] == 't' && b[i+1] == 'h' && b[i+2] == 'e' &&
            b[i+3] == 'm' && b[i+4] == 'e' && b[i+5] == '=') {
            int v = b[i+6] - '0';
            if (v >= 0 && v < 6)
                gui_wallpaper_set_theme(v);
        } else if (b[i] == 'v' && b[i+1] == 'o' && b[i+2] == 'l' &&
                   b[i+3] == '=') {
            int v = 0;
            int j = i + 4;
            while (j < n && b[j] >= '0' && b[j] <= '9' && v < 1000)
                v = v * 10 + (b[j++] - '0');
            if (v > 100) v = 100;
            ctx->vol_level = v;
        } else if (b[i] == 'b' && b[i+1] == 't' && b[i+2] == '=') {
            ctx->bt_on = (b[i+3] == '1');
        }
    }
}

void sprach_draw_clock_popup(struct sprach_ctx *ctx)
{
    if (ctx->clock_slot < 0 || !ctx->clock_open)
        return;

    sp_fill(clock_popup_buf, CLOCK_POP_W * CLOCK_POP_H, 0x00E8E8E8);
    sp_rect(clock_popup_buf, CLOCK_POP_W, CLOCK_POP_H, 0, 0, CLOCK_POP_W, CLOCK_POP_H,
            SPRACH_COL_BORDER);

    if (ctx->clock_about) {
        /* "About This PC" panel: kernel version, memory, CPU.  The
         * panel surface is resized to ABOUT_POP_H while About is
         * open (see sprach_create_clock_popup / toggle paths). */
        sp_fill(clock_popup_buf, CLOCK_POP_W * ABOUT_POP_H, 0x00E8E8E8);
        sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 0, 0,
                CLOCK_POP_W, ABOUT_POP_H, SPRACH_COL_BORDER);
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 8,
                    "About This PC", SPRACH_COL_MENUBAR_FG);
        sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 8, 24,
                CLOCK_POP_W - 16, 1, SPRACH_COL_BORDER);

        struct utsname uts;
        int have_uts = (musr_sc_uname(&uts) == 0);
        char ln[40];
        int k;

        /* OS / kernel version from uname */
        k = 0;
        const char *p1 = "OS:      ";
        while (*p1 && k < 38) ln[k++] = *p1++;
        if (have_uts) {
            for (int i = 0; uts.sysname[i] && k < 38; i++)
                ln[k++] = uts.sysname[i];
            ln[k++] = ' ';
            for (int i = 0; uts.release[i] && k < 38; i++)
                ln[k++] = uts.release[i];
        } else {
            const char *fb = "M4KK1 4P1";
            while (*fb && k < 38) ln[k++] = *fb++;
        }
        ln[k] = '\0';
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 32, ln,
                    SPRACH_COL_MENUBAR_FG);

        /* Kernel build string */
        k = 0;
        p1 = "Kernel:  ";
        while (*p1 && k < 38) ln[k++] = *p1++;
        if (have_uts) {
            for (int i = 0; uts.version[i] && k < 38; i++)
                ln[k++] = uts.version[i];
            ln[k++] = ' ';
            for (int i = 0; uts.machine[i] && k < 38; i++)
                ln[k++] = uts.machine[i];
        } else {
            const char *fb = "M4K i386 monolithic";
            while (*fb && k < 38) ln[k++] = *fb++;
        }
        ln[k] = '\0';
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 44, ln,
                    SPRACH_COL_MENUBAR_FG);

        /* Memory in KB — sysinfo reports bytes; spec wants
         * "Total/Used/Free" triple.  Hand-rolled u32-to-dec. */
        struct sysinfo si;
        if (musr_sc_sysinfo(&si) == 0) {
            uint32_t tot_kb = si.total_ram / 1024u;
            uint32_t used_kb = si.used_ram / 1024u;
            uint32_t free_kb = si.free_ram / 1024u;
            k = 0;
            p1 = "Mem KB:  ";
            while (*p1 && k < 38) ln[k++] = *p1++;
            char dg[12];
            int nd = 0;
            do { dg[nd++] = (char)('0' + tot_kb % 10); tot_kb /= 10; }
            while (tot_kb && nd < 11);
            while (nd && k < 38) ln[k++] = dg[--nd];
            const char *p2 = "/";
            while (*p2 && k < 38) ln[k++] = *p2++;
            nd = 0;
            do { dg[nd++] = (char)('0' + used_kb % 10); used_kb /= 10; }
            while (used_kb && nd < 11);
            while (nd && k < 38) ln[k++] = dg[--nd];
            const char *p3 = "/";
            while (*p3 && k < 38) ln[k++] = *p3++;
            nd = 0;
            do { dg[nd++] = (char)('0' + free_kb % 10); free_kb /= 10; }
            while (free_kb && nd < 11);
            while (nd && k < 38) ln[k++] = dg[--nd];
            ln[k] = '\0';
            sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 56,
                        ln, SPRACH_COL_MENUBAR_FG);
        }

        /* Build date — __DATE__ of the sprach translation unit */
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 68,
                    "Built:   " __DATE__, SPRACH_COL_MENUBAR_FG);

        /* CPU: fixed identification from the boot CPU detect */
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 80,
                    "CPU:     x86 i386 (QEMU std)", SPRACH_COL_MENUBAR_FG);

        /* Tray control panels (opened from the menubar tray glyphs) */
        if (ctx->tray_mode) {
            const char *title = ctx->tray_mode == 1 ? "Volume"
                              : ctx->tray_mode == 2 ? "Network"
                              : "Bluetooth";
            sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 8,
                        title, SPRACH_COL_MENUBAR_FG);
            sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 8, 20,
                    CLOCK_POP_W - 16, 1, SPRACH_COL_BORDER);

            if (ctx->tray_mode == 3) {
                /* Bluetooth: radio toggle + adapter presence from the
                 * PCI scan.  QEMU i386 has no class-0xD controller,
                 * so the honest report is "no adapter". */
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 28, ctx->bt_on ? "Radio: ON"
                                               : "Radio: OFF",
                            SPRACH_COL_MENUBAR_FG);
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 40, "Adapter: none (QEMU)",
                            SPRACH_COL_MENUBAR_DIM);
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 52, "click radio to toggle",
                            SPRACH_COL_MENUBAR_DIM);
            } else if (ctx->tray_mode == 2) {
                /* Network: live S_NETINFO state + IP */
                uint32_t ip, mask, gw;
                uint32_t up = musr_sc3(S_NETINFO, (uint32_t)&ip,
                                       (uint32_t)&mask, (uint32_t)&gw);
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 28, up ? "eth0: up" : "eth0: down",
                            SPRACH_COL_MENUBAR_FG);
                if (up) {
                    char b[20];
                    int k = 0;
                    b[k++] = 'I'; b[k++] = 'P'; b[k++] = ':';
                    b[k++] = ' ';
                    for (int i = 0; i < 4; i++) {
                        uint8_t o = (uint8_t)(ip >> (24 - 8 * i));
                        if (o >= 100) b[k++] = '0' + o / 100;
                        if (o >= 10) b[k++] = '0' + (o / 10) % 10;
                        b[k++] = '0' + o % 10;
                        if (i < 3) b[k++] = '.';
                    }
                    b[k] = '\0';
                    sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                                10, 40, b, SPRACH_COL_MENUBAR_FG);
                }
            } else {
                /* Volume: level bar + click-to-adjust (no audio
                 * hardware yet; the level is the stored software
                 * setting, persisted in /etc/desktop.cfg) */
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 28, "Level:", SPRACH_COL_MENUBAR_FG);
                sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                        10, 40, 140, 8, SPRACH_COL_BORDER);
                sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                        12, 42,
                        (int)((uint32_t)ctx->vol_level * 136u / 100u),
                        4, 0x003060C0);
                sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            10, 56, "-   (click to adjust)   +",
                            SPRACH_COL_MENUBAR_DIM);
            }
            ctx->shm->dirty = 1;
            return;
        }

        if (ctx->clock_settings) {
            /* System Settings panel: wallpaper theme picker.  Six
             * theme swatches (two rows of three) painted with the
             * actual theme gradient; click a swatch to apply it
             * (see the clock-popup hit-test in sprach_handle_click).
             * The current theme gets a highlight border. */
            sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 96,
                        "Wallpaper:",
                        SPRACH_COL_MENUBAR_FG);
            int cur = gui_wallpaper_get_theme();
            for (int t = 0; t < 6; t++) {
                int sx = 12 + (t % 3) * 48;
                int sy = 108 + (t / 3) * 20;
                uint32_t top, bot;
                switch (t) {
                case 0: top = 0x00000044u; bot = 0x0066FFu; break;
                case 1: top = 0x00224488u; bot = 0x0044AACCu; break;
                case 2: top = 0x00331166u; bot = 0x00AA44CCu; break;
                case 3: top = 0x001A1A2Eu; bot = 0x00444466u; break;
                case 4: top = 0x00663300u; bot = 0x00CCAA33u; break;
                default: top = 0x00104010u; bot = 0x0030A030u; break;
                }
                /* swatch: vertical gradient in 6 steps */
                for (int r = 0; r < 14; r++) {
                    uint32_t f = (uint32_t)r * 0xFF / 13u;
                    uint32_t c = (((top & 0xFF0000u) * (0xFF - f)
                                   + (bot & 0xFF0000u) * f) >> 8)
                                   & 0xFF0000u;
                    c |= (((top & 0xFF00u) * (0xFF - f)
                           + (bot & 0xFF00u) * f) >> 8) & 0xFF00u;
                    c |= (((top & 0xFFu) * (0xFF - f)
                           + (bot & 0xFFu) * f) >> 8) & 0xFFu;
                    sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            sx, sy + r, 44, 1, c);
                }
                if (t == cur)
                    sp_rect(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H,
                            sx - 1, sy - 1, 46, 16, 0x003060C0);
            }
            ctx->shm->dirty = 1;
            return;
        }

        char up[24] = "up ";
        uint32_t secs = (uint32_t)musr_sc_uptime();
        int n = 3;
        up[n++] = '0' + (char)((secs / 3600) % 10);
        up[n++] = 'h';
        up[n++] = ' ';
        up[n++] = '0' + (char)((secs / 60 / 10) % 10);
        up[n++] = '0' + (char)((secs / 60) % 10);
        up[n++] = 'm';
        up[n] = '\0';
        sp_draw_str(clock_popup_buf, CLOCK_POP_W, ABOUT_POP_H, 10, 92, up,
                    SPRACH_COL_MENUBAR_FG);
        ctx->shm->dirty = 1;
        return;
    }

    sp_icon_clock(clock_popup_buf, CLOCK_POP_W, CLOCK_POP_H, 10, 16, 0x00F0F0F0);

    int epoch = musr_sc_time();
    if (epoch < 0)
        epoch = 0;
    int total_sec = epoch % 86400;

    char ds[11];
    clock_fmt_date(epoch, ds);
    sp_draw_str(clock_popup_buf, CLOCK_POP_W, CLOCK_POP_H, 48, 10, ds,
                SPRACH_COL_MENUBAR_FG);

    char ts[9];
    ts[0] = '0' + (char)((total_sec / 3600) / 10);
    ts[1] = '0' + (char)((total_sec / 3600) % 10);
    ts[2] = ':';
    ts[3] = '0' + (char)(((total_sec % 3600) / 60) / 10);
    ts[4] = '0' + (char)(((total_sec % 3600) / 60) % 10);
    ts[5] = ':';
    ts[6] = '0' + (char)((total_sec % 60) / 10);
    ts[7] = '0' + (char)((total_sec % 60) % 10);
    ts[8] = '\0';
    sp_draw_str_bold(clock_popup_buf, CLOCK_POP_W, CLOCK_POP_H, 48, 26, ts,
                     SPRACH_COL_MENUBAR_FG);

    sp_draw_str(clock_popup_buf, CLOCK_POP_W, CLOCK_POP_H, 10, 50, "M4KK1 4P1",
                SPRACH_COL_MENUBAR_DIM);

    ctx->shm->surfaces[ctx->clock_slot].x = ctx->clock_x;
    ctx->shm->surfaces[ctx->clock_slot].y = ctx->clock_y;
    ctx->shm->dirty = 1;
    ser_puts("[SPRACH] clock popup ");
    ser_puts(ts);
    ser_puts("\n");
}

/* ==== APP MENU (brand dropdown) =======================================
 * Clicking the bold "M4KK1" brand in the menubar opens a dropdown with
 * four entries.  It is drawn on its own surface below the brand. */

#define APP_MENU_W     150
#define APP_MENU_H     128          /* 4 items * 28 + padding */
#define APP_MENU_ITEM_H 28
#define APP_MENU_ITEMS 4
#define APP_MENU_X     2            /* dropdown x: under the brand text */
static uint32_t app_menu_buf[APP_MENU_W * APP_MENU_H];

static const char *app_menu_items[4] = {
    "About This PC",
    "System Settings",
    "Lock Screen",
    "Shut Down",
};

int sprach_create_app_menu(struct sprach_ctx *ctx)
{
    if (ctx->menu_slot >= 0)
        return 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (!ctx->shm->surfaces[i].in_use) {
            struct copland_surface *s =
                &ctx->shm->surfaces[i];
            s->x = 6;
            s->y = MENUBAR_H;
            s->w = APP_MENU_W;
            s->h = APP_MENU_H;
            s->color = 0x00E8E8E8;
            s->flags = 0 /* start hidden: no VISIBLE flag */;
            s->dmg_w = 0;
            s->buffer_ptr =
                (uint32_t)(uintptr_t)app_menu_buf;
            s->in_use = 1;      /* publish LAST: the compositor
                                 * must never see in_use=1 with
                                 * half-filled geometry */
            ctx->menu_slot = i;
            return 0;
        }
    }
    return -1;
}

void sprach_draw_app_menu(struct sprach_ctx *ctx)
{
    if (ctx->menu_slot < 0 || !ctx->menu_open)
        return;
    sp_fill(app_menu_buf, APP_MENU_W * APP_MENU_H, 0x00E8E8E8);
    sp_rect(app_menu_buf, APP_MENU_W, APP_MENU_H, 0, 0, APP_MENU_W, APP_MENU_H,
            SPRACH_COL_BORDER);
    for (int i = 0; i < 4; i++) {
        int iy = 4 + i * APP_MENU_ITEM_H;
        int sel = (ctx->mouse_x >= 6 && ctx->mouse_x < 6 + APP_MENU_W &&
                   ctx->mouse_y >= MENUBAR_H + iy &&
                   ctx->mouse_y < MENUBAR_H + iy + APP_MENU_ITEM_H - 4);
        if (sel)
            sp_rect(app_menu_buf, APP_MENU_W, APP_MENU_H, 3, iy - 2, APP_MENU_W - 6,
                    APP_MENU_ITEM_H - 4, 0x003060C0);
        sp_draw_str(app_menu_buf, APP_MENU_W, APP_MENU_H, 12, iy + 6,
                    app_menu_items[i],
                    sel ? 0x00FFFFFF : SPRACH_COL_MENUBAR_FG);
    }
    ctx->shm->dirty = 1;
}

/* Execute the clicked app-menu item (mi = 0..3). */
static void sprach_app_menu_activate(struct sprach_ctx *ctx, int mi)
{
    ser_puts("[SPRACH] app menu: ");
    ser_puts(app_menu_items[mi]);
    ser_puts("\n");
    switch (mi) {
    case 0:   /* About This PC → info popup in the clock popup style */
        ctx->clock_open = 1;
        ctx->clock_about = 1;
        ctx->clock_settings = 0;
        ctx->tray_mode = 0;
        ctx->clock_x = (SCREEN_W - CLOCK_POP_W) / 2;
        ctx->clock_y = 200;
        ctx->clock_last_sec = -1;
        if (ctx->clock_slot >= 0) {
            ctx->shm->surfaces[ctx->clock_slot].x = ctx->clock_x;
            ctx->shm->surfaces[ctx->clock_slot].y = ctx->clock_y;
            ctx->shm->surfaces[ctx->clock_slot].h = ABOUT_POP_H;
            ctx->shm->surfaces[ctx->clock_slot].flags |=
                COPLAND_SURF_VISIBLE;
            sprach_draw_clock_popup(ctx);
        }
        break;
    case 1:   /* System Settings → wallpaper picker panel */
        ser_puts("[SPRACH] system settings\n");
        if (ctx->clock_slot >= 0) {
            ctx->clock_open = 1;
            ctx->clock_about = 1;
            ctx->clock_settings = 1;
            ctx->tray_mode = 0;
            ctx->clock_x = (SCREEN_W - CLOCK_POP_W) / 2;
            ctx->clock_y = 200;
            ctx->clock_last_sec = -1;
            ctx->shm->surfaces[ctx->clock_slot].x = ctx->clock_x;
            ctx->shm->surfaces[ctx->clock_slot].y = ctx->clock_y;
            ctx->shm->surfaces[ctx->clock_slot].h = ABOUT_POP_H;
            ctx->shm->surfaces[ctx->clock_slot].flags |=
                COPLAND_SURF_VISIBLE;
            sprach_draw_clock_popup(ctx);
        }
        break;
    case 2:   /* Lock Screen → end the session, MDM regains the screen */
        ser_puts("[SPRACH] locking screen...\n");
        ctx->shm->shutdown = 1;
        break;
    case 3:   /* Shut Down → reboot syscall (magic-gated 0x6D).
         * QEMU std i386 has no ACPI poweroff; the triple-fault warm
         * reset in the kernel's reboot handler is the only machine
         * restart this hardware offers. */
        ser_puts("[SPRACH] shutting down (reboot syscall)...\n");
        ctx->shm->shutdown = 1;
        ctx->shm->reboot = 1;
        break;
    }
    ctx->shm->dirty = 1;
}

/* ==== LAUNCHPAD ======================================================
 * A full-work-area overlay with a 4-column grid of installed apps.
 * The app list is SCANNED at open time from /bin (= /usr/bin symlink)
 * and /usr/bin: every regular executable except the WM/server-internal
 * ELFs (spawning those again would kill or duplicate the session).
 * Clicking an icon forks+spawns the app; clicking the background or
 * pressing Esc closes the overlay. */

#define LP_W  SCREEN_W
#define LP_H  (SCREEN_H - MENUBAR_H - TASKBAR_H)
static uint32_t lp_buf[LP_W * LP_H];

#define LP_MAX_APPS 40
struct lp_app {
    char name[16];      /* basename shown under the icon */
    char path[40];      /* /bin/<name> used for spawn */
    uint32_t icon;      /* icon kind (LP_ICON_*) */
};
static struct lp_app lp_apps[LP_MAX_APPS];
static int lp_app_count = 0;

/* WM/server-internal ELFs that must never be launched from Launchpad:
 * they already own the compositor/WM/session. */
static const char *lp_hidden_apps[] = {
    "copland", "sprach", "sprach_stack", "sprach_scroll", "sprach_tiling",
    "login", "mdm", "mdm_mini", "init", "reset-passwd", "fsck",
};

#define LP_ICON_TERMINAL 0
#define LP_ICON_WINDOW   1
#define LP_ICON_GEAR     2
#define LP_ICON_FILE     3

static uint32_t *lp_icon_for(struct lp_app *a)
{
    /* Named PNG assets first: app basename matches an icons_data
     * entry (e.g. "clock", "altr", "logview"). */
    uint32_t *pic = icons_data_find(a->name);
    if (pic)
        return (uint32_t *)pic;
    /* Network tools get the net icon set. */
    if (a->name[0] == 'p' && a->name[1] == 'i' && a->name[2] == 'n'
        && a->name[3] == 'g')
        return (uint32_t *)icons_data_find("net_056");
    if (a->name[0] == 'w' && a->name[1] == 'g' && a->name[2] == 'e'
        && a->name[3] == 't')
        return (uint32_t *)icons_data_find("net_057");
    if (a->name[0] == 'i' && a->name[1] == 'f')
        return (uint32_t *)icons_data_find("net_058");
    switch (a->icon) {
    case LP_ICON_TERMINAL: return icon_terminal;
    case LP_ICON_GEAR:     return icon_gear;
    case LP_ICON_FILE:     return icon_file;
    default:               return icon_window;
    }
}

static int lp_name_hidden(const char *n)
{
    for (unsigned i = 0;
         i < sizeof(lp_hidden_apps) / sizeof(lp_hidden_apps[0]); i++) {
        const char *h = lp_hidden_apps[i];
        int j = 0;
        while (n[j] && h[j] && n[j] == h[j])
            j++;
        if (!n[j] && !h[j])
            return 1;
    }
    return 0;
}

/* Pick an icon kind from the app basename. */
static uint32_t lp_icon_kind(const char *n)
{
    if (n[0] == 't' && n[1] == 'e' && n[2] == 'r' && n[3] == 'm' &&
        n[4] == '\0')
        return LP_ICON_TERMINAL;
    if (n[0] == 'm' && n[1] == '4' && n[2] == 's' && n[3] == 'h')
        return LP_ICON_TERMINAL;
    if (n[0] == 'f' && n[1] == 'm' && n[2] == '\0')
        return LP_ICON_WINDOW;
    if (n[0] == 'p' && n[1] == 'a' && n[2] == 's' && n[3] == 's' &&
        n[4] == 'w' && n[5] == 'd')
        return LP_ICON_GEAR;
    if (n[0] == 'u' && n[1] == 's' && n[2] == 'e' && n[3] == 'r')
        return LP_ICON_GEAR;
    if (n[0] == 'p' && n[1] == 'a' && n[2] == 's' && n[3] == 's')
        return LP_ICON_GEAR;
    return LP_ICON_FILE;
}

/* Scan /bin and /usr/bin (dedup — /bin is a symlink to /usr/bin).
 * '.'-prefixed entries are skipped per spec. */
static void lp_scan(void)
{
    static const char *dirs[] = { "/bin", "/usr/bin" };
    lp_app_count = 0;
    for (int d = 0; d < 2 && lp_app_count < LP_MAX_APPS; d++) {
        int fd = musr_sc_open((char *)dirs[d], O_RDONLY);
        if (fd < 0)
            continue;
        struct dirent dbuf[48];
        int n = musr_sc_getdents(fd, dbuf, 48);
        musr_sc_close(fd);
        if (n <= 0)
            continue;
        if (n > 48)
            n = 48;
        for (int i = 0; i < n && lp_app_count < LP_MAX_APPS; i++) {
            const char *nm = dbuf[i].name;
            if (nm[0] == '.' || dbuf[i].type == 2 /* dir */)
                continue;
            if (lp_name_hidden(nm))
                continue;
            /* dedup against earlier entries */
            int dup = 0;
            for (int k = 0; k < lp_app_count; k++) {
                const char *a = lp_apps[k].name, *b = nm;
                while (*a && *b && *a == *b) { a++; b++; }
                if (!*a && !*b) { dup = 1; break; }
            }
            if (dup)
                continue;
            /* copy name (clamped) */
            int j = 0;
            while (nm[j] && j < 15) {
                lp_apps[lp_app_count].name[j] = nm[j];
                j++;
            }
            lp_apps[lp_app_count].name[j] = '\0';
            /* path: /bin/<name> */
            lp_apps[lp_app_count].path[0] = '/';
            lp_apps[lp_app_count].path[1] = 'b';
            lp_apps[lp_app_count].path[2] = 'i';
            lp_apps[lp_app_count].path[3] = 'n';
            lp_apps[lp_app_count].path[4] = '/';
            int p = 5;
            for (int q = 0; nm[q] && p < 39; q++, p++)
                lp_apps[lp_app_count].path[p] = nm[q];
            lp_apps[lp_app_count].path[p] = '\0';
            lp_apps[lp_app_count].icon = lp_icon_kind(nm);
            lp_app_count++;
        }
    }
    /* Sort: user-facing apps first (fm = file manager, terminal,
     * m4sh), then alphabetical.  The raw getdents order put altr
     * (a CLI helper) at desktop cell 0, so double-clicking the first
     * desktop icon launched the wrong app entirely. */
    for (int a = 1; a < lp_app_count; a++) {
        struct lp_app tmp = lp_apps[a];
        int b = a - 1;
        while (b >= 0 && lp_app_prio(&lp_apps[b]) > lp_app_prio(&tmp)) {
            lp_apps[b + 1] = lp_apps[b];
            b--;
        }
        lp_apps[b + 1] = tmp;
    }
}

/* Priority for desktop/launchpad ordering (lower = closer to front).
 * 0: fm (file manager — the icon users actually look for)
 * 1: terminal family
 * 2: everything else (alphabetical via stable insertion) */
static int lp_app_prio(const struct lp_app *a)
{
    const char *n = a->name;
    if (n[0] == 'f' && n[1] == 'm' && n[2] == '\0')
        return 0;
    if (n[0] == 't' && n[1] == 'e' && n[2] == 'r' && n[3] == 'm')
        return 1;
    if (n[0] == 'm' && n[1] == '4' && n[2] == 's' && n[3] == 'h')
        return 1;
    return 2;
}

#define LP_COLS     4
#define LP_CELL_W   160
#define LP_CELL_H   110
#define LP_GRID_X   ((LP_W - LP_COLS * LP_CELL_W) / 2)
#define LP_GRID_Y   60

int sprach_create_launchpad(struct sprach_ctx *ctx)
{
    if (ctx->lp_slot >= 0)
        return 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (!ctx->shm->surfaces[i].in_use) {
            struct copland_surface *s =
                &ctx->shm->surfaces[i];
            s->x = 0;
            s->y = MENUBAR_H;
            s->w = LP_W;
            s->h = LP_H;
            s->color = 0x00102040;
            s->flags = 0 /* start hidden: no VISIBLE flag */;
            s->dmg_w = 0;
            s->buffer_ptr = (uint32_t)(uintptr_t)lp_buf;
            s->in_use = 1;      /* publish LAST (same reason as
                                 * sprach_create_app_menu) */
            ctx->lp_slot = i;
            return 0;
        }
    }
    return -1;
}

void sprach_draw_launchpad(struct sprach_ctx *ctx)
{
    if (ctx->lp_slot < 0 || !ctx->lp_open)
        return;
    /* translucent-ish dark backdrop */
    for (int i = 0; i < LP_W * LP_H; i++)
        lp_buf[i] = 0x00C8203048;
    sp_draw_str(lp_buf, LP_W, LP_H, (LP_W - 7 * 9) / 2, 20, "Launchpad",
                0x00FFFFFF);
    for (int a = 0; a < lp_app_count; a++) {
        int cx = LP_GRID_X + (a % LP_COLS) * LP_CELL_W;
        int cy = LP_GRID_Y + (a / LP_COLS) * LP_CELL_H;
        int sel = (ctx->mouse_x >= cx && ctx->mouse_x < cx + LP_CELL_W &&
                   ctx->mouse_y - MENUBAR_H >= cy &&
                   ctx->mouse_y - MENUBAR_H < cy + LP_CELL_H);
        if (sel)
            sp_rect(lp_buf, LP_W, LP_H, cx + 8, cy + 8, LP_CELL_W - 16,
                    LP_CELL_H - 16, 0x004060A0);
        /* icon from the shared icons.c set (32x32 ARGB, centered) */
        {
            uint32_t *ic = lp_icon_for(&lp_apps[a]);
            int ix = cx + (LP_CELL_W - 32) / 2;
            int iy = cy + 14;
            for (int yy = 0; yy < 32; yy++)
                for (int xx = 0; xx < 32; xx++) {
                    uint32_t px = ic[yy * 32 + xx];
                    if (!(px >> 24))
                        continue;   /* transparent */
                    lp_buf[(iy + yy) * LP_W + ix + xx] = px;
                }
        }
        /* name centered under the icon */
        int nl = 0;
        for (const char *p = lp_apps[a].name; *p; p++)
            nl++;
        sp_draw_str(lp_buf, LP_W, LP_H, cx + (LP_CELL_W - nl * 6) / 2,
                    cy + 64, lp_apps[a].name,
                    sel ? 0x00FFFFFF : 0x00D0D0D0);
    }

    ctx->shm->dirty = 1;
}

/* Launch the app under the launchpad click point. */
static void sprach_launchpad_activate(struct sprach_ctx *ctx)
{
    int lx = ctx->mouse_x;
    int ly = ctx->mouse_y - MENUBAR_H;
    for (int a = 0; a < lp_app_count; a++) {
        int cx = LP_GRID_X + (a % LP_COLS) * LP_CELL_W;
        int cy = LP_GRID_Y + (a / LP_COLS) * LP_CELL_H;
        if (lx >= cx && lx < cx + LP_CELL_W &&
            ly >= cy && ly < cy + LP_CELL_H) {
            ser_puts("[SPRACH] launchpad: launching ");
            ser_puts(lp_apps[a].path);
            ser_puts("\n");
            if (lp_apps[a].path[5] == 't' && lp_apps[a].path[6] == 'e' &&
                lp_apps[a].path[7] == 'r' && lp_apps[a].path[8] == 'm' &&
                lp_apps[a].path[9] == '\0') {
                /* /bin/terminal: the WM-owned emulator */
                sprach_spawn_terminal(ctx);
            } else if (lp_apps[a].path[5] == 'm' && lp_apps[a].path[6] == '4' &&
                       lp_apps[a].path[7] == 's' && lp_apps[a].path[8] == 'h' &&
                       lp_apps[a].path[9] == '\0') {
                /* /bin/m4sh: the console shell would hog the raw
                 * console — launch the graphical shell instead */
                int pid = musr_sc_fork();
                if (pid == 0) {
                    m4k_spawn("/bin/m4shg", 0);
                    m4k_exit(1);
                }
            } else {
                int pid = musr_sc_fork();
                if (pid == 0) {
                    m4k_spawn(lp_apps[a].path, 0);
                    m4k_exit(1);
                }
            }
            ctx->lp_open = 0;
            if (ctx->lp_slot >= 0)
                ctx->shm->surfaces[ctx->lp_slot].flags &=
                    ~COPLAND_SURF_VISIBLE;
            ctx->shm->dirty = 1;
            return;
        }
    }
    /* background click closes the launchpad */
    ctx->lp_open = 0;
    if (ctx->lp_slot >= 0)
        ctx->shm->surfaces[ctx->lp_slot].flags &= ~COPLAND_SURF_VISIBLE;
    ctx->shm->dirty = 1;
}

/* ==== DESKTOP ICONS ===================================================
 * A single surface covering the work area, painted with the active
 * wallpaper theme gradient as its background (the legacy compositor
 * has no per-pixel alpha, so the icon layer "pretends" to be part of
 * the wallpaper).  Icons are laid out in a grid on the left; a click
 * on an icon spawns the app, exactly like Launchpad entries.  The
 * layer always stays at the BOTTOM of the z-order (excluded from
 * raise swaps). */

#define DESK_ICON_MAX   12
#define DESK_ICON_COLS  3
#define DESK_CELL_W     76
#define DESK_CELL_H     76
#define DESK_GRID_X     16
#define DESK_GRID_Y     (MENUBAR_H + 16)
static uint32_t desk_buf[SCREEN_W * WORK_AREA_H];
static int desk_slot = -1;
static int desk_count = 0;
/* Reuse the launchpad app table: desk icons are simply the first
 * DESK_ICON_MAX launchable apps. */

static void sprach_desktop_paint(struct sprach_ctx *ctx)
{
    if (desk_slot < 0)
        return;
    /* Background: the active wallpaper theme gradient, matching the
     * compositor's full-screen math (top at screen y=0) so the layer
     * blends seamlessly with the exposed wallpaper around it. */
    uint32_t theme = shm_wallpaper_theme();
    static const struct { uint32_t top, bot; } th[] = {
        { 0x00000044u, 0x0066FFu }, { 0x00224488u, 0x0044AACCu },
        { 0x00331166u, 0x00AA44CCu }, { 0x001A1A2Eu, 0x00444466u },
        { 0x00663300u, 0x00CCAA33u }, { 0x00104010u, 0x0030A030u },
    };
    if (theme >= 6)
        theme = 0;
#ifdef USE_ZIG_DRAW
    /* Zig row-LUT gradient: one bulk fill per scanline. */
    zsp_gradient(desk_buf, SCREEN_W, WORK_AREA_H, th[theme].top,
                 th[theme].bot, WORK_AREA_Y, SCREEN_H - 1);
#else
    uint32_t top = th[theme].top, bot = th[theme].bot;
    for (int y = 0; y < WORK_AREA_H; y++) {
        /* global screen scanline = WORK_AREA_Y + y */
        uint32_t f = (uint32_t)(WORK_AREA_Y + y) * 0xFFu
                     / (uint32_t)(SCREEN_H - 1);
        uint32_t c = (((top & 0xFF0000u) * (0xFF - f)
                       + (bot & 0xFF0000u) * f) >> 8) & 0xFF0000u;
        c |= (((top & 0xFF00u) * (0xFF - f)
               + (bot & 0xFF00u) * f) >> 8) & 0xFF00u;
        c |= (((top & 0xFFu) * (0xFF - f)
               + (bot & 0xFFu) * f) >> 8) & 0xFFu;
        uint32_t *row = &desk_buf[y * SCREEN_W];
        for (int x = 0; x < SCREEN_W; x++)
            row[x] = c;
    }
#endif

    /* Icon grid */
    for (int a = 0; a < desk_count && a < DESK_ICON_MAX; a++) {
        int cx = DESK_GRID_X + (a % DESK_ICON_COLS) * DESK_CELL_W;
        int cy = DESK_GRID_Y - MENUBAR_H + (a / DESK_ICON_COLS) * DESK_CELL_H;
        if (cy + DESK_CELL_H > WORK_AREA_H)
            break;
        int sel = (ctx->mouse_x >= cx && ctx->mouse_x < cx + DESK_CELL_W &&
                   ctx->mouse_y >= cy + MENUBAR_H &&
                   ctx->mouse_y < cy + MENUBAR_H + DESK_CELL_H);
        if (sel)
            sp_rect(desk_buf, SCREEN_W, WORK_AREA_H, cx + 4, cy + 4,
                    DESK_CELL_W - 8, DESK_CELL_H - 8, 0x4060A000);
        uint32_t *ic = lp_icon_for(&lp_apps[a]);
        int ix = cx + (DESK_CELL_W - 32) / 2;
        int iy = cy + 6;
#ifdef USE_ZIG_DRAW
        zsp_icon_blit32(desk_buf, SCREEN_W, ix, iy, ic);
#else
        for (int yy = 0; yy < 32; yy++)
            for (int xx = 0; xx < 32; xx++) {
                uint32_t px = ic[yy * 32 + xx];
                if (!(px >> 24))
                    continue;
                desk_buf[(iy + yy) * SCREEN_W + ix + xx] = px;
            }
#endif
        int nl = 0;
        for (const char *p = lp_apps[a].name; *p; p++)
            nl++;
        sp_draw_str(desk_buf, SCREEN_W, WORK_AREA_H,
                    cx + (DESK_CELL_W - nl * 6) / 2, cy + 44,
                    lp_apps[a].name, sel ? 0x00FFFFFF : 0x00E8E8E8);
    }
    ctx->shm->surfaces[desk_slot].dmg_x = 0;
    ctx->shm->surfaces[desk_slot].dmg_y = WORK_AREA_Y;
    ctx->shm->surfaces[desk_slot].dmg_w = SCREEN_W;
    ctx->shm->surfaces[desk_slot].dmg_h = WORK_AREA_H;
    ctx->shm->dirty = 1;
}

static int sprach_desktop_create(struct sprach_ctx *ctx)
{
    if (desk_slot >= 0)
        return 0;
    lp_scan();
    desk_count = lp_app_count;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (!ctx->shm->surfaces[i].in_use) {
            struct copland_surface *s = &ctx->shm->surfaces[i];
            s->x = 0;
            s->y = WORK_AREA_Y;
            s->w = SCREEN_W;
            s->h = WORK_AREA_H;
            s->color = 0x00000044;
            s->flags = COPLAND_SURF_VISIBLE;
            s->dmg_w = 0;
            s->buffer_ptr = (uint32_t)(uintptr_t)desk_buf;
            s->in_use = 1;
            desk_slot = i;
            sprach_desktop_paint(ctx);
            return 0;
        }
    }
    return -1;
}

/* Launch the app under a desktop-icon click (work-area coords). */
static int sprach_desktop_click(struct sprach_ctx *ctx)
{
    if (desk_slot < 0)
        return 0;
    for (int a = 0; a < desk_count && a < DESK_ICON_MAX; a++) {
        int cx = DESK_GRID_X + (a % DESK_ICON_COLS) * DESK_CELL_W;
        int cy = DESK_GRID_Y + (a / DESK_ICON_COLS) * DESK_CELL_H;
        if (ctx->mouse_x >= cx && ctx->mouse_x < cx + DESK_CELL_W &&
            ctx->mouse_y >= cy && ctx->mouse_y < cy + DESK_CELL_H) {
            ser_puts("[SPRACH] desktop icon: launching ");
            ser_puts(lp_apps[a].path);
            ser_puts("\n");
            if (lp_apps[a].path[5] == 't' && lp_apps[a].path[6] == 'e' &&
                lp_apps[a].path[7] == 'r' && lp_apps[a].path[8] == 'm' &&
                lp_apps[a].path[9] == '\0') {
                sprach_spawn_terminal(ctx);
            } else if (lp_apps[a].path[5] == 'm' && lp_apps[a].path[6] == '4' &&
                       lp_apps[a].path[7] == 's' && lp_apps[a].path[8] == 'h' &&
                       lp_apps[a].path[9] == '\0') {
                int pid = musr_sc_fork();
                if (pid == 0) {
                    m4k_spawn("/bin/m4shg", 0);
                    m4k_exit(1);
                }
            } else {
                int pid = musr_sc_fork();
                if (pid == 0) {
                    m4k_spawn(lp_apps[a].path, 0);
                    m4k_exit(1);
                }
            }
            return 1;
        }
    }
    return 0;
}

/* ==== VIRTUAL DESKTOPS ================================================
 * SPRACH_DESKTOPS workspaces.  Each window (and the terminal) carries
 * the desktop it was opened on; switching only toggles surface
 * visibility — nothing is destroyed. */

/* Toggle helpers used by keyboard (Esc) and click handlers. */
void sprach_app_menu_toggle(struct sprach_ctx *ctx, int open)
{
    if (ctx->menu_slot < 0)
        return;
    ctx->menu_open = open;
    if (open) {
        ctx->shm->surfaces[ctx->menu_slot].flags |=
            COPLAND_SURF_VISIBLE;
        /* Raise above client windows (low slot would hide it). */
        sprach_raise_surface(ctx, ctx->menu_slot);
        sprach_draw_app_menu(ctx);
        ser_puts("[SPRACH] app menu open\n");
    } else {
        ctx->shm->surfaces[ctx->menu_slot].flags &=
            ~COPLAND_SURF_VISIBLE;
        ser_puts("[SPRACH] app menu closed\n");
    }
    ctx->shm->dirty = 1;
}

void sprach_launchpad_toggle(struct sprach_ctx *ctx, int open)
{
    if (ctx->lp_slot < 0)
        return;
    ctx->lp_open = open;
    if (open) {
        lp_scan();
        ctx->lp_count = lp_app_count;
        ctx->shm->surfaces[ctx->lp_slot].flags |=
            COPLAND_SURF_VISIBLE;
        /* Raise above every client window — DIAGNOSTIC: still disabled,
         * bisecting the post-open EXC (EIP lands inside lp_buf).
         * draw stays ENABLED this round. */
        /* sprach_raise_surface(ctx, ctx->lp_slot); */
        sprach_draw_launchpad(ctx);
        ser_puts("[SPRACH] launchpad open\n");
    } else {
        ctx->shm->surfaces[ctx->lp_slot].flags &=
            ~COPLAND_SURF_VISIBLE;
        ser_puts("[SPRACH] launchpad closed\n");
    }
    ctx->shm->dirty = 1;
}

/* Rule+N: activate the Nth Dock entry (0-based idx).  Dock layout is
 * [windows...][terminal icon][launchpad grid icon] from DOCK_PAD. */
void sprach_dock_activate(struct sprach_ctx *ctx, int idx)
{
    int seen = 0;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot < 0)
            continue;
        if (seen == idx) {
            if (ctx->wins[i].hidden) {
                ctx->wins[i].hidden = 0;
                ctx->shm->surfaces[ctx->wins[i].slot].flags |=
                    COPLAND_SURF_VISIBLE;
            }
            ctx->active = i;
            sprach_raise_window(ctx, i);
            ctx->last_tbar_active = -999;
            ctx->shm->dirty = 1;
            ser_puts("[SPRACH] Rule+N: window ");
            print_u32((uint32_t)i);
            ser_puts("\n");
            return;
        }
        seen++;
    }
    if (ctx->term_slot >= 0 && seen == idx) {
        if (ctx->term_hidden) {
            ctx->term_hidden = 0;
            ctx->shm->surfaces[ctx->term_slot].flags |=
                COPLAND_SURF_VISIBLE;
        }
        ctx->active = -1;
        sprach_raise_surface(ctx, ctx->term_slot);
        ctx->last_tbar_active = -999;
        ctx->shm->dirty = 1;
        ser_puts("[SPRACH] Rule+N: terminal\n");
        return;
    }
    if (seen == idx) {   /* launchpad grid icon slot */
        sprach_launchpad_toggle(ctx, 1);
        return;
    }
    ser_puts("[SPRACH] Rule+N: no such dock entry\n");
}

void sprach_switch_desktop(struct sprach_ctx *ctx, int desk)
{
    if (desk < 0 || desk >= SPRACH_DESKTOPS || desk == ctx->desktop_idx)
        return;
    ctx->desktop_idx = desk;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot < 0)
            continue;
        int vis = (ctx->wins[i].desktop == desk) && !ctx->wins[i].hidden;
        if (vis)
            ctx->shm->surfaces[ctx->wins[i].slot].flags |=
                COPLAND_SURF_VISIBLE;
        else
            ctx->shm->surfaces[ctx->wins[i].slot].flags &=
                ~COPLAND_SURF_VISIBLE;
    }
    if (ctx->term_slot >= 0) {
        int vis = (ctx->term_desktop == desk) && !ctx->term_hidden;
        if (vis)
            ctx->shm->surfaces[ctx->term_slot].flags |=
                COPLAND_SURF_VISIBLE;
        else
            ctx->shm->surfaces[ctx->term_slot].flags &=
                ~COPLAND_SURF_VISIBLE;
    }
    ctx->last_tbar_active = -999;   /* dock indicator follows */
    sprach_draw_menubar(ctx);
    ctx->shm->dirty = 1;
    ser_puts("[SPRACH] desktop ");
    print_u32((uint32_t)(desk + 1));
    ser_puts("\n");
}

/* Activate the Nth dock entry (windows left-to-right, then terminal,
 * then launcher): Rule+N / Alt+N. */
static void sprach_dock_activate_index(struct sprach_ctx *ctx, int n)
{
    int seen = 0;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot < 0)
            continue;
        if (seen == n) {
            if (ctx->wins[i].desktop != ctx->desktop_idx)
                sprach_switch_desktop(ctx, ctx->wins[i].desktop);
            if (ctx->wins[i].hidden) {
                ctx->wins[i].hidden = 0;
                ctx->shm->surfaces[ctx->wins[i].slot].flags |=
                    COPLAND_SURF_VISIBLE;
            }
            ctx->active = i;
            sprach_raise_window(ctx, i);
            ctx->last_tbar_active = -999;
            ctx->shm->dirty = 1;
            return;
        }
        seen++;
    }
    if (seen == n && ctx->term_slot >= 0) {
        if (ctx->term_desktop != ctx->desktop_idx)
            sprach_switch_desktop(ctx, ctx->term_desktop);
        if (ctx->term_hidden) {
            ctx->term_hidden = 0;
            ctx->shm->surfaces[ctx->term_slot].flags |=
                COPLAND_SURF_VISIBLE;
        }
        ctx->active = -1;
        sprach_raise_surface(ctx, ctx->term_slot);
        ctx->last_tbar_active = -999;
        ctx->shm->dirty = 1;
    }
}

/* ── Z-order: raise a surface slot to the top of the window stack ──
 * Excludes the menubar and taskbar slots; any other in-use surface is
 * a window and may swap.  Used for both Sprach-owned windows and the
 * terminal client surface. */

void sprach_raise_surface(struct sprach_ctx *ctx, int slot)
{
    if (slot < 0 || slot >= COPLAND_MAX_SURFACES)
        return;
    if (!ctx->shm->surfaces[slot].in_use)
        return;

    int top = -1;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use &&
            i != ctx->taskbar_slot &&
            i != ctx->menubar_slot &&
            i != desk_slot)
            top = i;
    if (top < 0 || top == slot)
        return;

    /* Track slot changes in our own window table so subsequent chrome
     * hit-tests and paints use the new slots. */
    struct sprach_window *other = NULL;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++)
        if (ctx->wins[i].slot == top) {
            other = &ctx->wins[i];
            break;
        }

    struct copland_surface tmp = ctx->shm->surfaces[top];
    ctx->shm->surfaces[top] = ctx->shm->surfaces[slot];
    ctx->shm->surfaces[slot] = tmp;

    /* Keep our window-table slots in sync with the swapped surfaces. */
    int old_slot = slot;
    if (other)
        other->slot = old_slot;
    if (ctx->term_slot == slot)
        ctx->term_slot = top;          /* the terminal itself was raised */
    else if (ctx->term_slot == top)
        ctx->term_slot = old_slot;     /* a window displaced the terminal */
    /* Chrome popup surfaces swap too — keep their slots in sync or
     * later VISIBLE/draw calls hit the displaced surface. */
    if (ctx->clock_slot == slot)
        ctx->clock_slot = top;
    else if (ctx->clock_slot == top)
        ctx->clock_slot = old_slot;
    if (ctx->menu_slot == slot)
        ctx->menu_slot = top;
    else if (ctx->menu_slot == top)
        ctx->menu_slot = old_slot;
    if (ctx->lp_slot == slot)
        ctx->lp_slot = top;
    else if (ctx->lp_slot == top)
        ctx->lp_slot = old_slot;
}

void sprach_raise_window(struct sprach_ctx *ctx, int idx)
{
    struct sprach_window *w = &ctx->wins[idx];
    if (w->slot < 0)
        return;
    int slot = w->slot;
    if (slot < 0 || slot >= COPLAND_MAX_SURFACES)
        return;
    if (!ctx->shm->surfaces[slot].in_use)
        return;

    /* Mirror raise_surface's top-slot scan: if the raise actually
     * swaps, OUR surface ends up at `top` — but raise_surface cannot
     * re-slot us because it doesn't know which window we are.  Leaving
     * w->slot stale would make later MOVE/MIN/CLOSE act on the
     * displaced (someone else's) surface. */
    int top = -1;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use &&
            i != ctx->taskbar_slot &&
            i != ctx->menubar_slot &&
            i != desk_slot)
            top = i;

    sprach_raise_surface(ctx, slot);
    if (top >= 0 && top != slot)
        w->slot = top;
}

/* ── Mouse input ──
 *
 * Cursor-only moves: call m4k_update_cursor() (kernel-level hardware
 * cursor, NO flip needed).  Button actions: set shm->dirty = 1
 * individually to trigger exactly one Copland composite. */

void sprach_handle_mouse(struct sprach_ctx *ctx)
{
    struct m4k_mouse_event ev;
    int cursor_moved = 0;

    while (m4k_get_mouse_event(&ev)) {
        /* Raw deltas are consumed here only for the moved/click edge
         * detection; the ABSOLUTE position comes from the kernel mouse
         * driver (single source of truth — 2x ballistics + screen
         * clamp applied there), so hit-testing can never drift from
         * the visible cursor. */
        if (ev.dx || ev.dy)
            cursor_moved = 1;

        if (ev.buttons & 1) {
            /* Edge-trigger: fire the click only on the press edge, not on
             * every event that still carries the held button (trailing
             * move events would otherwise double-fire toggle actions). */
            if (!ctx->btn_was_down) {
                m4k_get_mouse_pos(&ctx->mouse_x, &ctx->mouse_y);
                sprach_handle_click(ctx);
            }
            ctx->btn_was_down = 1;
        } else {
            ctx->btn_was_down = 0;
        }
    }

    /* Keep the cached absolute position fresh for painting/hover even
     * when no click happened. */
    if (cursor_moved)
        m4k_get_mouse_pos(&ctx->mouse_x, &ctx->mouse_y);

    if (cursor_moved)
        m4k_update_cursor();
}

/* ── Terminal client window ──
 *
 * Ctrl+Alt+T (detected in the keyboard path) forks a child that execs
 * /bin/terminal at 0xC00000 — a separate flat-address-space process
 * whose code/data never overlap Sprach's 0x1100000 zone, mirroring how
 * Copland (0x600000) forks Sprach (0x1100000).  The terminal owns its
 * surface's pixel buffer and paints both chrome and the 80×25 grid.
 * Sprach only tracks the slot, routes chrome clicks, raises it in
 * z-order and forwards keystrokes through the term_mailbox ring. */

void sprach_spawn_terminal(struct sprach_ctx *ctx)
{
    if (ctx->term_slot >= 0) {
        ser_puts("[SPRACH] terminal already running\n");
        return;
    }
    /* A stale pid whose child died before registering its surface would
     * block relaunch forever; allow a retry once the spawn timeouts. */
    if (ctx->term_pid > 0) {
        ser_puts("[SPRACH] terminal pid stale (retrying)\n");
        m4k_kill(ctx->term_pid, 2 /* SIGKILL */);
        ctx->term_pid = -1;
    }
    ser_puts("[SPRACH] Ctrl+Alt+T: launching /bin/terminal...\n");
    int pid = musr_sc_fork();
    if (pid == 0) {
        /* Child: become the terminal emulator */
        int r = m4k_spawn("/bin/terminal", 0);
        ser_puts("[SPRACH] terminal spawn failed (ret=");
        print_u32((uint32_t)r);
        ser_puts(")\n");
        m4k_exit(1);
    }
    if (pid < 0) {
        ser_puts("[SPRACH] terminal fork failed\n");
        return;
    }
    ctx->term_pid = pid;
    ctx->term_slot = -1;
    ctx->term_hidden = 0;
    ctx->term_maximized = 0;
    ctx->term_spawn_tick = ctx->tick;
    ser_puts("[SPRACH] terminal pid=");
    print_u32((uint32_t)pid);
    ser_puts("\n");
}

/* Find the terminal's surface: the mailbox announces the process, the
 * surface is the first in-use slot Sprach does not own that is not the
 * taskbar/menubar.  Called every frame; cheap scan over 16 slots. */
void sprach_poll_terminal(struct sprach_ctx *ctx)
{
    struct term_mailbox *mb = (struct term_mailbox *)TERM_MAILBOX_BASE;
    if (ctx->term_slot >= 0) {
        /* Terminal was killed / closed: its surface vanishes */
        if (!ctx->shm->surfaces[ctx->term_slot].in_use) {
            ser_puts("[SPRACH] terminal surface gone\n");
            ctx->term_slot = -1;
            ctx->term_pid = -1;
            if (ctx->active < 0)
                ctx->active = -1;
        }
        return;
    }
    if (mb->magic != TERM_MAILBOX_MAGIC)
        return;   /* no terminal process yet */

    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (!ctx->shm->surfaces[i].in_use)
            continue;
        if (i == ctx->taskbar_slot || i == ctx->menubar_slot)
            continue;
        if (i == ctx->clock_slot || i == ctx->menu_slot ||
            i == ctx->lp_slot)
            continue;   /* our own popup surfaces */
        int ours = 0;
        for (int j = 0; j < SPRACH_WINDOW_COUNT; j++)
            if (ctx->wins[j].slot == i)
                ours = 1;
        if (ours)
            continue;
        /* A surface with a pixel buffer that Sprach did not create.
         * Guard against transient half-created surfaces from other
         * clients: require a plausible window size. */
        if (ctx->shm->surfaces[i].w < 50 ||
            ctx->shm->surfaces[i].h < 50)
            continue;
        ctx->term_slot = i;
        ctx->term_hidden = 0;
        ctx->term_maximized = 0;
        /* The terminal belongs to the desktop it was opened on —
         * switching away must hide it (was hard-pinned to 0, so the
         * terminal showed on every desktop). */
        ctx->term_desktop = ctx->desktop_idx;
        ctx->term_normal_x = ctx->shm->surfaces[i].x;
        ctx->term_normal_y = ctx->shm->surfaces[i].y;
        ctx->term_normal_w = ctx->shm->surfaces[i].w;
        ctx->term_normal_h = ctx->shm->surfaces[i].h;
        ser_puts("[SPRACH] terminal window registered (slot=");
        print_u32((uint32_t)i);
        ser_puts(")\n");
        /* Raise it to the top immediately: the terminal is the newest
         * window and must composite above the demo windows (Copland
         * renders by slot order; without this the demo windows stay
         * on top of the freshly registered terminal surface). */
        ctx->active = -1;
        sprach_raise_surface(ctx, i);
        ctx->last_tbar_active = -999;
        ctx->shm->dirty = 1;
        return;
    }
}

/* Forward one keystroke to the terminal process.  Only called when the
 * terminal window is active (ctx->active < 0) and not minimized. */
static void sprach_terminal_key(struct sprach_ctx *ctx, unsigned char ch)
{
    struct term_mailbox *mb = (struct term_mailbox *)TERM_MAILBOX_BASE;
    if (mb->magic != TERM_MAILBOX_MAGIC)
        return;
    uint32_t next = (mb->write_idx + 1) % TERM_MAILBOX_SIZE;
    if (next == mb->read_idx)
        return;   /* ring full: drop keystroke */
    mb->buf[mb->write_idx] = ch;
    mb->write_idx = next;
}

/* ── File-manager key forwarding ──
 * The FM client (/bin/fm) registers a key mailbox at 0x610000 on
 * startup (same ring protocol as the terminal mailbox; see fm.c).
 * We forward keystrokes when the FM window is the top-most surface.
 * It is identified by its 560-px width — the same convention fm.c
 * itself uses to claim its slot. */
#define FM_MAILBOX_BASE     0x00610000
#define FM_MAILBOX_MAGIC    0x464D4B31u   /* "FMK1" */
#define FM_MAILBOX_SIZE     64
#define FM_SURF_W           560           /* fm.c FM_W */

struct sprach_fm_mailbox {
    uint32_t magic;
    uint32_t write_idx;
    uint32_t read_idx;
    unsigned char buf[FM_MAILBOX_SIZE];
};

/* Forward one keystroke to the FM if it runs and owns the top-most
 * surface.  Returns 1 when the key was consumed (or dropped on a full
 * ring), 0 when the FM should not receive it. */
static int sprach_fm_key(struct sprach_ctx *ctx, unsigned char ch)
{
    volatile struct sprach_fm_mailbox *mb =
        (volatile struct sprach_fm_mailbox *)FM_MAILBOX_BASE;
    if (mb->magic != FM_MAILBOX_MAGIC)
        return 0;                     /* FM not running */

    /* Top-most non-chrome surface must be a foreign FM-sized window */
    int top = -1;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use &&
            i != ctx->taskbar_slot && i != ctx->menubar_slot)
            top = i;
    if (top < 0 || ctx->shm->surfaces[top].w != FM_SURF_W)
        return 0;
    if (top == ctx->term_slot || top == ctx->clock_slot ||
        top == ctx->menu_slot || top == ctx->lp_slot)
        return 0;
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++)
        if (ctx->wins[i].slot == top)
            return 0;

    uint32_t next = (mb->write_idx + 1) % FM_MAILBOX_SIZE;
    if (next == mb->read_idx)
        return 1;                     /* ring full: drop keystroke */
    mb->buf[mb->write_idx] = ch;
    mb->write_idx = next;
    return 1;
}
void sprach_handle_terminal_click(struct sprach_ctx *ctx, int sx, int sy,
                                  int sw, int sh, int lx, int ly)
{
    if (ctx->term_slot < 0)
        return;

    /* Close: tear down the surface and kill the terminal process */
    if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
        lx >= CTRL_CLOSE_X && lx < CTRL_CLOSE_X + CTRL_SIZE) {
        ser_puts("[SPRACH] TERMINAL CLOSE\n");
        ctx->shm->surfaces[ctx->term_slot].in_use = 0;
        if (ctx->shm->surface_count > 0)
            ctx->shm->surface_count--;
        ctx->shm->dirty = 1;
        if (ctx->term_pid > 0)
            m4k_kill(ctx->term_pid, 2 /* SIGKILL */);
        ctx->term_slot = -1;
        ctx->term_pid = -1;
        if (ctx->active < 0) {
            ctx->active = -1;
            for (int j = SPRACH_WINDOW_COUNT - 1; j >= 0; j--)
                if (ctx->wins[j].slot >= 0 && !ctx->wins[j].hidden) {
                    ctx->active = j;
                    break;
                }
        }
        return;
    }

    /* Minimize: hide the surface, keep the taskbar button */
    if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
        lx >= CTRL_MIN_X && lx < CTRL_MIN_X + CTRL_SIZE) {
        ser_puts("[SPRACH] TERMINAL MIN\n");
        ctx->term_hidden = 1;
        ctx->shm->surfaces[ctx->term_slot].flags &=
            ~COPLAND_SURF_VISIBLE;
        ctx->shm->dirty = 1;
        if (ctx->active < 0) {
            ctx->active = -1;
            for (int j = SPRACH_WINDOW_COUNT - 1; j >= 0; j--)
                if (ctx->wins[j].slot >= 0 && !ctx->wins[j].hidden) {
                    ctx->active = j;
                    break;
                }
        }
        return;
    }

    /* Maximize / restore: span the whole work area */
    if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
        lx >= CTRL_MAX_X && lx < CTRL_MAX_X + CTRL_SIZE) {
        struct copland_surface *ts = &ctx->shm->surfaces[ctx->term_slot];
        if (ctx->term_maximized) {
            ts->x = ctx->term_normal_x;
            ts->y = ctx->term_normal_y;
            ts->w = ctx->term_normal_w;
            ts->h = ctx->term_normal_h;
            ctx->term_maximized = 0;
            ser_puts("[SPRACH] TERMINAL RESTORE\n");
        } else {
            ctx->term_normal_x = ts->x;
            ctx->term_normal_y = ts->y;
            ctx->term_normal_w = ts->w;
            ctx->term_normal_h = ts->h;
            ts->x = 0;
            ts->y = WORK_AREA_Y;
            ts->w = SCREEN_W;
            ts->h = WORK_AREA_H;
            ctx->term_maximized = 1;
            ser_puts("[SPRACH] TERMINAL MAX\n");
        }
        ctx->shm->dirty = 1;
        return;
    }

    /* Title bar click → activate + raise */
    ctx->active = -1;
    sprach_raise_surface(ctx, ctx->term_slot);
    ctx->shm->dirty = 1;
}

/* ── Click handling ──
 *
 * Fired once per button press edge (the event loop edge-triggers this).
 * Hit-test order: taskbar buttons → terminal chrome → window chrome
 * (topmost first). */

static void sprach_handle_click(struct sprach_ctx *ctx)
{
    int hit = 0;

            /* ── Menubar hit-test: clock area toggles the clock popup ── */
            if (ctx->mouse_y < MENUBAR_H) {
                /* Tray glyphs (left of the clock): open the control
                 * panel for the clicked indicator. */
                {
                    int clock_x0 = SCREEN_W - 8 * 7 - 12;
                    int tray_x0 = clock_x0 - TRAY_W - 8;
                    if (ctx->mouse_x >= tray_x0 &&
                        ctx->mouse_x < clock_x0) {
                        int t = (ctx->mouse_x - tray_x0) / 20;
                        ctx->clock_open = 1;
                        ctx->clock_about = 1;
                        ctx->clock_settings = 0;
                        ctx->tray_mode = t + 1;   /* 1 vol 2 net 3 bt */
                        ctx->clock_x = tray_x0 - 8;
                        if (ctx->clock_x < 0)
                            ctx->clock_x = 0;
                        ctx->clock_y = MENUBAR_H;
                        ctx->clock_last_sec = -1;
                        if (ctx->clock_slot >= 0) {
                            ctx->shm->surfaces[ctx->clock_slot].x =
                                ctx->clock_x;
                            ctx->shm->surfaces[ctx->clock_slot].y =
                                ctx->clock_y;
                            ctx->shm->surfaces[ctx->clock_slot].h =
                                ABOUT_POP_H;
                            ctx->shm->surfaces[ctx->clock_slot].flags |=
                                COPLAND_SURF_VISIBLE;
                            sprach_draw_clock_popup(ctx);
                        }
                        ctx->shm->dirty = 1;
                        hit = 1;
                    }
                }
                int clock_x0 = SCREEN_W - 8 * 7 - 12;
                if (!hit && ctx->mouse_x >= clock_x0) {
                    ctx->clock_open = !ctx->clock_open;
                    ctx->clock_about = 0;   /* time view, not About */
                    ctx->tray_mode = 0;     /* normal clock popup */
                    if (ctx->clock_open) {
                        ctx->clock_x = SCREEN_W - CLOCK_POP_W - 8;
                        ctx->clock_y = MENUBAR_H;
                        ctx->clock_last_sec = -1;
                        if (ctx->clock_slot >= 0) {
                            ctx->shm->surfaces[ctx->clock_slot].flags |=
                                COPLAND_SURF_VISIBLE;
                        }
                        ser_puts("[SPRACH] clock popup open\n");
                    } else {
                        if (ctx->clock_slot >= 0)
                            ctx->shm->surfaces[ctx->clock_slot].flags &=
                                ~COPLAND_SURF_VISIBLE;
                        ser_puts("[SPRACH] clock popup closed\n");
                    }
                    ctx->shm->dirty = 1;
                    hit = 1;
                }

                /* Brand text → toggle the app menu dropdown */
                if (!hit && ctx->mouse_x >= 6 &&
                    ctx->mouse_x < 6 + 5 * 8 + 6) {
                    sprach_app_menu_toggle(ctx, !ctx->menu_open);
                    hit = 1;
                }
            }

            /* Clock popup BODY hit-test (below the menubar — outside
             * the y<MENUBAR_H block; inside it this was dead code:
             * popup y >= MENUBAR_H contradicts the enclosing guard). */
            if (!hit && ctx->clock_open && ctx->clock_slot >= 0 &&
                ctx->mouse_x >= ctx->clock_x &&
                ctx->mouse_x < ctx->clock_x + CLOCK_POP_W &&
                ctx->mouse_y >= ctx->clock_y &&
                ctx->mouse_y < ctx->clock_y + ABOUT_POP_H) {
                /* Settings swatches: apply the clicked theme and
                 * force a full repaint (wallpaper + chrome).
                 * Bluetooth panel: clicking the radio line toggles
                 * the software radio state. */
                int lx = ctx->mouse_x - ctx->clock_x;
                int ly = ctx->mouse_y - ctx->clock_y;
                if (ctx->tray_mode == 3) {
                    if (ly >= 24 && ly < 40) {
                        ctx->bt_on = !ctx->bt_on;
                        sprach_cfg_save(ctx);
                        sprach_draw_menubar(ctx);
                        sprach_draw_clock_popup(ctx);
                        ser_puts(ctx->bt_on
                            ? "[SPRACH] bluetooth radio ON\n"
                            : "[SPRACH] bluetooth radio OFF\n");
                    }
                    hit = 1;
                } else if (ctx->tray_mode == 1) {
                    /* Volume: click "-"/"+" halves to adjust ±10. */
                    if (ly >= 48 && ly < 68) {
                        if (lx >= 10 && lx < 60) {
                            ctx->vol_level -= 10;
                            if (ctx->vol_level < 0)
                                ctx->vol_level = 0;
                        } else if (lx >= CLOCK_POP_W - 60 &&
                                   lx < CLOCK_POP_W - 10) {
                            ctx->vol_level += 10;
                            if (ctx->vol_level > 100)
                                ctx->vol_level = 100;
                        }
                        sprach_cfg_save(ctx);
                        sprach_draw_clock_popup(ctx);
                        ser_puts("[SPRACH] volume -> ");
                        print_u32((uint32_t)ctx->vol_level);
                        ser_puts("\n");
                    }
                    hit = 1;
                } else if (ctx->clock_settings) {
                    if (lx >= 12 && ly >= 108 && ly < 148 &&
                        lx < 12 + 3 * 48) {
                        int t = (lx - 12) / 48;
                        if (ly >= 128)
                            t += 3;
                        if (t >= 0 && t < 6) {
                            gui_wallpaper_set_theme(t);
                            sprach_cfg_save(ctx);
                            ctx->shm->dirty = 1;
                            sprach_desktop_paint(ctx);
                            sprach_draw_clock_popup(ctx);
                            ser_puts("[SPRACH] wallpaper theme -> ");
                            print_u32((uint32_t)t);
                            ser_puts("\n");
                        }
                        hit = 1;
                    } else {
                        hit = 1;   /* swallow other popup clicks */
                    }
                } else {
                    hit = 1;   /* swallow clicks inside the popup */
                }
            }

            /* App-menu items (when the dropdown is open) — also moved
             * out of the y<MENUBAR_H block (items are BELOW it). */
            if (!hit && ctx->menu_open && ctx->menu_slot >= 0 &&
                ctx->mouse_x >= APP_MENU_X &&
                ctx->mouse_x < APP_MENU_X + APP_MENU_W &&
                ctx->mouse_y >= MENUBAR_H &&
                ctx->mouse_y < MENUBAR_H + APP_MENU_H) {
                int item = (ctx->mouse_y - MENUBAR_H) / APP_MENU_ITEM_H;
                if (item >= 0 && item < APP_MENU_ITEMS)
                    sprach_app_menu_activate(ctx, item);
                hit = 1;
            }

            /* Launchpad overlay hit-test (covers the work area) */
            if (!hit && ctx->lp_open && ctx->lp_slot >= 0 &&
                ctx->mouse_x >= 0 && ctx->mouse_x < SCREEN_W &&
                ctx->mouse_y >= MENUBAR_H &&
                ctx->mouse_y < MENUBAR_H + LP_H) {
                sprach_launchpad_activate(ctx);
                hit = 1;
            }

            /* Dock icon hit-test (32x32 icons) */
            if (!hit &&
                ctx->mouse_y >= SCREEN_H - TASKBAR_H && ctx->mouse_y < SCREEN_H) {
                int icon_y = (TASKBAR_H - DOCK_ICON_SIZE) / 2;
                int my = ctx->mouse_y - (SCREEN_H - TASKBAR_H) - icon_y;
                if (my >= 0 && my < DOCK_ICON_SIZE) {
                    /* Far-left launchpad grid icon */
                    if (ctx->mouse_x >= DOCK_PAD &&
                        ctx->mouse_x < DOCK_PAD + DOCK_ICON_SIZE) {
                        sprach_launchpad_toggle(ctx, !ctx->lp_open);
                        hit = 1;
                    }
                    int bx = DOCK_PAD + DOCK_ICON_PITCH;
                    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
                        if (ctx->wins[i].slot < 0)
                            continue;   /* closed: icon removed */
                        if (ctx->mouse_x >= bx &&
                            ctx->mouse_x < bx + DOCK_ICON_SIZE) {
                            if (ctx->wins[i].hidden) {
                                ctx->wins[i].hidden = 0;
                                ctx->shm->surfaces[ctx->wins[i].slot].flags |=
                                    COPLAND_SURF_VISIBLE;
                            }
                            ctx->active = i;
                            sprach_raise_window(ctx, i);
                            hit = 1;
                            ctx->last_tbar_active = -999;
                            ser_puts("[SPRACH] Dock icon clicked, window ");
                            print_u32((uint32_t)i);
                            ser_puts("\n");
                            break;
                        }
                        bx += DOCK_ICON_PITCH;
                    }
                    /* Terminal window icon */
                    if (!hit && ctx->term_slot >= 0 &&
                        ctx->mouse_x >= bx &&
                        ctx->mouse_x < bx + DOCK_ICON_SIZE) {
                        if (ctx->term_hidden) {
                            ctx->term_hidden = 0;
                            ctx->shm->surfaces[ctx->term_slot].flags |=
                                COPLAND_SURF_VISIBLE;
                        }
                        ctx->active = -1;
                        sprach_raise_surface(ctx, ctx->term_slot);
                        hit = 1;
                        ctx->last_tbar_active = -999;
                        ser_puts("[SPRACH] Dock icon clicked: terminal\n");
                    }
                    /* Far-right launcher icon: spawn or focus terminal */
                    if (!hit) {
                        int lx = SCREEN_W - DOCK_ICON_SIZE - DOCK_PAD;
                        if (ctx->mouse_x >= lx &&
                            ctx->mouse_x < lx + DOCK_ICON_SIZE) {
                            if (ctx->term_slot < 0) {
                                ser_puts("[SPRACH] dock launch: terminal\n");
                                sprach_spawn_terminal(ctx);
                            } else {
                                if (ctx->term_hidden) {
                                    ctx->term_hidden = 0;
                                    ctx->shm->surfaces[ctx->term_slot].flags |=
                                        COPLAND_SURF_VISIBLE;
                                }
                                ctx->active = -1;
                                sprach_raise_surface(ctx, ctx->term_slot);
                                ser_puts("[SPRACH] dock: terminal focused\n");
                            }
                            hit = 1;
                            ctx->last_tbar_active = -999;
                        }
                    }
                }
            }


            /* Terminal window chrome hit-test (client window; typically
             * topmost when raised) */
            if (!hit && ctx->term_slot >= 0 && !ctx->term_hidden) {
                struct copland_surface *ts =
                    &ctx->shm->surfaces[ctx->term_slot];
                if (ctx->mouse_x >= ts->x &&
                    ctx->mouse_x < ts->x + ts->w &&
                    ctx->mouse_y >= ts->y &&
                    ctx->mouse_y < ts->y + TERM_TITLE_H) {
                    sprach_handle_terminal_click(ctx, ts->x, ts->y,
                                                 ts->w, ts->h,
                                                 ctx->mouse_x - ts->x,
                                                 ctx->mouse_y - ts->y);
                    hit = 1;
                }
            }

            /* Window chrome hit-test (topmost window first) */
            if (!hit) {
                for (int i = SPRACH_WINDOW_COUNT - 1; i >= 0; i--) {
                    struct sprach_window *w = &ctx->wins[i];
                    if (w->slot < 0 || w->hidden)
                        continue;
                    int cw = w->maximized ? SCREEN_W : w->w;

                    if (ctx->mouse_x >= w->x && ctx->mouse_x < w->x + cw &&
                        ctx->mouse_y >= w->y && ctx->mouse_y < w->y + SPRACH_TITLE_H) {
                        int lx = ctx->mouse_x - w->x;
                        int ly = ctx->mouse_y - w->y;

                        /* Close */
                        if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
                            lx >= CTRL_CLOSE_X && lx < CTRL_CLOSE_X + CTRL_SIZE) {
                            w->btn_clicked = 1;
                            w->click_tick = ctx->tick;
                            ctx->shm->surfaces[w->slot].in_use = 0;
                            if (ctx->shm->surface_count > 0)
                                ctx->shm->surface_count--;
                            ctx->shm->dirty = 1;
                            w->slot = -1;
                            ser_puts("[SPRACH] CLOSE ");
                            print_u32((uint32_t)i);
                            ser_puts("\n");
                            if (ctx->active == i) {
                                ctx->active = -1;
                                for (int j = SPRACH_WINDOW_COUNT - 1;
                                     j >= 0; j--) {
                                    if (ctx->wins[j].slot >= 0 &&
                                        !ctx->wins[j].hidden) {
                                        ctx->active = j;
                                        break;
                                    }
                                }
                            }
                            hit = 1;
                            break;
                        }
                        /* Minimize */
                        if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
                            lx >= CTRL_MIN_X && lx < CTRL_MIN_X + CTRL_SIZE) {
                            w->btn_clicked = 2;
                            w->click_tick = ctx->tick;
                            w->hidden = 1;
                            ctx->shm->surfaces[w->slot].flags &= ~COPLAND_SURF_VISIBLE;
                            ctx->shm->dirty = 1;
                            ser_puts("[SPRACH] MIN ");
                            print_u32((uint32_t)i);
                            ser_puts("\n");
                            hit = 1;
                            break;
                        }
                        /* Maximize / Restore */
                        if (ly >= CTRL_Y && ly < CTRL_Y + CTRL_SIZE &&
                            lx >= CTRL_MAX_X && lx < CTRL_MAX_X + CTRL_SIZE) {
                            w->btn_clicked = 3;
                            w->click_tick = ctx->tick;
                            if (w->maximized) {
                                int idx = (int)(w - ctx->wins);
                                w->buf = sprach_bufs[idx];
                                w->w = w->normal_w;
                                w->h = w->normal_h;
                                w->x = w->normal_x;
                                w->y = w->normal_y;
                                w->maximized = 0;
                                ser_puts("[SPRACH] RESTORE ");
                                print_u32((uint32_t)i);
                                ser_puts("\n");
                            } else {
                                int idx = (int)(w - ctx->wins);
                                w->normal_x = w->x;
                                w->normal_y = w->y;
                                w->normal_w = w->w;
                                w->normal_h = w->h;
                                w->buf = maximize_bufs[idx];
                                w->w = SCREEN_W;
                                w->h = WORK_AREA_H;
                                w->x = 0;
                                w->y = WORK_AREA_Y;
                                w->maximized = 1;
                                ser_puts("[SPRACH] MAX ");
                                print_u32((uint32_t)i);
                                ser_puts("\n");
                            }
                            /* Keep the surface in sync: Copland blits from
                             * buffer_ptr with the surface geometry.  Paint
                             * into the (possibly brand-new) buffer NOW so a
                             * composite can never read unpainted memory. */
                            ctx->shm->surfaces[w->slot].buffer_ptr =
                                (uint32_t)(uintptr_t)w->buf;
                            ctx->shm->surfaces[w->slot].x = w->x;
                            ctx->shm->surfaces[w->slot].y = w->y;
                            ctx->shm->surfaces[w->slot].w = w->w;
                            ctx->shm->surfaces[w->slot].h = w->h;
                            sprach_paint_window(ctx, w);
                            ctx->shm->dirty = 1;
                            hit = 1;
                            break;
                        }
                        /* Title bar click → raise */
                        ctx->active = i;
                        sprach_raise_window(ctx, i);
                        hit = 1;
                        break;
                    }
                }
            }

            /* FM client window: forward the click (window-local) into
             * the FM mailbox.  Double click = two presses within 25
             * ticks (~0.5 s) inside the FM surface. */
            if (!hit) {
                int top = -1;
                for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
                    if (ctx->shm->surfaces[i].in_use &&
                        i != ctx->taskbar_slot && i != ctx->menubar_slot)
                        top = i;
                if (top >= 0 && ctx->shm->surfaces[top].w == FM_SURF_W &&
                    top != ctx->term_slot && top != ctx->clock_slot &&
                    top != ctx->menu_slot && top != ctx->lp_slot) {
                    struct copland_surface *fs = &ctx->shm->surfaces[top];
                    int lx = ctx->mouse_x - (int)fs->x;
                    int ly = ctx->mouse_y - (int)fs->y;
                    if (lx >= 0 && lx < (int)fs->w &&
                        ly >= 0 && ly < (int)fs->h) {
                        static uint32_t last_fm_click_tick = 0;
                        static int last_fm_click_x = -99, last_fm_click_y = -99;
                        int dbl = (ctx->tick - last_fm_click_tick <= 25) &&
                                  (lx > last_fm_click_x - 6 &&
                                   lx < last_fm_click_x + 6) &&
                                  (ly > last_fm_click_y - 6 &&
                                   ly < last_fm_click_y + 6);
                        last_fm_click_tick = ctx->tick;
                        last_fm_click_x = lx;
                        last_fm_click_y = ly;
                        volatile struct sprach_fm_mailbox *mb =
                            (volatile struct sprach_fm_mailbox *)
                                FM_MAILBOX_BASE;
                        if (mb->magic == FM_MAILBOX_MAGIC) {
                            unsigned char code = dbl ? 0xF2 : 0xF1;
                            for (int b = 0; b < 3; b++) {
                                unsigned char v = b == 0 ? code
                                              : (b == 1 ? (unsigned char)lx
                                                        : (unsigned char)ly);
                                uint32_t next =
                                    (mb->write_idx + 1) % FM_MAILBOX_SIZE;
                                if (next == mb->read_idx)
                                    break;   /* ring full */
                                mb->buf[mb->write_idx] = v;
                                mb->write_idx = next;
                            }
                        }
                        hit = 1;
                    }
                }
            }

            /* Desktop icon hit-test: LAST resort — only clicks that
             * fell through every panel, menu, window and client
             * surface reach the desktop icons (they sit on the
             * wallpaper, under all floating chrome). */
            if (!hit && ctx->mouse_y >= MENUBAR_H &&
                ctx->mouse_y < SCREEN_H - TASKBAR_H) {
                hit = sprach_desktop_click(ctx);
            }
}

/* ── Entry point ── */

void _start(void)
{
    icons_init();
    ser_puts("[SPRACH] ================================\n");
    ser_puts("[SPRACH] Window manager starting (mode: ");
    ser_puts(sprach_mode_name());
    ser_puts(")...\n");
    /* Debug: confirm the mouse mapping on boot.  PS/2 dy > 0 = DOWN
     * (toward the operator); screen Y grows downward, so the raw delta
     * is added (y += dy * speed) — identical to the kernel's own
     * accumulation for the visible hardware cursor. */
    ser_puts("[SPRACH] mouse: y += dy * ");
    print_u32((uint32_t)SPRACH_MOUSE_SPEED);
    ser_puts(", speed ");
    print_u32((uint32_t)SPRACH_MOUSE_SPEED);
    ser_puts("px/step; clicks are edge-triggered\n");
    ser_puts("[SPRACH] ================================\n");

    struct copland_shm *shm = copland_shm_get();

    /* Wait for Copland to initialize the shared region */
    int guard = 0;
    while (!shm->ready) {
        /* m4k_yield only — NEVER m4k_get_keyboard_event(): it consumes
         * the kernel key buffer and steals keys from the login form. */
        m4k_yield();
        if (++guard > 2000000) {
            ser_puts("[SPRACH] timeout waiting for Copland\n");
            m4k_exit(1);
        }
    }

    struct sprach_ctx ctx;
    ctx.shm = shm;
    ctx.tick = 0;
    ctx.active = 0;
    ctx.dir = 1;
    ctx.offs = 0;
    ctx.taskbar_slot = -1;
    ctx.menubar_slot = -1;
    ctx.mouse_x = SCREEN_W / 2;
    ctx.mouse_y = SCREEN_H / 2;
    ctx.btn_was_down = 0;
    ctx.term_slot = -1;
    ctx.term_pid = -1;
    ctx.term_hidden = 0;
    ctx.term_maximized = 0;
    ctx.term_normal_x = 0;
    ctx.term_normal_y = 0;
    ctx.term_normal_w = 0;
    ctx.term_normal_h = 0;
    ctx.last_tbar_active = -999;
    ctx.last_tbar_win = 0xFFFFFFFFu;
    ctx.last_tbar_minute = -1;
    ctx.menu_last_second = -1;
    ctx.clock_open = 0;
    ctx.clock_slot = -1;
    ctx.clock_x = 0;
    ctx.clock_y = MENUBAR_H;
    ctx.clock_last_sec = -1;
    ctx.clock_about = 0;
    ctx.desktop_idx = 0;
    ctx.bt_on = 0;
    ctx.tray_mode = 0;
    ctx.vol_level = 70;
    ctx.menu_open = 0;
    ctx.menu_slot = -1;
    ctx.lp_open = 0;
    ctx.lp_slot = -1;
    ctx.lp_count = 0;
    ctx.term_desktop = 0;

    /* Restore persisted desktop settings (theme/vol/bt) AFTER the
     * defaults above so a saved value wins, but BEFORE the first
     * paint — boot then shows the saved wallpaper immediately. */
    sprach_cfg_load(&ctx);

    sprach_mode_init(&ctx);

    /* Create chrome surfaces: taskbar first, then menubar.
     * Menubar gets higher slot → composited on top by Copland. */
    sp_proto_ready = (sprach_proto_connect() == 0);
    if (sp_proto_ready)
        ser_puts("[SPRACH] WM identity claimed (new protocol)\n");
    else
        ser_puts("[SPRACH] new protocol unavailable, legacy surfaces\n");
    if (sp_proto_ready) {
        /* Chrome on the new protocol: menubar (top) then taskbar
         * (bottom; created last → higher z in the legacy pass too,
         * irrelevant here since these live outside the shm slots). */
        if (sprach_proto_chrome(&sp_id_mb_surf, &sp_id_mb_buf,
                                (uint32_t)(uintptr_t)menubar_buf,
                                0, 0, SCREEN_W, MENUBAR_H) == 0
            && sprach_proto_chrome(&sp_id_tb_surf, &sp_id_tb_buf,
                                   (uint32_t)(uintptr_t)taskbar_buf,
                                   0, SCREEN_H - TASKBAR_H,
                                   SCREEN_W, TASKBAR_H) == 0) {
            ctx.menubar_slot = -2;   /* sentinel: proto-backed */
            ctx.taskbar_slot = -2;
        } else {
            sp_proto_ready = 0;
            ser_puts("[SPRACH] chrome surface creation failed,"
                     " legacy fallback\n");
        }
    }
    if (!sp_proto_ready) {
        if (sprach_create_taskbar(&ctx) != 0)
            ser_puts("[SPRACH] warning: taskbar creation failed\n");
        if (sprach_create_menubar(&ctx) != 0)
            ser_puts("[SPRACH] warning: menubar creation failed\n");
    }
    /* Desktop icon layer FIRST: the legacy compositor paints in slot
     * order, so the lowest slot (created first) is the bottom-most
     * surface — windows opened later always composite above it. */
    if (sprach_desktop_create(&ctx) != 0)
        ser_puts("[SPRACH] warning: desktop icon layer creation failed\n");
    if (sprach_create_clock_popup(&ctx) != 0)
        ser_puts("[SPRACH] warning: clock popup creation failed\n");
    if (sprach_create_app_menu(&ctx) != 0)
        ser_puts("[SPRACH] warning: app menu creation failed\n");
    if (sprach_create_launchpad(&ctx) != 0)
        ser_puts("[SPRACH] warning: launchpad creation failed\n");

    /* Initial full draw + composite */
    ser_puts("[SPRACH] painting initial scene (");
    print_u32(SPRACH_WINDOW_COUNT);
    ser_puts(" windows)\n");
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx.wins[i].slot >= 0 && !ctx.wins[i].hidden)
            sprach_paint_window(&ctx, &ctx.wins[i]);
    }
    ser_puts("[SPRACH] Dock init OK (taskbar painted, proto=");
    ser_puts(sp_proto_ready ? "1" : "0");
    ser_puts(", slot=");
    print_u32((uint32_t)ctx.taskbar_slot);
    ser_puts(")\n");
    sprach_draw_taskbar(&ctx);
    ser_puts("[SPRACH] Bar init OK (menubar painted, slot=");
    print_u32((uint32_t)ctx.menubar_slot);
    ser_puts(")\n");
    sprach_draw_menubar(&ctx);
    sprach_commit_layout(&ctx);
    shm->dirty = 1;

    ser_puts("[SPRACH] mode '");
    ser_puts(sprach_mode_name());
    ser_puts("' initialized\n");
    ser_puts("[SPRACH] Entering main loop\n");

    /*
     * ── MAIN LOOP (refactored) ──
     *
     * shm->dirty = 1 ONLY for structural scene changes:
     *   1. Window create / destroy (mouse clicks on close button)
     *   2. Window move / resize (mode tick changes layout, maximize/restore)
     *   3. Window Z-order change (click-to-raise)
     *   4. Menubar clock second tick (HH:MM:SS, one composite/s)
     *
     * Mouse-only movement uses m4k_update_cursor() (kernel cursor)
     * and does NOT trigger a Copland composite+flip.
     *
     * Frame pacing: m4k_sleep(20) → ~50 FPS.  The int 0x4D syscall ISR
     * yields to the other processes (Copland/terminal) after every
     * handler, so the sleep also hands the CPU over cooperatively.
     */

    for (;;) {
        ctx.tick++;

        /* ── DIAG: frame timing + memory watchdog.  Off by default;
         * build with -DSPRACH_DIAG to re-enable the ≈2 s serial
         * telemetry. ── */
#ifdef SPRACH_DIAG
        if ((ctx.tick % 100) == 0) {
            struct sysinfo si;
            uint32_t t0 = (uint32_t)musr_sc_time();
            musr_sc_sysinfo(&si);
            ser_puts("[DIAG] tick=");
            print_u32(ctx.tick);
            ser_puts(" t=");
            print_u32(t0);
            ser_puts(" used=");
            print_u32(si.used_ram / 1024);
            ser_puts("K proc=");
            print_u32(si.process_count);
            ser_puts(" hb=");
            print_u32(shm->heartbeat);
            ser_puts("\n");
        }
#endif

        /* ── Terminal client discovery (Ctrl+Alt+T spawns /bin/terminal;
         * the child registers its surface asynchronously) ── */
        sprach_poll_terminal(&ctx);

        /* ── Mode tick (may change window positions) ── */
        int old_active = ctx.active;
        int old_offs   = ctx.offs;
        int old_dir    = ctx.dir;
        sprach_mode_tick(&ctx);
        int layout_changed = (ctx.active != old_active ||
                              ctx.offs   != old_offs   ||
                              ctx.dir    != old_dir);

        /* ── Repaint demo window buffers: ONLY on ticks whose result
         * will actually reach the screen.  A composite happens on
         * layout changes, per-second chrome repaints and every
         * SPRACH_ANIM_TICKS; repainting on the other ~29/30 ticks
         * wrote buffers nobody ever blitted (~36 MB/s of pure RAM
         * bandwidth at 50 FPS x 3 windows). ── */
        int epoch_now = musr_sc_time();
        if (epoch_now < 0)
            epoch_now = 0;
        int need_composite =
            layout_changed ||
            ((epoch_now % 86400) != ctx.menu_last_second) ||
            ((ctx.tick % SPRACH_ANIM_TICKS) == 0);
        if (need_composite) {
            for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
                if (ctx.wins[i].slot >= 0 && !ctx.wins[i].hidden)
                    sprach_paint_window(&ctx, &ctx.wins[i]);
            }
        }

        /* ── Dock: repaint only when the window set or the active
         * window / terminal-launcher highlight changed ── */
        int taskbar_repainted = 0;
        if (sprach_taskbar_dirty(&ctx)) {
            sprach_draw_taskbar(&ctx);
            taskbar_repainted = 1;
        }

        /* ── Menubar clock: repaint + composite once per second ── */
        int menubar_repainted = 0;
        int sec_now = epoch_now % 86400;
        if (sec_now != ctx.menu_last_second) {
            ctx.menu_last_second = sec_now;
            sprach_draw_menubar(&ctx);
            menubar_repainted = 1;
            /* Clock popup (if open) repaints on the same 1 Hz tick. */
            if (ctx.clock_open && ctx.clock_slot >= 0) {
                sprach_draw_clock_popup(&ctx);
                ctx.clock_last_sec = sec_now;
            }
        }

        /* ── Trigger composite ──
         * 1. Structural changes (layout) → commit + composite.
         * 2. Taskbar / menubar repaint → composite this tick only.
         * 3. Every SPRACH_ANIM_TICKS → slow (~0.5 s) refresh so the
         *    demo progress bar / bouncing dot stay alive without
         *    tanking FPS.  The kernel cursor is hardware-accelerated
         *    and never depends on this. */
        if (layout_changed) {
            sprach_commit_layout(&ctx);
            shm->dirty = 1;
        } else if (taskbar_repainted || menubar_repainted) {
            if (sp_proto_ready) {
                /* New-protocol chrome: push damage+commit instead
                 * of the legacy full-screen dirty flag. */
                if (taskbar_repainted)
                    sprach_proto_repaint(sp_id_tb_surf,
                                         SCREEN_W, TASKBAR_H);
                if (menubar_repainted)
                    sprach_proto_repaint(sp_id_mb_surf,
                                         SCREEN_W, MENUBAR_H);
            } else {
                shm->dirty = 1;
            }
        } else if ((ctx.tick % SPRACH_ANIM_TICKS) == 0) {
            shm->dirty = 1;
        }

        /* ── Heartbeat ── */
        shm->heartbeat++;

        /* ── Keyboard ── */
        struct m4k_keyboard_event ev;
        while (m4k_get_keyboard_event(&ev)) {
            if (!ev.ascii_char)
                continue;

            /* Ctrl+Alt+T → launch the terminal emulator */
            if ((ev.modifiers & (M4K_MOD_CTRL | M4K_MOD_ALT)) ==
                    (M4K_MOD_CTRL | M4K_MOD_ALT) &&
                (ev.ascii_char == 't' || ev.ascii_char == 'T')) {
                sprach_spawn_terminal(&ctx);
                continue;
            }

            /* Rule(=Alt)+Shift+1/2/3/4 → switch virtual desktop.
             * Shift maps the digit row to symbols on the US layout
             * (2→@, 3→#, 4→$) — accept both forms. */
            if ((ev.modifiers & (M4K_MOD_ALT | M4K_MOD_SHIFT)) ==
                    (M4K_MOD_ALT | M4K_MOD_SHIFT)) {
                int desk = -1;
                if (ev.ascii_char >= '1' && ev.ascii_char <= '4')
                    desk = ev.ascii_char - '1';
                else if (ev.ascii_char == '!') desk = 0;
                else if (ev.ascii_char == '@') desk = 1;
                else if (ev.ascii_char == '#') desk = 2;
                else if (ev.ascii_char == '$') desk = 3;
                if (desk >= 0) {
                    sprach_switch_desktop(&ctx, desk);
                    continue;
                }
            }

            /* Rule(=Alt)+1..9 → activate the Nth Dock entry */
            if ((ev.modifiers & M4K_MOD_ALT) &&
                !(ev.modifiers & M4K_MOD_SHIFT) &&
                ev.ascii_char >= '1' && ev.ascii_char <= '9') {
                sprach_dock_activate(&ctx, ev.ascii_char - '1');
                continue;
            }

            /* Esc → close launchpad / app menu */
            if (ev.ascii_char == 0x1B) {
                if (ctx.lp_open)
                    sprach_launchpad_toggle(&ctx, 0);
                else if (ctx.menu_open)
                    sprach_app_menu_toggle(&ctx, 0);
                continue;
            }

            /* Ctrl+Alt+Q → quit the WM (bare 'q' used to kill the
             * whole desktop whenever the terminal/FM had focus and
             * the user typed a q — check AFTER the focus-forward
             * paths below would have consumed it). */
            if ((ev.modifiers & (M4K_MOD_CTRL | M4K_MOD_ALT)) ==
                    (M4K_MOD_CTRL | M4K_MOD_ALT) &&
                (ev.ascii_char == 'q' || ev.ascii_char == 'Q')) {
                ser_puts("[SPRACH] quitting (Ctrl+Alt+Q)\n");
                m4k_exit(0);
            }

            /* Terminal window active → forward the keystroke */
            if (ctx.active < 0 && ctx.term_slot >= 0 &&
                !ctx.term_hidden) {
                sprach_terminal_key(&ctx, ev.ascii_char);
                continue;
            }

            /* File manager running and on top → forward there too.
             * The FM registers its key mailbox at FM_MAILBOX_BASE on
             * startup; if the magic isn't there it isn't running. */
            if (sprach_fm_key(&ctx, ev.ascii_char)) {
                continue;
            }

            sprach_mode_key(&ctx, ev.ascii_char);
        }

        /* ── Mouse ── */
        sprach_handle_mouse(&ctx);

        /* ── Frame pacing (~50 FPS).  The syscall ISR yields to other
         * processes after the handler, so no extra m4k_yield needed. ── */
        m4k_sleep(20);
    }
}
