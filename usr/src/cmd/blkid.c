/*
 * M4KK1 4P1 - blkid.c
 * Description: blkid command - block device info
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_blkid - Display block device information
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_blkid(int ac, char **av)
{
    (void)ac;
    (void)av;
    struct statfs fs;
    if (musr_sc_statfs(&fs) < 0) {
        out_puts("blkid: error\n");
        return;
    }
    out_puts("/dev/yafs0: ");
    out_puts("TYPE=\"yafs\" BLOCKSIZE=\"");
    print_u32(fs.block_size);
    out_puts("\" TOTAL=\"");
    print_u32(fs.total_blocks);
    out_puts("\" USAGE=\"");
    print_u32(fs.used_blocks * 100
              / (fs.total_blocks ? fs.total_blocks : 1));
    out_puts("%\"\n");
}
