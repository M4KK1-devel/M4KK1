/*
 * M4KK1 4P1 - backup_gui.c → backup command (CLI)
 * Description: backup command - copy /export into /export/backup-<n>
 *              (one-click document backup; CLI builtin now, the
 *              GUI panel draft was dropped for address-space room).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

#define BK_CHUNK 4096
static char bk_io[BK_CHUNK];

/* copy one file; returns bytes copied or -1 */
static int bk_copy_file(const char *src, const char *dst)
{
    int in = musr_sc_open((char *)src, O_RDONLY);
    if (in < 0)
        return -1;
    int out = musr_sc_open((char *)dst,
                           O_CREAT | O_WRONLY | O_TRUNC);
    if (out < 0) {
        musr_sc_close(in);
        return -1;
    }
    int total = 0;
    for (;;) {
        int n = musr_sc_read(in, bk_io, BK_CHUNK);
        if (n <= 0)
            break;
        int w = musr_sc_write(out, bk_io, n);
        if (w != n)
            break;
        total += n;
    }
    musr_sc_close(in);
    musr_sc_close(out);
    return total;
}

/**
 * musr_cmd_backup - back up /export to /export/backup-<n>
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_backup(int ac, char **av)
{
    (void)ac;
    (void)av;
    /* find the first free backup slot (backup-0..15) */
    int slot = 0;
    char dst[80];
    for (; slot < 16; slot++) {
        musr_strncpy(dst, "/export/backup-", 16);
        dst[15] = '0' + slot;
        dst[16] = 0;
        int fd = musr_sc_open(dst, O_RDONLY);
        if (fd < 0)
            break;
        musr_sc_close(fd);
    }
    if (slot >= 16) {
        out_puts("backup: all 16 slots used\n");
        return;
    }
    int copied = 0, files = 0;

    /* enumerate /export and copy regular files */
    int dfd = musr_sc_open("/export", O_RDONLY);
    if (dfd < 0) {
        out_puts("backup: cannot open /export\n");
        return;
    }
    struct dirent dbuf[24];
    int n = musr_sc_getdents(dfd, dbuf, 24);
    musr_sc_close(dfd);
    if (n <= 0) {
        out_puts("backup: /export empty or unreadable\n");
        return;
    }
    if (n > 24)
        n = 24;
    for (int i = 0; i < n; i++) {
        if (dbuf[i].name[0] == '.' || dbuf[i].type == 2)
            continue;
        char srcp[64], dstp[80];
        musr_strncpy(srcp, "/export/", 9);
        int j = 0;
        while (dbuf[i].name[j] && j < 50) {
            srcp[8 + j] = dbuf[i].name[j];
            j++;
        }
        srcp[8 + j] = 0;
        musr_strncpy(dstp, "/export/backup-", 16);
        int k = 15;
        int q = 0;
        while (dbuf[i].name[q] && k < 74) {
            dstp[k++] = dbuf[i].name[q++];
        }
        dstp[k] = 0;
        int r = bk_copy_file(srcp, dstp);
        if (r >= 0) {
            copied += r;
            files++;
        }
    }
    out_puts("backup: ");
    print_u32((uint32_t)files);
    out_puts(" files, ");
    print_u32((uint32_t)copied);
    out_puts(" bytes -> /export/backup-");
    out_putc('0' + slot);
    out_puts("\n");
}
