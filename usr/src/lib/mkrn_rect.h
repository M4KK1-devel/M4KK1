/*
 * mkrn_rect.h — 脏矩形核心算法（4P1 标准）
 *
 * 半开矩形（half-open）的并/交/有序化纯函数集，供内核与用户态
 * 增量合成计算脏区域包围盒使用。
 *
 * M4KK1 4P1 / 2026-08 / SPDX-License-Identifier: 4P1-Custom
 */
#ifndef _M4KK1_MKRN_RECT_H_
#define _M4KK1_MKRN_RECT_H_

#include <stdint.h>

/* RECTL: 左上闭、右下开（half-open 约定） */
struct mkrn_rect {
    int32_t left, top, right, bottom;
};

/* 空矩形约定: left > right 或 top > bottom 即空 */
static inline int mkrn_rect_is_empty(const struct mkrn_rect *r)
{
    return (r->left > r->right) || (r->top > r->bottom);
}

/* vInit: 初始化为点 */
static inline void mkrn_rect_init_pt(struct mkrn_rect *r,
                                     int32_t x, int32_t y)
{
    r->left = r->right = x;
    r->top = r->bottom = y;
}

/* operator+= (RECTL&): 并入边界（目标可为空，源必须非空）。
 * 语义: 若目标为空 → 直接取源；否则四边取极值扩展。 */
static inline void mkrn_rect_union(struct mkrn_rect *dst,
                                   const struct mkrn_rect *src)
{
    if (mkrn_rect_is_empty(dst)) {
        *dst = *src;
        return;
    }
    if (src->left < dst->left)   dst->left = src->left;
    if (src->top < dst->top)     dst->top = src->top;
    if (src->right > dst->right) dst->right = src->right;
    if (src->bottom > dst->bottom) dst->bottom = src->bottom;
}

/* operator*= (RECTL&): 相交（不检查空，结果可能为空） */
static inline void mkrn_rect_intersect(struct mkrn_rect *dst,
                                       const struct mkrn_rect *clip)
{
    if (clip->left > dst->left)   dst->left = clip->left;
    if (clip->top > dst->top)     dst->top = clip->top;
    if (clip->right < dst->right) dst->right = clip->right;
    if (clip->bottom < dst->bottom) dst->bottom = clip->bottom;
}

/* vOrder: 有序化（交换越界边） */
static inline void mkrn_rect_order(struct mkrn_rect *r)
{
    int32_t t;
    if (r->left > r->right) { t = r->left; r->left = r->right; r->right = t; }
    if (r->top > r->bottom) { t = r->top; r->top = r->bottom; r->bottom = t; }
}

#endif /* _M4KK1_MKRN_RECT_H_ */
