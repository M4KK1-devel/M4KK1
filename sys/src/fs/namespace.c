/*
 * M4KK1 4P1 - namespace.c
 * Description: Directory namespace operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>

int
mkrn_chdir(const char *pPath)
{
    (void)pPath;
    return -1;
}

int
mkrn_getcwd(char *pBuf, size_t size)
{
    if (pBuf && size > 0) {
        pBuf[0] = '/';
        pBuf[1] = '\0';
        return 1;
    }
    return -1;
}
