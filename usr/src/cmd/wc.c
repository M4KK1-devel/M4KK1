/*
 * M4KK1 4P1 - wc.c
 * Description: wc command - count lines, words, and bytes
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_wc - Count lines, words, and bytes in a file
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_wc(int ac, char **av)
{
    int fd = -1;
    char buf[1024];
    if (ac >= 2) {
        char ap[256];
        cwd_to_abs(av[1], ap, 256);
        fd = musr_sc_open(ap, O_RDONLY);
        if (fd < 0) {
            c_red();
            out_puts("wc: ");
            out_puts(av[1]);
            out_puts(": not found\n");
            c_rst();
            return;
        }
    }
    if (fd < 0) {
        ac = 1;
        int n;
        while ((n = musr_sc_read(0, buf, 1024)) > 0)
            musr_sc_write(out_fd, buf, n);
        return;
    }
    int lines = 0, words = 0, bytes = 0, inword = 0;
    int n;
    while ((n = musr_sc_read(fd, buf, 1024)) > 0) {
        for (int i = 0; i < n; i++) {
            bytes++;
            if (buf[i] == '\n')
                lines++;
            if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t')
                inword = 0;
            else if (!inword) {
                inword = 1;
                words++;
            }
        }
    }
    musr_sc_close(fd);
    out_puts(" ");
    print_u32(lines);
    out_puts("  ");
    print_u32(words);
    out_puts("  ");
    print_u32(bytes);
    if (ac >= 2) {
        out_puts("  ");
        out_puts(av[1]);
    }
    out_puts("\n");
}
