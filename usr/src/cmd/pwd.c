/*
 * M4KK1 4P1 - pwd.c
 * Description: pwd command - print working directory
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_pwd - Print the current working directory
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_pwd(int ac, char **av)
{
    (void)ac;
    (void)av;
    char buf[256];
    int n = musr_sc_getcwd(buf, 256);
    if (n > 0) {
        out_puts(buf);
        out_puts("\n");
    }
}
