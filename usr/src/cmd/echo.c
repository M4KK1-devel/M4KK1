/*
 * M4KK1 4P1 - echo.c
 * Description: echo command - print arguments with escape sequence support
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static void echo_puts(const char *s)
{
    while (*s) {
        if (*s == '\\' && s[1]) {
            s++;
            switch (*s) {
                case 'n': out_putc('\n'); break;
                case 't': out_putc('\t'); break;
                case 'r': out_putc('\r'); break;
                case '\\': out_putc('\\'); break;
                case 'a': out_putc('\a'); break;
                case 'b': out_putc('\b'); break;
                case 'f': out_putc('\f'); break;
                case 'v': out_putc('\v'); break;
                case '0': {
                    int val = 0;
                    s++;
                    for (int i = 0; i < 3 && *s >= '0' && *s <= '7'; i++) {
                        val = val * 8 + (*s - '0');
                        s++;
                    }
                    out_putc(val);
                    continue;
                }
                default:
                    out_putc(*s);
                    break;
            }
            s++;
        } else {
            out_putc(*s++);
        }
    }
}

void musr_cmd_echo(int ac, char **av)
{
    int interpret_escapes = 1;
    int no_newline = 0;
    int start = 1;

    for (int i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "-e") == 0) {
            interpret_escapes = 1;
            start++;
        } else if (musr_strcmp(av[i], "-E") == 0) {
            interpret_escapes = 0;
            start++;
        } else if (musr_strcmp(av[i], "-n") == 0) {
            no_newline = 1;
            start++;
        } else {
            break;
        }
    }

    for (int i = start; i < ac; i++) {
        if (i > start)
            out_putc(' ');
        if (interpret_escapes)
            echo_puts(av[i]);
        else
            out_puts(av[i]);
    }
    if (!no_newline)
        out_putc('\n');
}
