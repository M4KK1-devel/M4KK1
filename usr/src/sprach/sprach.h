/*
 * M4KK1 4P1 - sprach.h
 * Description: Sprach window manager - shared core declarations
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Sprach is the desktop shell / window manager.  It runs as a child of
 * the Copland display server (fork + exec) at 0x900000 and talks to
 * Copland through the shared-memory protocol in libcopland.h:
 *
 *   - windows are Copland surfaces whose pixel buffers live in Sprach's
 *     own BSS (clients render, the server composites via gfx blit);
 *   - every frame Sprach paints its windows, pushes MOVE commands for
 *     the mode layout, bumps shm->heartbeat for the Copland watchdog
 *     and polls the keyboard (a syscall, which also cooperatively
 *     yields the CPU back to Copland).
 *
 * Three layout modes are compiled from one core (sprach.c) plus a mode
 * file: sprach_mode_stack.c (overlapping, active-on-top), sprach_mode_
 * tiling.c (non-overlapping grid) and sprach_mode_scroll.c (a virtual
 * canvas scrolled under a fixed viewport).
 */

#ifndef SPRACH_H
#define SPRACH_H

#include <stdint.h>
#include "../lib/libcopland.h"

/* ── Window geometry ── */

#define SPRACH_WINDOW_COUNT 3
#define SPRACH_WIN_W        256
#define SPRACH_WIN_H        192
#define SPRACH_TITLE_H      18

/* ── Chrome colors (BGRA) ── */

#define SPRACH_COL_BODY     0x00D0D0D0
#define SPRACH_COL_TITLE_1  0x00803030
#define SPRACH_COL_TITLE_2  0x00308030
#define SPRACH_COL_TITLE_3  0x00303080
#define SPRACH_COL_CLOSE    0x00C03030
#define SPRACH_COL_TEXTBAR  0x008080C0
#define SPRACH_COL_ACCENT   0x00FFD060
#define SPRACH_COL_DOT      0x00206020
#define SPRACH_COL_BORDER   0x00505050

/* ── A window owned by Sprach ── */

struct sprach_window {
    int slot;            /* Copland surface slot */
    int x, y;            /* screen position (mode-driven) */
    int w, h;            /* fixed size */
    uint32_t title;      /* title-bar color */
    uint32_t body;       /* body color */
    uint32_t *buf;       /* pixel buffer (w * h) */
};

/* ── Per-mode context ── */

struct sprach_ctx {
    struct copland_shm *shm;
    struct sprach_window wins[SPRACH_WINDOW_COUNT];
    uint32_t tick;
    int active;          /* active window index (stacking: raised) */
    int dir;             /* scroll direction */
    int offs;            /* scroll viewport offset */
};

/* Core services (sprach.c) */
int sprach_create_window(struct sprach_ctx *ctx, int idx, int x, int y,
                         uint32_t title, uint32_t body);
void sprach_paint_window(struct sprach_ctx *ctx, struct sprach_window *w);
void sprach_commit_layout(struct sprach_ctx *ctx);

/* Mode interface (one mode file per binary) */
void sprach_mode_init(struct sprach_ctx *ctx);
void sprach_mode_tick(struct sprach_ctx *ctx);
void sprach_mode_key(struct sprach_ctx *ctx, unsigned char key);
const char *sprach_mode_name(void);

#endif /* SPRACH_H */
