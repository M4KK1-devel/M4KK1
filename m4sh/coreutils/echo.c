/*
 * M4KK1 4P1 - echo.c
 * Description: echo command - print arguments
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_echo - Print arguments to stdout
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_echo(int ac, char **av)
{
    for (int i = 1; i < ac; i++) {
        if (i > 1)
            out_putc(' ');
        out_puts(av[i]);
    }
    out_puts("\n");
}
