/*
 * M4KK1 4P1 - umount.c
 * Description: umount command - unmount filesystems
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_umount - Unmount a filesystem
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_umount(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: umount <target>\n");
        return;
    }
    if (musr_sc_umount(av[1]) < 0) {
        c_red();
        out_puts("umount: failed\n");
        c_rst();
    }
}
