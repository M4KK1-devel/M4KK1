/*
 * M4KK1 4P1 - ls.c
 * Description: ls command - list directory contents
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_ls - List directory entries
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_ls(int ac, char **av)
{
    const char *arg = (ac > 1) ? av[1] : NULL;
    char ap[256];
    if (arg)
        cwd_to_abs(arg, ap, 256);
    else {
        musr_strcpy(ap, cwd);
    }
    int fd = musr_sc_open(ap, O_RDONLY);
    if (fd < 0) {
        c_red();
        out_puts("ls: ");
        out_puts(ap);
        out_puts(": not found\n");
        c_rst();
        return;
    }
    struct dirent buf[32];
    int n = musr_sc_getdents(fd, buf, 32);
    musr_sc_close(fd);
    if (n <= 0)
        return;
    int dirs = 0, files = 0, links = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i].type == 2)
            dirs++;
        else if (buf[i].type == 3)
            links++;
        else
            files++;
    }
    out_puts("total ");
    print_u32(n);
    out_puts(" (");
    print_u32(dirs);
    out_puts(" dir, ");
    print_u32(files);
    out_puts(" file, ");
    print_u32(links);
    out_puts(" link)\n");
    for (int i = 0; i < n; i++) {
        if (buf[i].type == 2) {
            c_cyn();
            out_puts("d");
            c_wht();
        } else if (buf[i].type == 3) {
            c_mag();
            out_puts("l");
            c_wht();
        } else
            out_puts("-");
        out_puts("  ");
        print_u64(buf[i].inode);
        out_puts("  ");
        if (buf[i].type == 2 || buf[i].type == 3) {
            int pad = 7;
            uint32_t st = (uint32_t)(buf[i].size);
            do {
                pad--;
                st /= 10;
            } while (st > 0 && pad > 0);
            while (pad-- > 0)
                out_putc(' ');
            c_cyn();
            out_puts("-");
            c_wht();
        } else {
            uint32_t sz32 = (uint32_t)buf[i].size;
            int pad = 7;
            uint32_t st = sz32;
            do {
                pad--;
                st /= 10;
            } while (st > 0 && pad > 0);
            while (pad-- > 0)
                out_putc(' ');
            print_u32(sz32);
        }
        out_puts("  ");
        if (buf[i].type == 2) {
            c_cyn();
            out_puts(buf[i].name);
            out_puts("/");
            c_wht();
        } else if (buf[i].type == 3) {
            c_mag();
            out_puts(buf[i].name);
            c_wht();
        } else
            out_puts(buf[i].name);
        out_puts("\n");
    }
    c_rst();
}
