/*
 * M4KK1 4P1 - mv.c
 * Description: mv command - move/rename files
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_mv - Move or rename a file
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_mv(int ac, char **av)
{
    if (ac < 3) {
        out_puts("usage: mv <src> <dst>\n");
        return;
    }
    char src[256], dst[256];
    cwd_to_abs(av[1], src, 256);
    cwd_to_abs(av[2], dst, 256);
    if (musr_sc_rename(src, dst) < 0) {
        c_red();
        out_puts("mv: failed\n");
        c_rst();
    }
}
