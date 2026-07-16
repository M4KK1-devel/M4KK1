/*
 * M4KK1 4P1 - exit.c
 * Description: exit command - exit the shell
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_exit - Exit the shell
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_exit(int ac, char **av)
{
    (void)av;
    out_puts("exit\n");
    musr_sc0(S_EXIT);
    for (;;)
        __asm__ volatile("hlt");
}
