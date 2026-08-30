/*
 * M4KK1 4P1 - spawn.c
 * Description: spawn command - launch a program by path via
 *              m4k_spawn (the only way to start GUI/Copland apps
 *              from the shell; m4sh has no PATH exec).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_spawn - Spawn a binary by absolute path
 * @ac: argument count
 * @av: argument vector; av[1] = /bin/<prog>
 *
 * Return: void
 */
void musr_cmd_spawn(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: spawn /bin/<program>\n");
        return;
    }
    int pid = musr_sc_fork();
    if (pid < 0) {
        out_puts("spawn: fork failed\n");
        return;
    }
    if (pid == 0) {
        int r = m4k_spawn(av[1], 0);
        (void)r;
        m4k_exit(1);
    }
    out_puts("spawned ");
    out_puts(av[1]);
    out_puts("\n");
}
