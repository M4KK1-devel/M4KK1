/*
 * M4KK1 4P1 - ping.c
 * Description: ICMP echo tool (network stack verification).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static void ip_str(uint32_t ip, char *out)
{
    uint8_t *o = (uint8_t *)&ip;
    int n = 0;
    for (int i = 3; i >= 0; i--) {
        int v = o[i];
        if (v >= 100) { out[n++] = '0' + v / 100; v %= 100; }
        if (v >= 10 || o[i] >= 100) out[n++] = '0' + v / 10;
        out[n++] = '0' + v % 10;
        if (i > 0) out[n++] = '.';
    }
    out[n] = 0;
}

static uint32_t parse_ip(const char *s)
{
    uint32_t ip = 0;
    int parts = 0;
    while (parts < 4) {
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
            digits++;
        }
        if (v > 255 || digits == 0 || digits > 3)
            return 0;
        ip = (ip << 8) | (uint32_t)v;
        parts++;
        if (parts < 4) {
            if (*s != '.')
                return 0;
            s++;
        }
    }
    if (*s)
        return 0;
    return ip;
}

void musr_cmd_ping(int argc, char **argv)
{
    if (argc < 2) {
        out_puts("usage: ping <ip>\n");
        return 1;
    }
    uint32_t dst = parse_ip(argv[1]);
    if (!dst) {
        out_puts("ping: invalid address\n");
        return 1;
    }
    char bufs[16];
    ip_str(dst, bufs);
    out_puts("PING ");
    out_puts(bufs);
    out_puts(" (");
    out_puts(argv[1]);
    out_puts(")\n");

    uint32_t r = musr_sc2(S_PING, dst, 3000);
    if (r == 0xFFFFFFFFu) {
        out_puts("no reply (timeout)\n");
        return 1;
    }
    out_puts("reply from ");
    ip_str(r, bufs);
    out_puts(bufs);
    out_puts(": time=---ms\n");
    return 0;
}
