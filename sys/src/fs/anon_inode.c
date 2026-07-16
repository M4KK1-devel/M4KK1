/*
 * M4KK1 4P1 - anon_inode.c
 * Description: Anonymous inode file descriptor allocation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>

int
mkrn_anon_inode_getfd(const char *pName)
{
    (void)pName;
    return -1;
}
