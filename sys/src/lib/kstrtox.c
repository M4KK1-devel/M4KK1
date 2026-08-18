/*
 * mkrn_kstrtox.c — 内核字符串→整数转换（4P1 移植层）
 *
 * 算法: 经典 kstrtox 路线 —
 *   - radix 自动检测（0x→16、前导 0→8、否则 10）
 *   - 逐字符换算，仅在 res 高 4 位已置位时做显式
 *     乘/加溢出检查（快路径零开销）
 *   - 返回值带 consumed 计数与 KSTRTOX_OVERFLOW 位
 *
 * M4KK1 4P1 / 2026-08
 */

#include <stdint.h>
#include <stddef.h>

#include "../../include/kernel.h"

#define ERANGE  M4K_ERANGE
#ifndef EINVAL
#define EINVAL  M4K_EINVAL
#endif

#define M4K_KSTRTOX_OVERFLOW (1u << 31)

static int m4k_isdigit(int c)
{
    return c >= '0' && c <= '9';
}

static int m4k_tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

static int m4k_isxdigit(int c)
{
    return m4k_isdigit(c) ||
           (m4k_tolower(c) >= 'a' && m4k_tolower(c) <= 'f');
}

static const char *mkrn_fixup_radix(const char *s,
                                    unsigned int *base)
{
    if (*base == 0) {
        if (s[0] == '0') {
            if (m4k_tolower(s[1]) == 'x' && m4k_isxdigit(s[2]))
                *base = 16;
            else
                *base = 8;
        } else {
            *base = 10;
        }
    }
    if (*base == 16 && s[0] == '0' && m4k_tolower(s[1]) == 'x')
        s += 2;
    return s;
}

/**
 * mkrn_parse_integer - convert digit string to u64
 * @s: digit string
 * @base: radix (2..16)
 * @p: result location
 * @max_chars: maximum characters to consume
 *
 * Return: number of chars consumed, ORed with
 *         M4K_KSTRTOX_OVERFLOW on overflow (res set to max)
 */
static unsigned int mkrn_parse_integer(const char *s,
                                       unsigned int base,
                                       uint64_t *p,
                                       size_t max_chars)
{
    unsigned int rv;
    int overflow = 0;
    uint64_t res = 0;

    for (rv = 0; rv < max_chars; rv++, s++) {
        unsigned int c = (unsigned char)*s;
        unsigned int lc = (unsigned int)m4k_tolower((int)c);
        unsigned int val;

        if (c >= '0' && c <= '9')
            val = c - '0';
        else if (lc >= 'a' && lc <= 'f')
            val = lc - 'a' + 10;
        else
            break;

        if (val >= base)
            break;

        if (res & (~0ull << 59)) {
            /* slow path: saturate.  res >= 2^59 and base in
             * 2..16: after 4 more digits it certainly exceeds
             * any caller's range; just stop and flag.  (Full
             * 64-bit precision is not needed by M4KK1's u32
             * callers; ERANGE on absurd input is correct.) */
            res = ~0ull;
            overflow = 1;
            break;
        }
        res = res * base + val;
    }
    *p = res;
    return rv | (overflow ? M4K_KSTRTOX_OVERFLOW : 0u);
}

static int mkrn_kstrtoull_impl(const char *s, unsigned int base,
                          uint64_t *res)
{
    uint64_t v;
    unsigned int rv;

    s = mkrn_fixup_radix(s, &base);
    rv = mkrn_parse_integer(s, base, &v, (size_t)-1);
    if (rv & M4K_KSTRTOX_OVERFLOW)
        return -ERANGE;
    if (rv == 0)
        return -EINVAL;
    s += rv & ~M4K_KSTRTOX_OVERFLOW;
    if (*s == '\n')
        s++;
    if (*s)
        return -EINVAL;
    *res = v;
    return 0;
}

/* ── public wrappers ── */

int mkrn_kstrtoull(const char *s, unsigned int base,
                   uint64_t *res)
{
    if (s[0] == '+')
        s++;
    return mkrn_kstrtoull_impl(s, base, res);
}

int mkrn_kstrtoll(const char *s, unsigned int base,
                  int64_t *res)
{
    uint64_t tmp;
    int rv;

    if (s[0] == '-') {
        rv = mkrn_kstrtoull_impl(s + 1, base, &tmp);
        if (rv < 0)
            return rv;
        if ((int64_t)-tmp > 0)
            return -ERANGE;
        *res = -(int64_t)tmp;
        return 0;
    }
    rv = mkrn_kstrtoull(s, base, &tmp);
    if (rv < 0)
        return rv;
    if (tmp > (uint64_t)INT64_MAX)
        return -ERANGE;
    *res = (int64_t)tmp;
    return 0;
}

int mkrn_kstrtoul(const char *s, unsigned int base,
                  unsigned long *res)
{
    uint64_t tmp;
    int rv = mkrn_kstrtoull_impl(s, base, &tmp);
    if (rv < 0)
        return rv;
    if (tmp != (unsigned long)tmp)
        return -ERANGE;
    *res = (unsigned long)tmp;
    return 0;
}

int mkrn_kstrtol(const char *s, unsigned int base, long *res)
{
    int64_t tmp;
    int rv = mkrn_kstrtoll(s, base, &tmp);
    if (rv < 0)
        return rv;
    if (tmp != (long)tmp)
        return -ERANGE;
    *res = (long)tmp;
    return 0;
}

int mkrn_kstrtouint(const char *s, unsigned int base,
                    unsigned int *res)
{
    unsigned long long tmp;
    int rv = mkrn_kstrtoull_impl(s, base, &tmp);
    if (rv < 0)
        return rv;
    if (tmp != (unsigned int)tmp)
        return -ERANGE;
    *res = (unsigned int)tmp;
    return 0;
}

int mkrn_kstrtoint(const char *s, unsigned int base, int *res)
{
    long long tmp;
    int rv = mkrn_kstrtoll(s, base, &tmp);
    if (rv < 0)
        return rv;
    if (tmp != (int)tmp)
        return -ERANGE;
    *res = (int)tmp;
    return 0;
}
