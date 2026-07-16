/*
 * M4KK1 4P1 - open.c
 * Description: File open wrapper.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include "../include/console.h"

int
mkrn_open(const char *pPathname, int flags, int mode)
{
    (void)mode;
    int fd = mkrn_vfs_open(pPathname, flags);
    if (fd < 0) {
        mkrn_console_write("open: failed to open ");
        mkrn_console_write(pPathname);
        mkrn_console_write("\n");
    }
    return fd;
}
