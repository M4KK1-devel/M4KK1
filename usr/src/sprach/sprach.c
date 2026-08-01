/*
 * M4KK1 4P1 - sprach.c
 * Description: Sprach window manager - core (mode-independent)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../lib/libgui.h"
#include "sprach.h"

/* Globals required by the m4sh.h ABI */
int out_fd = 1;
char cwd[256] = "/";

/* Window pixel buffers live in Sprach's own BSS: flat memory means
 * Copland can blit them straight from here into the framebuffer. */
static uint32_t sprach_bufs[SPRACH_WINDOW_COUNT]
                          [SPRACH_WIN_W * SPRACH_WIN_H]
    __attribute__((aligned(16)));

/* ── Pixel helpers (into a window buffer) ── */

static void sp_buf_px(struct sprach_window *w, int x, int y, uint32_t c)
{
    if (x >= 0 && x < w->w && y >= 0 && y < w->h)
        w->buf[y * w->w + x] = c;
}

static void sp_buf_fill(struct sprach_window *w, uint32_t c)
{
    int n = w->w * w->h;
    for (int i = 0; i < n; i++)
        w->buf[i] = c;
}

static void sp_buf_rect(struct sprach_window *w, int x, int y,
                        int rw, int rh, uint32_t c)
{
    for (int yy = 0; yy < rh; yy++)
        for (int xx = 0; xx < rw; xx++)
            sp_buf_px(w, x + xx, y + yy, c);
}

/* ── Surface creation ──
 *
 * Push a CREATE command, then wait for Copland to occupy a new surface
 * slot.  The wait loop polls the keyboard (a syscall), which both gives
 * the server a chance to run and drains any input noise.  Sprach takes
 * ownership of the pixel buffer by writing buffer_ptr directly into the
 * shared surface struct. */
int sprach_create_window(struct sprach_ctx *ctx, int idx, int x, int y,
                         uint32_t title, uint32_t body)
{
    struct sprach_window *w = &ctx->wins[idx];

    w->slot = -1;
    w->x = x;
    w->y = y;
    w->w = SPRACH_WIN_W;
    w->h = SPRACH_WIN_H;
    w->title = title;
    w->body = body;
    w->buf = sprach_bufs[idx];

    int before = 0;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            before++;

    if (copland_cmd_push(ctx->shm, COPLAND_CMD_CREATE_SURFACE,
                         x, y, w->w, w->h, (int32_t)body,
                         COPLAND_SURF_VISIBLE) != 0)
        return -1;

    int guard = 0;
    for (;;) {
        struct m4k_keyboard_event ev;
        m4k_get_keyboard_event(&ev);        /* yields to Copland */
        if (++guard > 200000)
            return -1;
        int after = 0;
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (ctx->shm->surfaces[i].in_use)
                after++;
        if (after > before)
            break;
    }

    for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (ctx->shm->surfaces[i].in_use &&
            !ctx->shm->surfaces[i].buffer_ptr) {
            w->slot = i;
            ctx->shm->surfaces[i].buffer_ptr = (uint32_t)(uintptr_t)w->buf;
            return 0;
        }
    }
    return -1;
}

/* ── Window rendering (client side) ── */

void sprach_paint_window(struct sprach_ctx *ctx, struct sprach_window *w)
{
    sp_buf_fill(w, w->body);

    /* Title bar */
    sp_buf_rect(w, 0, 0, w->w, SPRACH_TITLE_H, w->title);

    /* Close button */
    sp_buf_rect(w, w->w - 16, 2, 12, 12, SPRACH_COL_CLOSE);

    /* Pseudo-title bar (rects stand in for a font) */
    sp_buf_rect(w, 4, 6, 48, 4, SPRACH_COL_TEXTBAR);

    /* Content: a progress bar driven by the frame counter (proves the
     * REDRAW path is live) */
    int pw = (int)((ctx->tick * 3) % (uint32_t)(w->w - 24));
    sp_buf_rect(w, 12, w->h - 20, pw, 8, SPRACH_COL_ACCENT);

    /* A bouncing dot */
    int bx = (int)((ctx->tick * 5 + (uint32_t)(w->slot + 1) * 37)
                   % (uint32_t)(w->w - 24)) + 12;
    int by = (int)((ctx->tick * 7 + (uint32_t)(w->slot + 1) * 53)
                   % (uint32_t)(w->h - 44)) + SPRACH_TITLE_H + 8;
    sp_buf_rect(w, bx, by, 6, 6, SPRACH_COL_DOT);

    /* Border */
    sp_buf_rect(w, 0, 0, w->w, 1, SPRACH_COL_BORDER);
    sp_buf_rect(w, 0, 0, 1, w->h, SPRACH_COL_BORDER);
    sp_buf_rect(w, 0, w->h - 1, w->w, 1, SPRACH_COL_BORDER);
    sp_buf_rect(w, w->w - 1, 0, 1, w->h, SPRACH_COL_BORDER);
}

/* ── Layout commit: tell Copland where every window is ── */

void sprach_commit_layout(struct sprach_ctx *ctx)
{
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        struct sprach_window *w = &ctx->wins[i];
        if (w->slot < 0)
            continue;
        copland_cmd_push(ctx->shm, COPLAND_CMD_MOVE_SURFACE,
                         w->slot, w->x, w->y, 0, 0, 0);
    }
}

/* ── Entry point ── */

void _start(void)
{
    ser_puts("[SPRACH] ================================\n");
    ser_puts("[SPRACH] Window manager starting (mode: ");
    ser_puts(sprach_mode_name());
    ser_puts(")...\n");
    ser_puts("[SPRACH] ================================\n");

    struct copland_shm *shm = copland_shm_get();

    /* Wait for the display server to bring the shared region up */
    int guard = 0;
    while (!shm->ready) {
        struct m4k_keyboard_event ev;
        m4k_get_keyboard_event(&ev);
        if (++guard > 200000) {
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

    sprach_mode_init(&ctx);

    ser_puts("[SPRACH] mode '");
    ser_puts(sprach_mode_name());
    ser_puts("' initialized\n");

    /* WM loop */
    for (;;) {
        ctx.tick++;

        sprach_mode_tick(&ctx);

        for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
            if (ctx.wins[i].slot >= 0)
                sprach_paint_window(&ctx, &ctx.wins[i]);
        }

        sprach_commit_layout(&ctx);

        /* Watchdog heartbeat for Copland */
        shm->heartbeat++;

        /* Input (a syscall: also yields cooperatively to Copland) */
        struct m4k_keyboard_event ev;
        while (m4k_get_keyboard_event(&ev)) {
            if (!ev.ascii_char)
                continue;
            if (ev.ascii_char == 'q' || ev.ascii_char == 'Q') {
                ser_puts("[SPRACH] quitting (key pressed)\n");
                m4k_exit(0);
            }
            sprach_mode_key(&ctx, ev.ascii_char);
        }

        /* Coarse frame pacing (no sleep syscall in 4P1) */
        for (volatile int i = 0; i < 50000; i++);
    }
}
