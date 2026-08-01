/*
 * M4KK1 4P1 - sprach_mode_stack.c
 * Description: Sprach stacking mode - overlapping windows, active on top
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Windows overlap diagonally.  Copland composites surfaces in slot
 * order (back to front), so "raise" = move the surface struct to the
 * highest in-use slot.  The active window cycles automatically and on
 * SPACE/ENTER.
 */

#include "sprach.h"

void sprach_mode_init(struct sprach_ctx *ctx)
{
    sprach_create_window(ctx, 0, 140, 90,  SPRACH_COL_TITLE_1, SPRACH_COL_BODY);
    sprach_create_window(ctx, 1, 180, 130, SPRACH_COL_TITLE_2, SPRACH_COL_BODY);
    sprach_create_window(ctx, 2, 220, 170, SPRACH_COL_TITLE_3, SPRACH_COL_BODY);
    ctx->active = 2;
}

/* Raise window idx to the top of the composite order */
static void sprach_raise(struct sprach_ctx *ctx, int idx)
{
    struct sprach_window *w = &ctx->wins[idx];
    if (w->slot < 0)
        return;

    int top = -1;
    for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
        if (ctx->shm->surfaces[i].in_use)
            top = i;
    if (top < 0 || top == w->slot)
        return;

    struct copland_surface tmp = ctx->shm->surfaces[top];
    ctx->shm->surfaces[top] = ctx->shm->surfaces[w->slot];
    ctx->shm->surfaces[w->slot] = tmp;
    w->slot = top;
}

static void sprach_activate(struct sprach_ctx *ctx, int idx)
{
    ctx->active = idx;
    sprach_raise(ctx, idx);
}

void sprach_mode_tick(struct sprach_ctx *ctx)
{
    if (ctx->tick % 60 == 0)
        sprach_activate(ctx, (ctx->active + 1) % SPRACH_WINDOW_COUNT);
}

void sprach_mode_key(struct sprach_ctx *ctx, unsigned char key)
{
    if (key == ' ' || key == '\r' || key == '\n')
        sprach_activate(ctx, (ctx->active + 1) % SPRACH_WINDOW_COUNT);
}

const char *sprach_mode_name(void)
{
    return "stacking";
}
