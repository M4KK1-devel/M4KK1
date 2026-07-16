/*
 * M4KK1 4P1 - rmdir.c
 * Description: rmdir command - remove directories
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_rmdir - Remove an empty directory
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_rmdir(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: rmdir <dir>\n");
        return;
    }
    char ap[256];
    cwd_to_abs(av[1], ap, 256);
    if (musr_sc_rmdir(ap) < 0) {
        c_red();
        out_puts("rmdir: ");
        out_puts(av[1]);
        out_puts(": failed\n");
        c_rst();
    }
}
