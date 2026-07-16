/*
 * M4KK1 4P1 - last.c
 * Description: last command - show uptime
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_last - Display system uptime
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_last(int ac, char **av)
{
    (void)ac;
    (void)av;
    out_puts("uptime: ");
    print_u32(musr_sc_time());
    out_puts(" sec\n");
    out_puts("last: only current session recorded\n");
}
