/*
 * M4KK1 4P1 - dd.c
 * Description: dd command - copy/convert raw data between
 *              files and block devices (if=/of=/bs=/count=).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

#define DD_MAX_BS 4096

/**
 * musr_cmd_dd - Copy data between input and output
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_dd(int ac, char **av)
{
    const char *in_path = 0;
    const char *out_path = 0;
    int bs = 512;
    int count = -1;

    for (int i = 1; i < ac; i++) {
        if (musr_strpref(av[i], "if=")) {
            in_path = av[i] + 3;
        } else if (musr_strpref(av[i], "of=")) {
            out_path = av[i] + 3;
        } else if (musr_strpref(av[i], "bs=")) {
            bs = musr_atoi(av[i] + 3);
        } else if (musr_strpref(av[i], "count=")) {
            count = musr_atoi(av[i] + 6);
        } else {
            out_puts("dd: unknown operand: ");
            out_puts(av[i]);
            out_puts("\n");
            return;
        }
    }

    if (!in_path || !out_path) {
        out_puts("usage: dd if=<in> of=<out> [bs=<n>] [count=<n>]\n");
        return;
    }
    if (bs <= 0)
        bs = 512;
    if (bs > DD_MAX_BS) {
        out_puts("dd: bs capped at ");
        print_u32(DD_MAX_BS);
        out_puts("\n");
        bs = DD_MAX_BS;
    }

    char in_abs[256], out_abs[256];
    cwd_to_abs(in_path, in_abs, sizeof(in_abs));
    cwd_to_abs(out_path, out_abs, sizeof(out_abs));

    int in_fd = musr_sc_open(in_abs, O_RDONLY);
    if (in_fd < 0) {
        c_red();
        out_puts("dd: cannot open ");
        out_puts(in_path);
        out_puts("\n");
        c_rst();
        return;
    }
    int out_fd = musr_sc_open(out_abs, O_CREAT | O_WRONLY | O_TRUNC);
    if (out_fd < 0) {
        c_red();
        out_puts("dd: cannot open ");
        out_puts(out_path);
        out_puts(" for writing\n");
        c_rst();
        musr_sc_close(in_fd);
        return;
    }

    static char buf[DD_MAX_BS];
    uint32_t total = 0;
    int blocks = 0;
    int max_blocks = (count < 0) ? 0x7FFFFFFF : count;

    while (blocks < max_blocks) {
        int n = musr_sc_read(in_fd, buf, bs);
        if (n <= 0)
            break;
        int w = musr_sc_write(out_fd, buf, n);
        if (w <= 0)
            break;
        total += (uint32_t)w;
        blocks++;
        if (w < n)
            break;
    }

    musr_sc_close(in_fd);
    musr_sc_close(out_fd);

    out_puts("dd: ");
    print_u32((uint32_t)blocks);
    out_puts(" block(s), ");
    print_u32(total);
    out_puts(" bytes copied\n");
}
