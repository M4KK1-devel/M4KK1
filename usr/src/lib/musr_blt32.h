/*
 * musr_blt32.h — 32bpp 定位拷贝原语（4P1）
 *
 * 重叠感知的同面 blt：同面前移时从行尾逆序拷贝避免自我覆盖，
 * 行内用整体内存移动代替逐像素循环（现代编译器可向量化）。
 *
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _MUSR_BLT32_H_
#define _MUSR_BLT32_H_

#include <stdint.h>
#include <string.h>

/*
 * musr_blt32_copy — 32bpp 矩形拷贝（方向感知，重叠安全）
 *
 * 当源与目标在同一表面且
 * x 方向正序拷贝会自我覆盖时，改从行尾逆序拷贝；行内用
 * memmove 整块移动（现代编译器会向量化，远快于逐像素循环）。
 *
 * 参数：
 *   dst/dst_w : 目标表面与宽度（像素）
 *   dx, dy    : 目标左上角
 *   src/src_w : 源表面与宽度（像素）
 *   sx, sy    : 源左上角
 *   w, h      : 矩形尺寸
 */
static inline void musr_blt32_copy(uint32_t *dst, int dst_w,
                                   int dx, int dy,
                                   const uint32_t *src, int src_w,
                                   int sx, int sy,
                                   int w, int h)
{
    int rev = (src == dst && dx > sx);  /* 同面前移 → 逆序拷 */

    if (!rev) {
        for (int r = 0; r < h; r++)
            memmove(&dst[(size_t)(dy + r) * dst_w + dx],
                    &src[(size_t)(sy + r) * src_w + sx],
                    (size_t)w * 4);
    } else {
        for (int r = h - 1; r >= 0; r--)
            memmove(&dst[(size_t)(dy + r) * dst_w + dx],
                    &src[(size_t)(sy + r) * src_w + sx],
                    (size_t)w * 4);
    }
}

/*
 * musr_blt32_fill — 纯色填充（fastfill.cxx 思想的极简版）
 */
static inline void musr_blt32_fill(uint32_t *dst, int dst_w,
                                   int x, int y, int w, int h,
                                   uint32_t color)
{
    for (int r = 0; r < h; r++) {
        uint32_t *row = &dst[(size_t)(y + r) * dst_w + x];
        for (int c = 0; c < w; c++)
            row[c] = color;
    }
}

#endif /* _MUSR_BLT32_H_ */
