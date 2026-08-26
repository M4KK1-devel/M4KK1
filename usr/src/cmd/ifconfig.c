/*
 * M4KK1 4P1 - ifconfig.c
 * Description: Show network interface configuration.
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

void musr_cmd_ifconfig(int argc, char **argv)
{
    uint32_t ip, mask, gw;
    (void)argc;
    (void)argv;
    uint32_t up = musr_sc3(S_NETINFO, (uint32_t)&ip,
                           (uint32_t)&mask, (uint32_t)&gw);
    char b[16];
    out_puts("eth0: ");
    if (!up) {
        out_puts("down\n");
        return 1;
    }
    ip_str(ip, b);
    out_puts("inet ");
    out_puts(b);
    out_puts(" netmask ");
    ip_str(mask, b);
    out_puts(b);
    out_puts(" gateway ");
    ip_str(gw, b);
    out_puts(b);
    out_puts("\n");
    return 0;
}
