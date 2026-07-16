/*
 * M4KK1 4P1 - close.c
 * Description: File descriptor close operation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"

int
mkrn_close(int fd)
{
    return mkrn_vfs_close(fd);
}
