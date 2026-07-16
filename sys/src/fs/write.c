/*
 * M4KK1 4P1 - write.c
 * Description: File write wrapper.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include <stdint.h>

ssize_t
mkrn_write(int fd, const void *pBuf, size_t count)
{
    return mkrn_vfs_write(fd, pBuf, count);
}
