/*
 * M4KK1 4P1 - kill.c
 * Description: kill command - send signal to process
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_kill - Send a signal to a process
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_kill(int ac, char **av)
{
    if (ac < 2) {
        out_puts("kill: usage: kill [-signal] <pid>\n");
        return;
    }
    int sig = 15;
    int pid_off = 1;
    if (av[1][0] == '-') {
        sig = 0;
        int i = 1;
        while (av[1][i]) {
            sig = sig * 10 + (av[1][i] - '0');
            i++;
        }
        pid_off = 2;
        if (ac <= 2) {
            out_puts("kill: missing pid\n");
            return;
        }
    }
    int pid = 0;
    char *p = av[pid_off];
    while (*p) {
        pid = pid * 10 + (*p - '0');
        p++;
    }
    if (musr_sc_kill(pid, sig) < 0) {
        out_puts("kill: failed\n");
    }
}
