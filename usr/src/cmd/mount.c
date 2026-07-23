/*
 * M4KK1 4P1 - mount.c
 * Description: mount command - mount filesystems
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_mount - Mount filesystems or show mount info
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_mount(int ac, char **av)
{
    if (ac >= 4) {
        if (musr_sc_mount(av[1], av[2], av[3]) < 0) {
            c_red();
            out_puts("mount: failed\n");
            c_rst();
        }
        return;
    }
    struct mount_entry buf[MOUNT_MAX];
    int n = musr_sc_mountinfo(buf, MOUNT_MAX);
    if (n <= 0) {
        out_puts("mount: no filesystems mounted\n");
        return;
    }
    c_ylw();
    out_puts("source                 target                fstype\n");
    c_wht();
    for (int i = 0; i < n; i++) {
        out_puts(buf[i].source);
        out_puts("  ");
        out_puts(buf[i].target);
        out_puts("  ");
        out_puts(buf[i].fstype);
        out_putc('\n');
    }
}
