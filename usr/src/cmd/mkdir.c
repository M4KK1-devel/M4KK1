/*
 * M4KK1 4P1 - mkdir.c
 * Description: mkdir command - create directories
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_mkdir - Create a directory
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_mkdir(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: mkdir <dir>\n");
        return;
    }
    char ap[256];
    cwd_to_abs(av[1], ap, 256);
    if (musr_sc_mkdir(ap) < 0) {
        c_red();
        out_puts("mkdir: ");
        out_puts(av[1]);
        out_puts(": failed\n");
        c_rst();
    }
}
