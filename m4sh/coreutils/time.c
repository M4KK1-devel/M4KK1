/*
 * M4KK1 4P1 - time.c
 * Description: time command - measure command execution time
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_time - Measure and display execution time of a command
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_time(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: time <cmd> [args...]\n");
        return;
    }
    uint32_t t0 = musr_sc_time();
    musr_cmd_f f = NULL;
    for (int i = 0; musr_cmd_table[i].name; i++)
        if (musr_strcmp(av[1], musr_cmd_table[i].name) == 0) {
            f = musr_cmd_table[i].func;
            break;
        }
    if (!f) {
        c_red();
        out_puts("time: unknown command\n");
        c_rst();
        return;
    }
    f(ac - 1, av + 1);
    uint32_t t1 = musr_sc_time();
    out_puts("time: ");
    print_u32(t1 - t0);
    out_puts(" sec\n");
}
