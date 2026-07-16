/*
 * M4KK1 4P1 - mount.c
 * Description: Mount/umount system call wrappers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include "../include/console.h"

int
mkrn_mount(const char *pSource, const char *pTarget,
           const char *pFstype, unsigned long flags,
           const void *pData)
{
    (void)flags;
    (void)pData;
    return mkrn_vfs_mount(pSource, pTarget, pFstype);
}

int
mkrn_umount(const char *pTarget)
{
    return mkrn_vfs_umount(pTarget);
}

int
mkrn_umount2(const char *pTarget, int flags)
{
    (void)flags;
    return mkrn_vfs_umount(pTarget);
}
