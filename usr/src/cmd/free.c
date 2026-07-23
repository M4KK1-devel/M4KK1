/*
 * M4KK1 4P1 - free.c
 * Description: free command - show memory statistics
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_free - Display system memory usage
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_free(int ac, char **av)
{
    (void)ac;
    (void)av;
    struct sysinfo info;
    if (musr_sc_sysinfo(&info) < 0) {
        out_puts("free: error\n");
        return;
    }
    c_ylw();
    out_puts("               total       used       free\n");
    c_wht();
    out_puts("Mem:   ");
    print_pad_u32(info.total_ram / 1024, 10);
    out_puts(" KB");
    print_pad_u32(info.used_ram / 1024, 10);
    out_puts(" KB");
    print_pad_u32(info.free_ram / 1024, 10);
    out_puts(" KB\n");
    out_puts("Proc:  ");
    print_pad_u32(info.process_count, 10);
    out_puts(" running\n");
}
