/*
 * M4KK1 4P1 - cd.c
 * Description: cd command - change directory
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_cd - Change the current working directory
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_cd(int ac, char **av)
{
    const char *path = (ac > 1) ? av[1] : "/";
    if (musr_sc_chdir(path) < 0) {
        c_red();
        out_puts("cd: ");
        out_puts(path);
        out_puts(": not found\n");
        c_rst();
        return;
    }
    cwd_init();
}
