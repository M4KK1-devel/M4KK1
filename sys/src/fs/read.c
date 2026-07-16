/*
 * M4KK1 4P1 - read.c
 * Description: File read wrapper.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include <stdint.h>

ssize_t
mkrn_read(int fd, void *pBuf, size_t count)
{
    return mkrn_vfs_read(fd, pBuf, count);
}
