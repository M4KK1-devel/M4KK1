/*
 * M4KK1 4P1 - diff.c
 * Description: diff command - compare two files
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static int read_file_buf(int fd, char *buf, int max)
{
    int pos = 0, n;
    while ((n = musr_sc_read(fd, buf + pos,
                             (max - pos > 512) ? 512 : (max - pos))) > 0)
        pos += n;
    return pos;
}

/**
 * musr_cmd_diff - Compare two files line by line
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_diff(int ac, char **av)
{
    if (ac < 3) {
        out_puts("usage: diff <file1> <file2>\n");
        return;
    }
    char ap1[256], ap2[256];
    cwd_to_abs(av[1], ap1, 256);
    cwd_to_abs(av[2], ap2, 256);
    int fd1 = musr_sc_open(ap1, O_RDONLY);
    if (fd1 < 0) {
        c_red();
        out_puts("diff: ");
        out_puts(av[1]);
        out_puts(": not found\n");
        c_rst();
        return;
    }
    int fd2 = musr_sc_open(ap2, O_RDONLY);
    if (fd2 < 0) {
        c_red();
        out_puts("diff: ");
        out_puts(av[2]);
        out_puts(": not found\n");
        c_rst();
        musr_sc_close(fd1);
        return;
    }
    char buf1[4096], buf2[4096];
    int n1 = read_file_buf(fd1, buf1, 4096);
    int n2 = read_file_buf(fd2, buf2, 4096);
    musr_sc_close(fd1);
    musr_sc_close(fd2);
    char *lines1[128], *lines2[128];
    int lc1 = 0, lc2 = 0;
    lines1[lc1++] = buf1;
    for (int i = 0; i < n1 && lc1 < 128; i++)
        if (buf1[i] == '\n') {
            buf1[i] = '\0';
            if (i + 1 < n1)
                lines1[lc1++] = &buf1[i + 1];
        }
    lines2[lc2++] = buf2;
    for (int i = 0; i < n2 && lc2 < 128; i++)
        if (buf2[i] == '\n') {
            buf2[i] = '\0';
            if (i + 1 < n2)
                lines2[lc2++] = &buf2[i + 1];
        }
    int diffs = 0, max = (lc1 > lc2) ? lc1 : lc2;
    for (int i = 0; i < max; i++) {
        const char *l1 = (i < lc1) ? lines1[i] : "";
        const char *l2 = (i < lc2) ? lines2[i] : "";
        if (musr_strcmp(l1, l2) != 0) {
            diffs++;
            c_red();
            out_puts("< ");
            c_rst();
            out_puts(l1);
            out_puts("\n");
            c_grn();
            out_puts("> ");
            c_rst();
            out_puts(l2);
            out_puts("\n");
        }
    }
    if (diffs == 0)
        out_puts("(no differences)\n");
}
