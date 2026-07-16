/*
 * M4KK1 4P1 - cat.c
 * Description: cat command - concatenate and display files
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_cat - Print file contents to stdout
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_cat(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: cat <file>\n");
        return;
    }
    char ap[256];
    cwd_to_abs(av[1], ap, 256);
    int fd = musr_sc_open(ap, O_RDONLY);
    if (fd < 0) {
        c_red();
        out_puts("cat: ");
        out_puts(av[1]);
        out_puts(": not found\n");
        c_rst();
        return;
    }
    char buf[513];
    int n;
    while ((n = musr_sc_read(fd, buf, 512)) > 0) {
        buf[n] = '\0';
        out_puts(buf);
    }
    musr_sc_close(fd);
}
