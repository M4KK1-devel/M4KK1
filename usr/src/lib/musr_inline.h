/*
 * musr_inline.h — i386 内联汇编原语（4P1 移植层）
 *
 * 对齐 dword 块操作：rep movsd / rep stosd，非对齐前导/尾随
 * 字节单独处理。移植自经典 i386 CRT 实现（memset/memcpy 的
 * Pentium 优化路线：对齐目标指针 → dword 主体 → 字节尾部）。
 *
 * M4KK1 4P1 / 2026-08 / SPDX-License-Identifier: 4P1-Custom
 */
#ifndef _M4KK1_MUSR_INLINE_H_
#define _M4KK1_MUSR_INLINE_H_

#include <stdint.h>
#include <stddef.h>

/*
 * musr_fill32 - 以 dword 粒度填充 n 个 uint32_t（rep stosd 路线）。
 * buf 无对齐要求（i386 允许非对齐访问，此处天然 4 对齐）。
 */
static inline void musr_fill32(uint32_t *buf, size_t n, uint32_t c)
{
    __asm__ volatile (
        "cld\n\t"
        "rep stosl"
        : /* no output */
        : "D" (buf), "c" (n), "a" (c)
        : "memory", "cc"
    );
}

/*
 * musr_copy32 - 以 dword 粒度拷贝 n 个 uint32_t（rep movsd 路线）。
 * 调用方保证不重叠（重叠请用 musr_memmove）。
 */
static inline void musr_copy32(uint32_t *dst, const uint32_t *src, size_t n)
{
    __asm__ volatile (
        "cld\n\t"
        "rep movsl"
        : /* no output */
        : "S" (src), "D" (dst), "c" (n)
        : "memory", "cc"
    );
}

/*
 * musr_blt32_row - 单行像素拷贝（合成热路径专用）。
 * 等价 musr_copy32，语义化命名便于合成器调用点识别。
 */
static inline void musr_blt32_row(uint32_t *dst, const uint32_t *src, size_t n)
{
    musr_copy32(dst, src, n);
}

#endif /* _M4KK1_MUSR_INLINE_H_ */
