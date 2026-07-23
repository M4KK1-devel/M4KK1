/*
 * M4KK1 4P1 - cp.c
 * Description: cp command - copy files
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_cp - Copy a file from source to destination
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_cp(int ac, char **av)
{
    if (ac < 3) {
        out_puts("usage: cp <src> <dst>\n");
        return;
    }
    char src[256], dst[256];
    cwd_to_abs(av[1], src, 256);
    cwd_to_abs(av[2], dst, 256);
    int sfd = musr_sc_open(src, O_RDONLY);
    if (sfd < 0) {
        c_red();
        out_puts("cp: ");
        out_puts(av[1]);
        out_puts(": not found\n");
        c_rst();
        return;
    }
    int dfd = musr_sc_open(dst, O_CREAT | O_WRONLY | O_TRUNC);
    if (dfd < 0) {
        c_red();
        out_puts("cp: ");
        out_puts(av[2]);
        out_puts(": error\n");
        c_rst();
        musr_sc_close(sfd);
        return;
    }
    char buf[512];
    int n;
    while ((n = musr_sc_read(sfd, buf, 512)) > 0)
        musr_sc_write(dfd, buf, n);
    musr_sc_close(sfd);
    musr_sc_close(dfd);
}
