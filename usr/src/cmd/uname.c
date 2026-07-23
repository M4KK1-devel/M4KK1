/*
 * M4KK1 4P1 - uname.c
 * Description: uname command - print system information
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static int has_flag(int ac, char **av, const char *f)
{
    for (int i = 1; i < ac; i++) {
        if (av[i][0] == '-' && av[i][1] != '-') {
            const char *p = av[i] + 1;
            while (*p) {
                if (*p == *f)
                    return 1;
                p++;
            }
        }
        if (musr_strcmp(av[i], "--help") == 0)
            return -1;
    }
    return 0;
}

/**
 * musr_cmd_uname - Print system information
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_uname(int ac, char **av)
{
    int a = 0, s = 0, n = 0, r = 0, v = 0, m = 0, p = 0, i = 0, o = 0;
    for (int j = 1; j < ac; j++) {
        if (av[j][0] == '-' && av[j][1] != '-') {
            const char *q = av[j] + 1;
            while (*q) {
                if (*q == 'a')
                    a = 1;
                else if (*q == 's')
                    s = 1;
                else if (*q == 'n')
                    n = 1;
                else if (*q == 'r')
                    r = 1;
                else if (*q == 'v')
                    v = 1;
                else if (*q == 'm')
                    m = 1;
                else if (*q == 'p')
                    p = 1;
                else if (*q == 'i')
                    i = 1;
                else if (*q == 'o')
                    o = 1;
                q++;
            }
        } else if (musr_strcmp(av[j], "--help") == 0) {
            out_puts("Usage: uname [OPTION]...\n");
            out_puts("Print system information.\n\n");
            out_puts("  -a, --all                print all\n");
            out_puts("  -s, --kernel-name        kernel name\n");
            out_puts("  -n, --nodename           hostname\n");
            out_puts("  -r, --kernel-release     kernel release\n");
            out_puts("  -v, --kernel-version     kernel version\n");
            out_puts("  -m, --machine            hardware name\n");
            out_puts("  -p, --processor          processor type\n");
            out_puts("  -i, --hardware-platform  platform\n");
            out_puts("  -o, --operating-system   OS\n");
            out_puts("      --help               display help\n");
            return;
        }
    }

    struct utsname u;
    if (musr_sc_uname(&u) < 0) {
        out_puts("uname: error\n");
        return;
    }

    if (a) {
        s = n = r = v = m = p = i = o = 1;
    }
    if (!s && !n && !r && !v && !m && !p && !i && !o)
        s = 1;

    int first = 1;
    if (s) {
        if (!first)
            out_putc(' ');
        out_puts(u.sysname);
        first = 0;
    }
    if (n) {
        if (!first)
            out_putc(' ');
        out_puts(u.nodename);
        first = 0;
    }
    if (r) {
        if (!first)
            out_putc(' ');
        out_puts(u.release);
        first = 0;
    }
    if (v) {
        if (!first)
            out_putc(' ');
        out_puts(u.version);
        first = 0;
    }
    if (m) {
        if (!first)
            out_putc(' ');
        out_puts(u.machine);
        first = 0;
    }
    if (p) {
        if (!first)
            out_putc(' ');
        out_puts(u.machine);
        first = 0;
    }
    if (i) {
        if (!first)
            out_putc(' ');
        out_puts(u.machine);
        first = 0;
    }
    if (o) {
        if (!first)
            out_putc(' ');
        out_puts(u.sysname);
        first = 0;
    }
    out_putc('\n');
}
