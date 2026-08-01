/*
 * M4KK1 4P1 - sprach_mode_tiling.c
 * Description: Sprach tiling mode - non-overlapping grid layout
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * The three windows tile the screen side by side (no overlap).  The
 * active window gets a highlighted title bar; it cycles automatically
 * and on SPACE/ENTER.
 */

#include "sprach.h"

static void sprach_set_active_title(struct sprach_ctx *ctx)
{
    static const uint32_t titles[SPRACH_WINDOW_COUNT] = {
        SPRACH_COL_TITLE_1, SPRACH_COL_TITLE_2, SPRACH_COL_TITLE_3
    };
    for (int i = 0; i < SPRACH_WINDOW_COUNT; i++) {
        if (ctx->wins[i].slot >= 0)
            ctx->wins[i].title = (i == ctx->active)
                                     ? SPRACH_COL_ACCENT
                                     : titles[i];
    }
}

void sprach_mode_init(struct sprach_ctx *ctx)
{
    sprach_create_window(ctx, 0, 8,  110, SPRACH_COL_TITLE_1, SPRACH_COL_BODY);
    sprach_create_window(ctx, 1, 272, 110, SPRACH_COL_TITLE_2, SPRACH_COL_BODY);
    sprach_create_window(ctx, 2, 536, 110, SPRACH_COL_TITLE_3, SPRACH_COL_BODY);
    ctx->active = 0;
    sprach_set_active_title(ctx);
}

void sprach_mode_tick(struct sprach_ctx *ctx)
{
    if (ctx->tick % 60 == 0) {
        ctx->active = (ctx->active + 1) % SPRACH_WINDOW_COUNT;
        sprach_set_active_title(ctx);
    }
}

void sprach_mode_key(struct sprach_ctx *ctx, unsigned char key)
{
    if (key == ' ' || key == '\r' || key == '\n') {
        ctx->active = (ctx->active + 1) % SPRACH_WINDOW_COUNT;
        sprach_set_active_title(ctx);
    }
}

const char *sprach_mode_name(void)
{
    return "tiling";
}
