/*
 * musr_dirty.h — 增量合成（脏矩形跟踪）用户态适配层
 *
 * 思路: 保存遮挡区背景 → 移动 → 恢复背景，避免全屏重绘。
 * 已有内核 syscall m4k_gfx_blit(x,y,w,h,src) 支持局部 blit，
 * 本层在其上实现"脏区域合并 → 单次局部 blit"。
 *
 * M4KK1 4P1 / 2026-08 / SPDX-License-Identifier: 4P1-Custom
 */
#ifndef _M4KK1_MUSR_DIRTY_H_
#define _M4KK1_MUSR_DIRTY_H_

#include <stdint.h>
#include "mkrn_rect.h"

#define MUSR_DIRTY_MAX_RECTS 8

/* 脏区域集: N 个矩形 + 包围盒。合并策略 = 简单包围盒累并。
 * 对 800x600 桌面包围盒策略在
 * 大多数场景（单窗口更新）已是最优；多远端窗口时退化为大矩形
 * 仍优于全屏（壁纸+全部 surface 重画）。 */
struct musr_dirty_region {
    struct mkrn_rect rects[MUSR_DIRTY_MAX_RECTS];
    int count;
    struct mkrn_rect bounds;    /* 累并包围盒（可能为空） */
};

static inline void musr_dirty_reset(struct musr_dirty_region *d)
{
    d->count = 0;
    d->bounds.left = 1; d->bounds.top = 1;
    d->bounds.right = 0; d->bounds.bottom = 0;   /* empty */
}

/* 标记 (x,y,w,h) 为脏。屏幕坐标裁剪由调用方负责（w/h<=0 忽略）。 */
static inline void musr_dirty_add(struct musr_dirty_region *d,
                                  int32_t x, int32_t y,
                                  int32_t w, int32_t h)
{
    struct mkrn_rect r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    if (w <= 0 || h <= 0)
        return;
    mkrn_rect_union(&d->bounds, &r);
    if (d->count < MUSR_DIRTY_MAX_RECTS)
        d->rects[d->count++] = r;
}

static inline int musr_dirty_is_empty(const struct musr_dirty_region *d)
{
    return mkrn_rect_is_empty(&d->bounds);
}

/* 将脏区域与屏幕求交并返回有效 blit 参数 */
static inline int musr_dirty_get_blit(const struct musr_dirty_region *d,
                                      struct mkrn_rect *out,
                                      int32_t screen_w, int32_t screen_h)
{
    struct mkrn_rect clip;
    *out = d->bounds;
    clip.left = 0; clip.top = 0;
    clip.right = screen_w; clip.bottom = screen_h;
    mkrn_rect_intersect(out, &clip);
    return !mkrn_rect_is_empty(out);
}

#endif /* _M4KK1_MUSR_DIRTY_H_ */
