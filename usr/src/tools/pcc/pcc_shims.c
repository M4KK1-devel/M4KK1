/*
 * M4KK1 4P1 - pcc_shims.c
 * Description: mkrn_* string/memory shims for the self-hosted PCC build.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * The i386-pc-m4kk1-pcc cross compiler lowers strcmp/strlen/memset/...
 * to mkrn_* kernel-named builtins.  The userspace pcc image links
 * against m4k_libc (POSIX names), so the mkrn_* aliases are provided
 * here as plain loops.  Do NOT call libc string functions from these
 * shims: the compiler would lower them back into mkrn_* calls and the
 * shims would recurse into themselves.
 */

int mkrn_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int mkrn_strncmp(const char *a, const char *b, unsigned long n)
{
    while (n > 0) {
        if (*a != *b || *a == '\0')
            return (int)(unsigned char)*a - (int)(unsigned char)*b;
        a++; b++; n--;
    }
    return 0;
}

char *mkrn_strncpy(char *d, const char *s, unsigned long n)
{
    unsigned long i = 0;
    while (i < n && s[i] != '\0') { d[i] = s[i]; i++; }
    while (i < n) { d[i] = '\0'; i++; }
    return d;
}

unsigned long mkrn_strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n] != '\0') n++;
    return n;
}

void *mkrn_memset(void *dst, int c, unsigned long n)
{
    unsigned char *p = (unsigned char *)dst;
    unsigned long i = 0;
    while (i < n) { p[i] = (unsigned char)c; i++; }
    return dst;
}

void *mkrn_memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    unsigned long i = 0;
    while (i < n) { d[i] = s[i]; i++; }
    return dst;
}
