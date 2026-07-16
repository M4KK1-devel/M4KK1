/*
 * M4KK1 4P1 - filesys.c
 * Description: Filesystem layer initialization.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include "../include/console.h"
#include <string.h>

int
mkrn_filesys_init(void)
{
    mkrn_console_write(
        "filesys: initializing filesystem layer\n");
    return mkrn_vfs_init();
}
