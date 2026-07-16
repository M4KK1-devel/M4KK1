/*
 * M4KK1 4P1 - file.c
 * Description: File table and file descriptor operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/vfs.h"
#include "../include/console.h"
#include <string.h>

static file_entry_t file_table[MAX_FILES];
static file_descriptor_t fd_table[MAX_FDS];
static bool bVfsInitialized = false;

/**
 * @brief Initialize the VFS layer (ramfs fallback).
 * @return 0 on success.
 */
int
mkrn_vfs_init(void)
{
    if (bVfsInitialized)
        return 0;

    mkrn_memset(file_table, 0, sizeof(file_table));
    mkrn_memset(fd_table, 0, sizeof(fd_table));

    file_entry_t *pRoot = &file_table[0];
    mkrn_strcpy(pRoot->name, "/");
    pRoot->type = FILE_TYPE_DIR;
    pRoot->in_use = true;

    bVfsInitialized = true;
    mkrn_console_write(
        "VFS: Initialized ramfs with 128 files, 256 FDs\n");
    return 0;
}

/**
 * @brief Find a file by name in the ramfs table.
 * @param pName  filename (leading slash optional)
 * @return pointer to file_entry_t, or NULL if not found.
 */
file_entry_t *
mkrn_vfs_find_file(const char *pName)
{
    if (!pName || !bVfsInitialized)
        return NULL;

    if (pName[0] == '/')
        pName++;

    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].in_use
            && mkrn_strcmp(file_table[i].name, pName) == 0)
        {
            return &file_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Create a file in the ramfs table.
 * @param pName  filename
 * @param type   file type
 * @return pointer to file_entry_t, or NULL on failure.
 */
file_entry_t *
mkrn_vfs_create_file(const char *pName, file_type_t type)
{
    if (!pName || !bVfsInitialized)
        return NULL;

    if (pName[0] == '/')
        pName++;

    if (mkrn_vfs_find_file(pName))
        return NULL;

    for (int i = 1; i < MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            mkrn_strncpy(file_table[i].name, pName,
                    MAX_FILENAME - 1);
            file_table[i].name[MAX_FILENAME - 1] = '\0';
            file_table[i].type = type;
            file_table[i].data = NULL;
            file_table[i].size = 0;
            file_table[i].capacity = 0;
            file_table[i].in_use = true;
            return &file_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Delete a file from the ramfs table.
 * @param pName  filename
 * @return 0 on success, -1 if not found.
 */
int
mkrn_vfs_delete_file(const char *pName)
{
    file_entry_t *pFile = mkrn_vfs_find_file(pName);
    if (!pFile)
        return -1;

    if (pFile->data)
        kfree(pFile->data);
    mkrn_memset(pFile, 0, sizeof(file_entry_t));
    return 0;
}

/**
 * @brief Open a file (ramfs backend).
 * @param pPathname  file path
 * @param flags      open flags (O_CREAT, O_TRUNC, O_APPEND)
 * @return file descriptor on success, -1 on error.
 */
int
mkrn_vfs_open(const char *pPathname, int flags)
{
    if (!pPathname || !bVfsInitialized)
        return -1;

    file_entry_t *pFile =
        mkrn_vfs_find_file(pPathname);

    if (!pFile) {
        if (flags & O_CREAT) {
            pFile = mkrn_vfs_create_file(
                pPathname, FILE_TYPE_REGULAR);
            if (!pFile)
                return -1;
        } else {
            return -1;
        }
    }

    if (flags & O_TRUNC) {
        if (pFile->data) {
            kfree(pFile->data);
            pFile->data = NULL;
        }
        pFile->size = 0;
        pFile->capacity = 0;
    }

    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].fd = i;
            fd_table[i].file = pFile;
            fd_table[i].offset =
                (flags & O_APPEND) ? pFile->size : 0;
            fd_table[i].flags = flags;
            fd_table[i].in_use = true;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Read from an open file descriptor.
 * @param fd     file descriptor
 * @param pBuf   destination buffer
 * @param count  number of bytes to read
 * @return number of bytes read, or -1 on error.
 */
ssize_t
mkrn_vfs_read(int fd, void *pBuf, size_t count)
{
    if (fd < 0 || fd >= MAX_FDS || !bVfsInitialized)
        return -1;

    file_descriptor_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    file_entry_t *pFile = pFdp->file;
    if (pFile->type != FILE_TYPE_REGULAR)
        return -1;

    if (pFdp->offset >= pFile->size)
        return 0;

    size_t uAvailable = pFile->size - pFdp->offset;
    size_t uToRead =
        (count < uAvailable) ? count : uAvailable;

    mkrn_memcpy(pBuf, pFile->data + pFdp->offset, uToRead);
    pFdp->offset += uToRead;
    return (ssize_t)uToRead;
}

/**
 * @brief Write to an open file descriptor.
 * @param fd     file descriptor
 * @param pBuf   source buffer
 * @param count  number of bytes to write
 * @return number of bytes written, or -1 on error.
 */
ssize_t
mkrn_vfs_write(int fd, const void *pBuf, size_t count)
{
    if (fd < 0 || fd >= MAX_FDS || !bVfsInitialized)
        return -1;

    file_descriptor_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    file_entry_t *pFile = pFdp->file;
    if (pFile->type != FILE_TYPE_REGULAR)
        return -1;

    uint32_t u32NewSize = pFdp->offset + (uint32_t)count;
    if (u32NewSize > pFile->capacity) {
        uint32_t u32NewCap =
            pFile->capacity ? pFile->capacity * 2 : 4096;
        while (u32NewCap < u32NewSize)
            u32NewCap *= 2;

        uint8_t *pNewData =
            (uint8_t *)kmalloc(u32NewCap);
        if (!pNewData)
            return -1;

        if (pFile->data) {
            mkrn_memcpy(pNewData, pFile->data, pFile->size);
            kfree(pFile->data);
        }
        pFile->data = pNewData;
        pFile->capacity = u32NewCap;
    }

    mkrn_memcpy(pFile->data + pFdp->offset, pBuf, count);
    pFdp->offset += (uint32_t)count;
    if (pFdp->offset > pFile->size)
        pFile->size = pFdp->offset;
    return (ssize_t)count;
}

/**
 * @brief Close a file descriptor.
 * @param fd  file descriptor to close
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_close(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !bVfsInitialized)
        return -1;

    if (!fd_table[fd].in_use)
        return -1;

    mkrn_memset(&fd_table[fd], 0, sizeof(file_descriptor_t));
    return 0;
}

/**
 * @brief Reposition the file offset.
 * @param fd      file descriptor
 * @param offset  offset value
 * @param whence  SEEK_SET, SEEK_CUR, or SEEK_END
 * @return new offset on success, -1 on error.
 */
int
mkrn_vfs_lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= MAX_FDS || !bVfsInitialized)
        return -1;

    file_descriptor_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    int new_offset;
    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = pFdp->offset + offset;
        break;
    case SEEK_END:
        new_offset = pFdp->file->size + offset;
        break;
    default:
        return -1;
    }

    if (new_offset < 0)
        new_offset = 0;
    if ((uint32_t)new_offset > pFdp->file->size)
        new_offset = (int)pFdp->file->size;

    pFdp->offset = (uint32_t)new_offset;
    return new_offset;
}

/**
 * @brief Mount a filesystem (debug wrapper).
 * @param pSource  source device
 * @param pTarget  mount point
 * @param pFstype  filesystem type
 * @return 0 on success.
 */
int
mkrn_vfs_mount(const char *pSource, const char *pTarget,
               const char *pFstype)
{
    mkrn_console_write("VFS: mount ");
    mkrn_console_write(pSource);
    mkrn_console_write(" -> ");
    mkrn_console_write(pTarget);
    mkrn_console_write(" (");
    mkrn_console_write(pFstype);
    mkrn_console_write(")\n");
    return 0;
}

/**
 * @brief Unmount a filesystem (debug wrapper).
 * @param pTarget  mount point
 * @return 0 on success.
 */
int
mkrn_vfs_umount(const char *pTarget)
{
    mkrn_console_write("VFS: umount ");
    mkrn_console_write(pTarget);
    mkrn_console_write("\n");
    return 0;
}
