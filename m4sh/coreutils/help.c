/*
 * M4KK1 4P1 - help.c
 * Description: help command - show available commands
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_help - Display the help message listing all commands
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_help(int ac, char **av)
{
    (void)ac;
    (void)av;
    out_puts("M4SH commands:\n");
    for (int i = 0; musr_cmd_table[i].name; i++) {
        c_grn();
        out_puts("  ");
        out_puts(musr_cmd_table[i].name);
        c_wht();
        out_puts("  - ");
        out_puts(musr_cmd_table[i].desc);
        out_puts("\n");
    }
    c_rst();
}
