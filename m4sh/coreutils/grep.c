/*
 * M4KK1 4P1 - grep.c
 * Description: grep command - search text for pattern
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_grep - Search for a pattern in a file or stdin
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_grep(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: grep <pattern> [file]\n");
        return;
    }
    const char *pattern = av[1];
    int fd = -1;
    char buf[1024];
    if (ac >= 3) {
        char ap[256];
        cwd_to_abs(av[2], ap, 256);
        fd = musr_sc_open(ap, O_RDONLY);
        if (fd < 0) {
            c_red();
            out_puts("grep: ");
            out_puts(av[2]);
            out_puts(": not found\n");
            c_rst();
            return;
        }
    }
    int plen = musr_strlen(pattern), lines = 0;
    int bi = 0, li = 0;
    char line[512];
    if (fd >= 0) {
        int n;
        while ((n = musr_sc_read(fd, buf, 1024)) > 0) {
            for (int i = 0; i < n; i++) {
                line[li++] = buf[i];
                if (buf[i] == '\n' || li >= 510) {
                    line[li] = '\0';
                    for (int j = 0; j <= li - plen; j++) {
                        int m = 1;
                        for (int k = 0; k < plen; k++)
                            if (line[j + k] != pattern[k]) {
                                m = 0;
                                break;
                            }
                        if (m) {
                            out_puts(line);
                            out_puts("\n");
                            lines++;
                            break;
                        }
                    }
                    li = 0;
                }
            }
        }
        musr_sc_close(fd);
    }
    if (lines > 0) {
        c_grn();
        print_u32(lines);
        out_puts(" match(es)\n");
        c_rst();
    }
}
