/*
 * M4KK1 4P1 - df.c
 * Description: df command - show filesystem disk usage
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_df - Display filesystem disk space usage
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_df(int ac, char **av)
{
    (void)ac;
    (void)av;
    struct statfs fs;
    if (musr_sc_statfs(&fs) < 0) {
        out_puts("df: error\n");
        return;
    }
    c_ylw();
    out_puts("      blksize      total       used       free  Use%\n");
    c_wht();
    out_puts("yafs  ");
    print_pad_u32(fs.block_size, 7);
    out_puts(" B");
    print_pad_u32(fs.total_blocks, 10);
    print_pad_u32(fs.used_blocks, 10);
    print_pad_u32(fs.free_blocks, 10);
    out_putc(' ');
    uint32_t pct = 0;
    if (fs.total_blocks > 0)
        pct = fs.used_blocks * 100 / fs.total_blocks;
    print_u32(pct);
    out_puts("%\n");
}
