/*
 * M4KK1 4P1 - touch.c
 * Description: touch command - create empty file
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_touch - Create an empty file or update its timestamp
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_touch(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: touch <file>\n");
        return;
    }
    char ap[256];
    cwd_to_abs(av[1], ap, 256);
    int fd = musr_sc_open(ap, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        c_red();
        out_puts("touch: ");
        out_puts(av[1]);
        out_puts(": error\n");
        c_rst();
    } else
        musr_sc_close(fd);
}
