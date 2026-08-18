/*
 * musr_memmove.h — 重叠安全的内存移动（4P1 移植层）
 *
 * 算法: 目标在源前（dst <= src）或完全越过（dst >= src+count）时
 * 正向拷贝无传播风险；否则反向拷贝（从高地址往低地址），保证重叠
 * 缓冲区内容不被提前覆盖。先对齐到 4 字节边界，主体走 uint32_t。
 *
 * M4KK1 4P1 / 2026-08 / SPDX-License-Identifier: 4P1-Custom
 */
#ifndef _M4KK1_MUSR_MEMMOVE_H_
#define _M4KK1_MUSR_MEMMOVE_H_

#include <stdint.h>
#include <stddef.h>

/*
 * musr_memmove - 重叠安全拷贝 count 字节，返回 dst。
 */
static inline void *musr_memmove(void *dst, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (count == 0 || d == s)
        return dst;

    if ((uintptr_t)d <= (uintptr_t)s ||
        (uintptr_t)d >= (uintptr_t)s + count) {
        /* 非重叠（或目标在前）: 低地址 → 高地址 */
        while (((uintptr_t)d & 3u) != 0 && count > 0) {
            *d++ = *s++;
            count--;
        }
        while (count >= 4) {
            *(uint32_t *)d = *(const uint32_t *)s;
            d += 4;
            s += 4;
            count -= 4;
        }
        while (count > 0) {
            *d++ = *s++;
            count--;
        }
    } else {
        /* 重叠: 高地址 → 低地址 */
        d += count;
        s += count;
        while (count > 0 && ((uintptr_t)d & 3u) != 0) {
            *--d = *--s;
            count--;
        }
        while (count >= 4) {
            d -= 4;
            s -= 4;
            *(uint32_t *)d = *(const uint32_t *)s;
            count -= 4;
        }
        while (count > 0) {
            *--d = *--s;
            count--;
        }
    }
    return dst;
}

#endif /* _M4KK1_MUSR_MEMMOVE_H_ */
