/*
 * M4KK1 4P1 - beep.c
 * Description: Userspace `beep` command — plays a square-wave tone
 *              through the SB16 driver via the M4K_SYS_BEEP syscall.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

void
musr_cmd_beep(int ac, char **av)
{
    uint32_t u32Hz = 440;
    uint32_t u32Ms = 200;

    if (ac > 1) {
        int v = musr_atoi(av[1]);
        if (v > 0)
            u32Hz = (uint32_t)v;
    }
    if (ac > 2) {
        int v = musr_atoi(av[2]);
        if (v > 0)
            u32Ms = (uint32_t)v;
    }

    if (u32Hz == 0 || u32Ms == 0) {
        c_red();
        out_puts("beep: invalid frequency/duration\n");
        c_rst();
        return;
    }

    int iRc = m4k_beep(u32Hz, u32Ms);
    if (iRc == 0) {
        c_grn();
        out_puts("beep: played ");
        print_u32(u32Hz);
        out_puts("Hz for ");
        print_u32(u32Ms);
        out_puts("ms\n");
        c_rst();
    } else {
        c_red();
        out_puts("beep: no sound hardware available\n");
        c_rst();
    }
}
