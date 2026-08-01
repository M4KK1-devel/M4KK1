/*
 * M4KK1 4P1 - sprach_mode_scroll.c
 * Description: Sprach scrolling mode - virtual desktop under a viewport
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * The three windows live on a virtual canvas three screen widths wide.
 * A viewport scrolls across it, so the desktop appears to pan sideways.
 * Direction flips automatically and on 'l'; SPACE/ENTER snaps the
 * viewport to the next window.
 */

#include "sprach.h"

#define SPRACH_SCROLL_CANVAS (3 * 800)

static const int sprach_vx[SPRACH_WINDOW_COUNT] = { 100, 900, 1700 };

void sprach_mode_init(struct sprach_ctx *ctx)
{
    sprach_create_window(ctx, 0, sprach_vx[0] - ctx->offs, 110,
                         SPRACH_COL_TITLE_1, SPRACH_COL_BODY);
    sprach_create_window(ctx, 1, sprach_vx[1] - ctx->offs, 110,
                         SPRACH_COL_TITLE_2, SPRACH_COL_BODY);
    sprach_create_window(ctx, 2, sprach_vx[2] - ctx->offs, 110,
                         SPRACH_COL_TITLE_3, SPRACH_COL_BODY);
    ctx->active = 0;
    ctx->dir = 1;
}

void sprach_mode_tick(struct sprach_ctx *ctx)
{
    if (ctx->tick % 300 == 0)
        ctx->dir = -ctx->dir;

    ctx->offs += ctx->dir * 2;
    if (ctx->offs < 0)
        ctx->offs += 800;
    if (ctx->offs >= 800)
        ctx->offs -= 800;

    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot >= 0) {
            ctx->wins[i].x = sprach_vx[i] - ctx->offs;
            ctx->wins[i].y = 110;
        }
    }
}

void sprach_mode_key(struct sprach_ctx *ctx, unsigned char key)
{
    if (key == 'l' || key == 'L') {
        ctx->dir = -ctx->dir;
    } else if (key == ' ' || key == '\r' || key == '\n') {
        ctx->active = (ctx->active + 1) % SPRACH_WINDOW_COUNT;
        /* Snap the viewport so the active window is centered */
        ctx->offs = sprach_vx[ctx->active] - (800 - SPRACH_WIN_W) / 2;
        if (ctx->offs < 0)
            ctx->offs = 0;
        if (ctx->offs >= 800)
            ctx->offs = 800 - 1;
    }
}

const char *sprach_mode_name(void)
{
    return "scrolling";
}
