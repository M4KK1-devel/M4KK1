/*
 * M4KK1 4P1 - super.c
 * Description: Superblock / mount table operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include "../include/console.h"
#include <string.h>

static mount_entry_t mount_table[MAX_MOUNTS];
static bool bSuperInitialized = false;

/**
 * @brief Initialize the mount table superblock.
 * @return 0 on success, -1 on error.
 */
int
mkrn_super_init(void)
{
    if (bSuperInitialized)
        return 0;
    mkrn_memset(mount_table, 0, sizeof(mount_table));
    bSuperInitialized = true;
    mkrn_console_write(
        "super: mount table initialized\n");
    return 0;
}

/**
 * @brief Mount a filesystem.
 * @param pSource  source device path
 * @param pTarget  mount point path
 * @param pFstype  filesystem type string
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_mount(const char *pSource, const char *pTarget,
               const char *pFstype)
{
    if (!pSource || !pTarget || !pFstype)
        return -1;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].mounted) {
            mkrn_strncpy(mount_table[i].source, pSource,
                    MAX_FILENAME - 1);
            mkrn_strncpy(mount_table[i].target, pTarget,
                    MAX_FILENAME - 1);
            mkrn_strncpy(mount_table[i].fstype, pFstype, 31);
            mount_table[i].mounted = true;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Unmount a filesystem.
 * @param pTarget  mount point path
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_umount(const char *pTarget)
{
    if (!pTarget)
        return -1;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].mounted
            && mkrn_strcmp(mount_table[i].target, pTarget) == 0)
        {
            mkrn_memset(&mount_table[i], 0,
                   sizeof(mount_entry_t));
            return 0;
        }
    }
    return -1;
}
