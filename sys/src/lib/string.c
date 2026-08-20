/*
 * M4KK1 4P1 - string.c
 * Description: Basic C string and memory functions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/memory.h"

/*
 * dword 粒度搬运 + 字节尾巴，同 usr 侧 musr_copy32 路线。
 * 内联 rep movsl：dword 计数装入 ECX，方向标志由硬件 ABI 保证清除。
 */
static inline void
mkrn_copy32(uint32_t *pDest, const uint32_t *pSrc, size_t n)
{
    __asm__ __volatile__(
        "rep movsl"
        : "+D"(pDest), "+S"(pSrc), "+c"(n)
        :
        : "memory");
}

void *
mkrn_memcpy(void *pDest, const void *pSrc, size_t n)
{
    uint8_t *pD = (uint8_t *)pDest;
    const uint8_t *pS = (const uint8_t *)pSrc;
    size_t nD = n >> 2;

    if (nD)
        mkrn_copy32((uint32_t *)pD, (const uint32_t *)pS, nD);

    for (size_t i = n & ~(size_t)3; i < n; i++)
        pD[i] = pS[i];

    return pDest;
}

void *
mkrn_memmove(void *pDest, const void *pSrc, size_t n)
{
    uint8_t *pD = (uint8_t *)pDest;
    const uint8_t *pS = (const uint8_t *)pSrc;

    if (pD < pS) {
        size_t nD = n >> 2;

        if (nD)
            mkrn_copy32((uint32_t *)pD, (const uint32_t *)pS, nD);

        for (size_t i = n & ~(size_t)3; i < n; i++)
            pD[i] = pS[i];
    } else if (pD > pS) {
        /* 重叠且 dest 在高地址：反向逐字节，语义优先于速度 */
        for (size_t i = n; i > 0; i--)
            pD[i - 1] = pS[i - 1];
    }
    return pDest;
}

/* dword 粒度填充 + 字节尾巴，同 usr 侧 musr_fill32 路线 */
static inline void
mkrn_fill32(uint32_t *pDest, size_t n, uint32_t c)
{
    __asm__ __volatile__(
        "rep stosl"
        : "+D"(pDest), "+c"(n)
        : "a"(c)
        : "memory");
}

void *
mkrn_memset(void *pS, int c, size_t n)
{
    uint8_t *p = (uint8_t *)pS;
    uint8_t v = (uint8_t)c;
    size_t nD = n >> 2;

    if (nD) {
        uint32_t pat = (uint32_t)v;
        pat |= pat << 8;
        pat |= pat << 16;
        mkrn_fill32((uint32_t *)p, nD, pat);
    }

    for (size_t i = n & ~(size_t)3; i < n; i++)
        p[i] = v;

    return pS;
}

char *
mkrn_strcpy(char *pDest, const char *pSrc)
{
    char *pD = pDest;

    while (*pSrc)
        *pD++ = *pSrc++;
    *pD = '\0';

    return pDest;
}

char *
mkrn_strncpy(char *pDest, const char *pSrc, size_t n)
{
    char *pD = pDest;

    while (n > 0 && *pSrc) {
        *pD++ = *pSrc++;
        n--;
    }

    while (n > 0) {
        *pD++ = '\0';
        n--;
    }

    return pDest;
}

size_t
mkrn_strlen(const char *pS)
{
    size_t len = 0;

    while (*pS++)
        len++;

    return len;
}

int
mkrn_strcmp(const char *pS1, const char *pS2)
{
    while (*pS1 && *pS2 && (*pS1 == *pS2)) {
        pS1++;
        pS2++;
    }

    return (int)(*pS1 - *pS2);
}

char *
mkrn_strcat(char *pDest, const char *pSrc)
{
    char *pD = pDest;
    while (*pD) pD++;
    while (*pSrc) *pD++ = *pSrc++;
    *pD = '\0';
    return pDest;
}

int
mkrn_strncmp(const char *pS1, const char *pS2, size_t n)
{
    while (n > 0 && *pS1 && *pS2 && (*pS1 == *pS2)) {
        pS1++;
        pS2++;
        n--;
    }

    if (n == 0)
        return 0;

    return (int)(unsigned char)*pS1 - (int)(unsigned char)*pS2;
}

char *
mkrn_strdup(const char *pS)
{
    if (!pS)
        return NULL;

    size_t len = mkrn_strlen(pS) + 1;
    char *pNewStr = (char *)mkrn_alloc(len);

    if (pNewStr)
        mkrn_memcpy(pNewStr, pS, len);

    return pNewStr;
}

/* Compatibility alias for compiler-generated memcpy calls */
__attribute__((weak, alias("mkrn_memcpy"))) void *
memcpy(void *pDest, const void *pSrc, size_t n);
