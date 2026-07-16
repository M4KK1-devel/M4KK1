/*
 * M4KK1 4P1 - aio.c
 * Description: Async I/O operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stddef.h>

int
mkrn_aio_read(void *pBuf, size_t count)
{
    (void)pBuf;
    (void)count;
    return -1;
}

int
mkrn_aio_write(const void *pBuf, size_t count)
{
    (void)pBuf;
    (void)count;
    return -1;
}
