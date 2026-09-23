/*
 * M4KK1 4P1 - scrstress.c
 * Description: scrstress command - scroll-stress generator for the
 *              GUI terminal probe (scrollback exhaustion / PageUp).
 *
 * Prints 600 rows then an END row.  Every 20th row starts with a
 * solid magenta cell (SGR 48;5;201) and the END row carries one too,
 * so any 25-row viewport page contains at least one marker for pixel
 * assertions.  >500 rows exhausts the terminal's 500-row scrollback
 * (term_top == TERM_SCROLLBACK) and every further bottom newline
 * takes the memmove branch (serial "[TERM] SCR 2").
 *
 * A shell builtin on purpose: m4k_spawn'd ELFs get their fds reset to
 * the console, so a spawned generator would bypass the GUI terminal
 * pipe entirely (verified the hard way).  Running inside m4shg keeps
 * out_fd on the terminal pipe.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

#define SCRS_DEFAULT_LINES 600

/**
 * musr_cmd_scrstress - emit N tagged rows plus one magenta END row
 * @ac: argument count; av[1] = optional row count (clamped 64..4000)
 * Return: void
 */
void musr_cmd_scrstress(int ac, char **av)
{
    int lines = SCRS_DEFAULT_LINES;
    int i;

    if (ac > 1 && av[1]) {
        lines = 0;
        for (i = 0; av[1][i] >= '0' && av[1][i] <= '9'; i++)
            lines = lines * 10 + (av[1][i] - '0');
        if (lines == 0)
            lines = SCRS_DEFAULT_LINES;
    }
    if (lines < 64)
        lines = 64;
    if (lines > 4000)
        lines = 4000;

    for (i = 0; i < lines; i++) {
        char buf[48];
        int p = 0;
        int v = i;
        int t = 0;
        char tmp[8];

        if (i % 20 == 0) {
            out_puts("\033[48;5;201m \033[0m");
        }
        buf[p++] = 'R';
        buf[p++] = 'O';
        buf[p++] = 'W';
        if (v == 0)
            buf[p++] = '0';
        while (v > 0 && t < 7) {
            tmp[t++] = (char)('0' + v % 10);
            v /= 10;
        }
        while (t > 0)
            buf[p++] = tmp[--t];
        buf[p++] = '\n';
        buf[p] = '\0';
        out_puts(buf);
    }
    out_puts("\033[48;5;201m \033[0mSCRSTRESS-END\n");
}
