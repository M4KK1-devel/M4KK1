/*
 * M4KK1 4P1 - vfs.c
 * Description: Virtual filesystem layer — YAFS + pipe + ramfs.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "vfs.h"
#include "console.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"
#include "sessions.h"
#include "device_tree.h"
#include <string.h>
#include <yafs.h>
#include <yafs_btree.h>

#define PIPE_BUF_SIZE 4096
#define MAX_PIPES 32

typedef struct {
    uint8_t  data[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t bytes;
    bool     in_use;
    bool     read_closed;
    bool     write_closed;
} pipe_buffer_t;

static pipe_buffer_t pipe_buffers[MAX_PIPES];

mkrn_file_ent_t file_table[M4K_VFS_MAX_FILES];
mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
static bool bVfsInitialized = false;
static mkrn_mount_ent_t mount_table[M4K_VFS_MAX_MOUNTS];
static int mount_count;

static struct yafs_mount *pRootYafsMount = NULL;
uint64_t root_yafs_tree = 0;
static uint64_t u64VfsNextIno = 1000;

extern int mkrn_yafs_readdir(
    uint64_t u64RootTree, uint64_t u64DirInode,
    struct mkrn_vfs_dirent *pBuf, uint32_t u32MaxEntries);
extern int mkrn_yafs_read_file_data(
    struct yafs_mount *pM, uint64_t u64Inode,
    void *pBuf, uint64_t u64Offset,
    uint32_t u32Size, uint32_t *pRead);
extern int mkrn_yafs_write_file_data(
    struct yafs_mount *pM, uint64_t u64Inode,
    const void *pBuf, uint64_t u64Offset,
    uint32_t u32Size, uint32_t *pWritten);
extern uint64_t mkrn_yafs_dev_alloc_block(void);
extern void mkrn_yafs_dev_free_block(uint64_t u64Lba);
extern int mkrn_yafs_dev_read(uint64_t u64Lba, void *pBuf);
extern int mkrn_yafs_dev_write(
    uint64_t u64Lba, const void *pBuf);
extern uint64_t mkrn_yafs_name_hash(const char *pName);
extern int mkrn_yafs_create_fhs(uint64_t *pRootLba);

/**
 * @brief Initialize the VFS layer.
 * @return 0 on success.
 */
int
mkrn_vfs_init(void)
{
    if (bVfsInitialized)
        return 0;

    mkrn_memset(file_table, 0, sizeof(file_table));
    mkrn_memset(fd_table, 0, sizeof(fd_table));
    mkrn_memset(mount_table, 0, sizeof(mount_table));
    mount_count = 0;

    mkrn_file_ent_t *pRoot = &file_table[0];
    mkrn_strcpy(pRoot->name, "/");
    pRoot->type = M4K_FILE_DIR;
    pRoot->in_use = true;

    bVfsInitialized = true;
    mkrn_console_write(
        "VFS: Initialized ramfs with 128 files, 256 FDs\n");
    return 0;
}

/**
 * @brief Set the YAFS root mount for VFS operations.
 * @param pM               YAFS mount pointer
 * @param u64RootTreeAddr  root tree LBA
 */
void
mkrn_vfs_set_root_yafs(struct yafs_mount *pM,
                       uint64_t u64RootTreeAddr)
{
    pRootYafsMount = pM;
    root_yafs_tree = u64RootTreeAddr;
    mkrn_console_write(
        "VFS: YAFS root mounted, tree at LBA 0x");
    mkrn_console_write_hex(u64RootTreeAddr);
    mkrn_console_write("\n");
}

/**
 * @brief Find a file in the ramfs table.
 * @param pName  filename
 * @return file entry pointer or NULL.
 */
mkrn_file_ent_t *
mkrn_vfs_find_file(const char *pName)
{
    if (!pName || !bVfsInitialized)
        return NULL;
    if (pName[0] == '/')
        pName++;
    for (int i = 0; i < M4K_VFS_MAX_FILES; i++) {
        if (file_table[i].in_use
            && mkrn_strcmp(file_table[i].name, pName) == 0)
            return &file_table[i];
    }
    return NULL;
}

/**
 * @brief Create a file in the ramfs table.
 * @param pName  filename
 * @param type   file type
 * @return file entry pointer or NULL.
 */
mkrn_file_ent_t *
mkrn_vfs_create_file(const char *pName, mkrn_file_type_t type)
{
    if (!pName || !bVfsInitialized)
        return NULL;
    if (pName[0] == '/')
        pName++;
    if (mkrn_vfs_find_file(pName))
        return NULL;
    for (int i = 1; i < M4K_VFS_MAX_FILES; i++) {
        if (!file_table[i].in_use) {
            mkrn_strncpy(file_table[i].name, pName,
                    M4K_VFS_MAX_FILENAME - 1);
            file_table[i].name[M4K_VFS_MAX_FILENAME - 1] = '\0';
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
    mkrn_file_ent_t *pFile = mkrn_vfs_find_file(pName);
    if (!pFile)
        return -1;
    if (pFile->data)
        mkrn_free(pFile->data);
    mkrn_memset(pFile, 0, sizeof(mkrn_file_ent_t));
    return 0;
}

/**
 * @brief Check whether a file descriptor is in use.
 * @param fd  file descriptor number
 * @return 1 if in use, 0 otherwise.
 */
int
mkrn_vfs_fd_in_use(int fd)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS)
        return 0;
    return fd_table[fd].in_use ? 1 : 0;
}

/**
 * @brief Create a pipe.
 * @param fd  output array for read/write FDs [2]
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_pipe(int fd[2])
{
    int pi = -1;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipe_buffers[i].in_use) {
            pi = i;
            break;
        }
    }
    if (pi < 0)
        return -1;
    mkrn_memset(&pipe_buffers[pi], 0, sizeof(pipe_buffer_t));
    pipe_buffers[pi].in_use = true;

    int rfd = -1, wfd = -1;
    for (int i = 3; i < M4K_VFS_MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            if (rfd < 0)
                rfd = i;
            else if (wfd < 0) {
                wfd = i;
                break;
            }
        }
    }
    if (rfd < 0 || wfd < 0) {
        pipe_buffers[pi].in_use = false;
        return -1;
    }
    fd[0] = rfd;
    fd[1] = wfd;

    fd_table[rfd].fd = rfd;
    fd_table[rfd].file = &file_table[0];
    fd_table[rfd].offset = (uint32_t)pi;
    fd_table[rfd].flags = M4K_O_RDONLY;
    fd_table[rfd].in_use = true;
    fd_table[rfd].yafs_inode = 0;

    fd_table[wfd].fd = wfd;
    fd_table[wfd].file = &file_table[0];
    fd_table[wfd].offset = (uint32_t)pi;
    fd_table[wfd].flags = M4K_O_WRONLY;
    fd_table[wfd].in_use = true;
    fd_table[wfd].yafs_inode = 0;
    return 0;
}

/**
 * @brief Duplicate a file descriptor.
 * @param oldfd  existing FD
 * @param newfd  target FD
 * @return newfd on success, -1 on error.
 */
int
mkrn_vfs_dup2(int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= M4K_VFS_MAX_FDS
        || newfd < 0 || newfd >= M4K_VFS_MAX_FDS)
        return -1;
    if (!fd_table[oldfd].in_use)
        return -1;
    /* POSIX: dup2(fd, fd) is a no-op that returns fd — closing
     * newfd first would destroy the shared entry. */
    if (oldfd == newfd)
        return newfd;
    if (fd_table[newfd].in_use)
        mkrn_vfs_close(newfd);
    fd_table[newfd] = fd_table[oldfd];
    fd_table[newfd].fd = newfd;
    return newfd;
}

static int
split_parent_name(const char *pPath, char *pParent,
                   int psize, const char **ppNameOut)
{
    if (!pPath || !*pPath)
        return -1;
    const char *pSlash = NULL;
    const char *p = pPath;
    while (*p) {
        if (*p == '/')
            pSlash = p;
        p++;
    }
    if (!pSlash) {
        *ppNameOut = pPath;
        mkrn_strcpy(pParent, "/");
        return 0;
    }
    *ppNameOut = pSlash + 1;
    int plen = (int)(pSlash - pPath);
    if (plen == 0) {
        mkrn_strcpy(pParent, "/");
        return 0;
    }
    if (plen >= psize)
        return -1;
    int i;
    for (i = 0; i < plen; i++)
        pParent[i] = pPath[i];
    pParent[i] = '\0';
    return 0;
}

/**
 * @brief Create a directory on YAFS.
 * @param pPathname  path to create
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_mkdir(const char *pPathname)
{
    if (!pPathname || !bVfsInitialized
        || root_yafs_tree == 0)
        return -1;
    char parent_path[256];
    const char *pName;
    if (split_parent_name(
            pPathname, parent_path,
            sizeof(parent_path), &pName)
        < 0)
        return -1;
    uint64_t u64Parent = mkrn_yafs_lookup_path(
        root_yafs_tree, parent_path);
    if (u64Parent == 0)
        return -1;
    if (mkrn_yafs_lookup_name(
            root_yafs_tree, u64Parent, pName, NULL)
        == 0)
        return -1;
    uint64_t u64Ino = u64VfsNextIno++;
    uint64_t u64Dummy;
    if (mkrn_yafs_mkdir(&root_yafs_tree, u64Parent,
                        pName, u64Ino, &u64Dummy)
        < 0)
        return -1;
    return 0;
}

/**
 * @brief Unlink a file or directory on YAFS.
 * @param pPathname  path to unlink
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_unlink(const char *pPathname)
{
    if (!pPathname || !bVfsInitialized
        || root_yafs_tree == 0)
        return -1;
    char parent_path[256];
    const char *pName;
    if (split_parent_name(
            pPathname, parent_path,
            sizeof(parent_path), &pName)
        < 0)
        return -1;
    uint64_t u64Parent = mkrn_yafs_lookup_path(
        root_yafs_tree, parent_path);
    if (u64Parent == 0)
        return -1;
    return mkrn_yafs_unlink(
        &root_yafs_tree, u64Parent, pName);
}

/**
 * @brief Remove a directory (alias for unlink).
 */
int
mkrn_vfs_rmdir(const char *pPathname)
{
    return mkrn_vfs_unlink(pPathname);
}

/**
 * @brief Rename a file or directory on YAFS.
 * @param pOldpath  old path
 * @param pNewpath  new path
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_rename(const char *pOldpath,
                const char *pNewpath)
{
    if (!pOldpath || !pNewpath || !bVfsInitialized
        || root_yafs_tree == 0)
        return -1;
    char old_parent[256], new_parent[256];
    const char *pOldName, *pNewName;
    if (split_parent_name(
            pOldpath, old_parent,
            sizeof(old_parent), &pOldName)
        < 0)
        return -1;
    if (split_parent_name(
            pNewpath, new_parent,
            sizeof(new_parent), &pNewName)
        < 0)
        return -1;
    uint64_t u64Op = mkrn_yafs_lookup_path(
        root_yafs_tree, old_parent);
    uint64_t u64Np = mkrn_yafs_lookup_path(
        root_yafs_tree, new_parent);
    if (u64Op == 0 || u64Np == 0)
        return -1;
    return mkrn_yafs_rename(
        &root_yafs_tree, u64Op, pOldName,
        u64Np, pNewName);
}

/**
 * @brief Create a regular file on YAFS.
 * @param pPathname  path to create
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_create_file_yafs(const char *pPathname)
{
    if (!pPathname || !bVfsInitialized
        || root_yafs_tree == 0)
        return -1;
    char parent_path[256];
    const char *pName;
    if (split_parent_name(
            pPathname, parent_path,
            sizeof(parent_path), &pName)
        < 0)
        return -1;
    uint64_t u64Parent = mkrn_yafs_lookup_path(
        root_yafs_tree, parent_path);
    if (u64Parent == 0)
        return -1;
    uint64_t u64Existing;
    if (mkrn_yafs_lookup_name(
            root_yafs_tree, u64Parent, pName,
            &u64Existing)
        == 0)
        return -1;
    uint64_t u64Ino = u64VfsNextIno++;
    uint64_t u64Lba = mkrn_yafs_dev_alloc_block();
    if (u64Lba == 0)
        return -1;
    /* Static scratch (see read_inode_raw): same non-reentrancy
     * argument, one shared 4 KB BSS buffer for raw inode blocks. */
    static uint8_t iv_buf[4096];
    mkrn_memset(iv_buf, 0, sizeof(iv_buf));
    struct yafs_inode_value *pIv =
        (struct yafs_inode_value *)iv_buf;
    pIv->file_type = YAFS_FT_REG_FILE;
    pIv->link_count = 1;
    pIv->uid = M4K_UID_ROOT;
    pIv->gid = M4K_GID_ROOT;
    pIv->mode = M4K_IRUSR | M4K_IWUSR | M4K_IRGRP | M4K_IROTH;
    mkrn_strncpy(pIv->name, pName, sizeof(pIv->name) - 1);
    pIv->name[sizeof(pIv->name) - 1] = '\0';
    if (mkrn_yafs_dev_write(u64Lba, iv_buf) != 0)
        return -1;
    bool bInserted;
    uint64_t u64InodeKey = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64Ino);
    if (mkrn_yafs_btree_insert(
            &root_yafs_tree, u64InodeKey, u64Lba,
            &bInserted)
        != 0) {
        mkrn_yafs_dev_free_block(u64Lba);
        return -1;
    }
    uint64_t u64Hash = mkrn_yafs_name_hash(pName);
    uint64_t u64DirKey = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64Parent << 16) | (u64Hash & 0xFFFF));
    if (mkrn_yafs_btree_insert(
            &root_yafs_tree, u64DirKey, u64Ino,
            &bInserted)
        != 0 || !bInserted) {
        /* Roll back the inode-key insert so a failed dir-entry
         * insert doesn't leak the allocated block + orphan key.
         * !bInserted = name-hash collision with a different sibling
         * — proceeding would silently overwrite that entry (data
         * loss), so refuse like create_link does. */
        bool bDeleted;
        mkrn_yafs_btree_delete(
            &root_yafs_tree, u64InodeKey, &bDeleted);
        mkrn_yafs_dev_free_block(u64Lba);
        return -1;
    }
    return 0;
}

/**
 * @brief Open a file (YAFS + ramfs fallback).
 * @param pPathname  file path
 * @param flags      open flags
 * @return file descriptor, or -1 on error.
 */
int
mkrn_vfs_open(const char *pPathname, int flags)
{
    if (!pPathname || !bVfsInitialized)
        return -1;

    /* Basic permission check: non-root cannot access /sys or /boot
     * (exact component match — a bare prefix compare would also
     * reject unrelated names like "/syshello" or "/bootcamp"). */
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (pCur && pCur->euid != M4K_UID_ROOT) {
        if (pPathname[0] == '/'
            && (mkrn_strcmp(pPathname, "/sys") == 0
                || mkrn_strncmp(pPathname, "/sys/", 5) == 0
                || mkrn_strcmp(pPathname, "/boot") == 0
                || mkrn_strncmp(pPathname, "/boot/", 6) == 0)) {
            return -1;
        }
    }

    /* Route /sys/proc/ to ProcFS */
    if (pPathname[0] == '/' && strncmp(pPathname, "/sys/proc", 9) == 0) {
        int fd;
        if (mkrn_procfs_open(pPathname, flags, &fd) == 0)
            return fd;
        return -1;
    }

    /* Route /sys/sessions/ to session tracking */
    if (pPathname[0] == '/' && strncmp(pPathname, "/sys/sessions", 13) == 0) {
        int fd;
        if (mkrn_sessions_open(pPathname, flags, &fd) == 0)
            return fd;
        return -1;
    }

    /* Route /device/ to Device Tree */
    if (pPathname[0] == '/' && strncmp(pPathname, "/device", 7) == 0) {
        int fd;
        if (mkrn_device_tree_open(pPathname, flags, &fd) == 0)
            return fd;
        return -1;
    }

    /* Route /dev and /dev/... to DevFS (block device nodes) */
    if (pPathname[0] == '/'
        && (strncmp(pPathname, "/dev/", 5) == 0
            || mkrn_strcmp(pPathname, "/dev") == 0)) {
        int fd;
        if (mkrn_devfs_open(pPathname, flags, &fd) == 0)
            return fd;
        return -1;
    }

    if (root_yafs_tree != 0) {
        bool bIsRoot =
            (pPathname[0] == '/' && pPathname[1] == '\0');
        uint64_t u64Inode =
            bIsRoot
                ? 1
                : mkrn_yafs_lookup_path(
                      root_yafs_tree, pPathname);
        if (u64Inode == 0 && (flags & M4K_O_CREAT)) {
            if (mkrn_vfs_create_file_yafs(pPathname) < 0)
                return -1;
            u64Inode = mkrn_yafs_lookup_path(
                root_yafs_tree, pPathname);
        }
        if (u64Inode == 0)
            return -1;
        for (int i = 0; i < M4K_VFS_MAX_FDS; i++) {
            if (!fd_table[i].in_use) {
                fd_table[i].fd = i;
                fd_table[i].file = &file_table[0];
                fd_table[i].offset = 0;
                fd_table[i].flags = flags;
                fd_table[i].in_use = true;
                fd_table[i].yafs_inode = u64Inode;
                return i;
            }
        }
        return -1;
    }

    mkrn_file_ent_t *pFile =
        mkrn_vfs_find_file(pPathname);
    if (!pFile) {
        if (flags & M4K_O_CREAT) {
            pFile = mkrn_vfs_create_file(
                pPathname, M4K_FILE_REGULAR);
            if (!pFile)
                return -1;
        } else {
            return -1;
        }
    }
    if (flags & M4K_O_TRUNC) {
        if (pFile->data) {
            mkrn_free(pFile->data);
            pFile->data = NULL;
        }
        pFile->size = 0;
        pFile->capacity = 0;
    }
    for (int i = 0; i < M4K_VFS_MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].fd = i;
            fd_table[i].file = pFile;
            fd_table[i].offset =
                (flags & M4K_O_APPEND) ? pFile->size : 0;
            fd_table[i].flags = flags;
            fd_table[i].in_use = true;
            fd_table[i].yafs_inode = 0;
            return i;
        }
    }
    return -1;
}

/**
 * @brief Read from a file descriptor.
 * @param fd     file descriptor
 * @param pBuf   destination buffer
 * @param count  bytes to read
 * @return bytes read, or -1 on error.
 */
int
mkrn_vfs_read(int fd, void *pBuf, size_t count)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !bVfsInitialized)
        return -1;

    /* Route ProcFS reads */
    if (mkrn_procfs_is_procfs_fd(fd))
        return mkrn_procfs_read(fd, pBuf, (uint32_t)count);

    /* Route sessions reads */
    if (mkrn_sessions_is_sessions_fd(fd))
        return mkrn_sessions_read(fd, pBuf, (uint32_t)count);

    /* Route device tree reads */
    if (mkrn_device_tree_is_dt_fd(fd))
        return mkrn_device_tree_read(fd, pBuf, (uint32_t)count);

    /* Route DevFS reads */
    if (mkrn_devfs_is_devfs_fd(fd))
        return mkrn_devfs_read(fd, pBuf, (uint32_t)count);

    if (fd_table[fd].file == &file_table[0]
        && fd_table[fd].yafs_inode == 0)
    {
        int pi = (int)fd_table[fd].offset;
        if (pi < 0 || pi >= MAX_PIPES
            || !pipe_buffers[pi].in_use)
            return -1;
        pipe_buffer_t *pPb = &pipe_buffers[pi];
        if (pPb->read_closed)
            return -1;
        uint32_t u32Total = 0;
        while (u32Total < count && pPb->bytes > 0) {
            /* Copy in contiguous segments instead of per-byte so a
             * 4 KB pipe read is 1–2 memcpys, not 4096 byte moves. */
            uint32_t u32Seg = PIPE_BUF_SIZE - pPb->read_pos;
            if (u32Seg > pPb->bytes)
                u32Seg = pPb->bytes;
            if (u32Seg > count - u32Total)
                u32Seg = count - u32Total;
            mkrn_memcpy((uint8_t *)pBuf + u32Total,
                   pPb->data + pPb->read_pos, u32Seg);
            pPb->read_pos =
                (pPb->read_pos + u32Seg) % PIPE_BUF_SIZE;
            pPb->bytes -= u32Seg;
            u32Total += u32Seg;
        }
        return (int)u32Total;
    }

    mkrn_file_desc_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    if (pFdp->yafs_inode != 0 && root_yafs_tree != 0) {
        uint32_t u32Actual;
        if (mkrn_yafs_read_file_data(
                NULL, pFdp->yafs_inode, pBuf,
                pFdp->offset, (uint32_t)count,
                &u32Actual)
            != 0)
            return -1;
        pFdp->offset += u32Actual;
        return (int)u32Actual;
    }

    mkrn_file_ent_t *pFile = pFdp->file;
    if (pFile->type != M4K_FILE_REGULAR)
        return -1;
    if (pFdp->offset >= pFile->size)
        return 0;
    size_t uAvailable = pFile->size - pFdp->offset;
    size_t uToRead =
        (count < uAvailable) ? count : uAvailable;
    mkrn_memcpy(pBuf, pFile->data + pFdp->offset, uToRead);
    pFdp->offset += (uint32_t)uToRead;
    return (int)uToRead;
}

/**
 * @brief Write to a file descriptor.
 * @param fd     file descriptor
 * @param pBuf   source buffer
 * @param count  bytes to write
 * @return bytes written, or -1 on error.
 */
int
mkrn_vfs_write(int fd, const void *pBuf, size_t count)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !bVfsInitialized)
        return -1;

    /* Route ProcFS writes */
    if (mkrn_procfs_is_procfs_fd(fd))
        return mkrn_procfs_write(fd, pBuf, (uint32_t)count);

    /* Route sessions writes */
    if (mkrn_sessions_is_sessions_fd(fd))
        return mkrn_sessions_write(fd, pBuf, (uint32_t)count);

    /* Route device tree writes */
    if (mkrn_device_tree_is_dt_fd(fd))
        return mkrn_device_tree_write(fd, pBuf, (uint32_t)count);

    /* Route DevFS writes */
    if (mkrn_devfs_is_devfs_fd(fd))
        return mkrn_devfs_write(fd, pBuf, (uint32_t)count);

    if (fd_table[fd].file == &file_table[0]
        && fd_table[fd].yafs_inode == 0)
    {
        int pi = (int)fd_table[fd].offset;
        if (pi < 0 || pi >= MAX_PIPES
            || !pipe_buffers[pi].in_use)
            return -1;
        pipe_buffer_t *pPb = &pipe_buffers[pi];
        if (pPb->write_closed)
            return -1;
        uint32_t u32Total = 0;
        while (u32Total < count
               && pPb->bytes < PIPE_BUF_SIZE)
        {
            /* Contiguous free run: to buffer end, or up to read_pos
             * when the tail wraps — 1–2 memcpys per write. */
            uint32_t u32Seg = PIPE_BUF_SIZE - pPb->write_pos;
            if (u32Seg > PIPE_BUF_SIZE - pPb->bytes)
                u32Seg = PIPE_BUF_SIZE - pPb->bytes;
            if (u32Seg > count - u32Total)
                u32Seg = count - u32Total;
            mkrn_memcpy(pPb->data + pPb->write_pos,
                   (const uint8_t *)pBuf + u32Total, u32Seg);
            pPb->write_pos =
                (pPb->write_pos + u32Seg) % PIPE_BUF_SIZE;
            pPb->bytes += u32Seg;
            u32Total += u32Seg;
        }
        return (int)u32Total;
    }

    mkrn_file_desc_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    if (pFdp->yafs_inode != 0 && root_yafs_tree != 0) {
        uint32_t u32Actual;
        if (mkrn_yafs_write_file_data(
                NULL, pFdp->yafs_inode, pBuf,
                pFdp->offset, (uint32_t)count,
                &u32Actual)
            != 0)
            return -1;
        pFdp->offset += u32Actual;
        return (int)u32Actual;
    }

    mkrn_file_ent_t *pFile = pFdp->file;
    if (pFile->type != M4K_FILE_REGULAR)
        return -1;
    uint32_t u32NewSize =
        pFdp->offset + (uint32_t)count;
    if (u32NewSize > pFile->capacity) {
        uint32_t u32NewCap =
            pFile->capacity ? pFile->capacity * 2 : 4096;
        while (u32NewCap < u32NewSize)
            u32NewCap *= 2;
        uint8_t *pNewData =
            (uint8_t *)mkrn_alloc(u32NewCap);
        if (!pNewData)
            return -1;
        if (pFile->data) {
            mkrn_memcpy(pNewData, pFile->data, pFile->size);
            mkrn_free(pFile->data);
        }
        pFile->data = pNewData;
        pFile->capacity = u32NewCap;
        /* Zero the extension gap so a seek past EOF does not leak
         * stale kernel-heap bytes into the file on read back. */
        mkrn_memset(pFile->data + pFile->size, 0,
                    pFile->capacity - pFile->size);
    }
    mkrn_memcpy(pFile->data + pFdp->offset, pBuf, count);
    pFdp->offset += (uint32_t)count;
    if (pFdp->offset > pFile->size)
        pFile->size = pFdp->offset;
    return (int)count;
}

/**
 * @brief Close a file descriptor.
 * @param fd  file descriptor to close
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_close(int fd)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !bVfsInitialized)
        return -1;

    /* Route special filesystem closes (fds not in fd_table) */
    if (mkrn_procfs_is_procfs_fd(fd))
        return mkrn_procfs_close(fd);
    if (mkrn_sessions_is_sessions_fd(fd))
        return mkrn_sessions_close(fd);
    if (mkrn_device_tree_is_dt_fd(fd))
        return mkrn_device_tree_close(fd);
    if (mkrn_devfs_is_devfs_fd(fd))
        return mkrn_devfs_close(fd);

    if (!fd_table[fd].in_use)
        return -1;

    /* Pipe teardown: fd_table entries with file == &file_table[0] and
     * yafs_inode == 0 encode the pipe index in `offset` (see
     * mkrn_vfs_pipe).  Mark the closed end and release the slot once
     * BOTH ends are closed — previously nothing ever set these flags,
     * so every pipe leaked its slot forever (MAX_PIPES exhaustion). */
    if (fd_table[fd].file == &file_table[0]
        && fd_table[fd].yafs_inode == 0)
    {
        int pi = (int)fd_table[fd].offset;
        if (pi >= 0 && pi < MAX_PIPES
            && pipe_buffers[pi].in_use)
        {
            pipe_buffer_t *pPb = &pipe_buffers[pi];
            bool bIsWriteEnd =
                (fd_table[fd].flags & M4K_O_WRONLY) != 0;
            bool bIsReadEnd =
                (fd_table[fd].flags & M4K_O_RDONLY) != 0;
            if (!bIsWriteEnd && !bIsReadEnd)
                bIsReadEnd = true; /* RDWR/O_RDWR default: read end */
            if (bIsWriteEnd)
                pPb->write_closed = true;
            else
                pPb->read_closed = true;
            if (pPb->read_closed && pPb->write_closed) {
                mkrn_memset(pPb, 0, sizeof(pipe_buffer_t));
            }
        }
    }

    mkrn_memset(&fd_table[fd], 0, sizeof(mkrn_file_desc_t));
    return 0;
}

/**
 * @brief Reposition read/write offset.
 * @param fd      file descriptor
 * @param offset  offset value
 * @param whence  M4K_SEEK_SET, M4K_SEEK_CUR, M4K_SEEK_END
 * @return new offset, or -1 on error.
 */
static int read_inode_raw(uint64_t inode_nr, struct yafs_inode_value *iv_out);

int
mkrn_vfs_lseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !bVfsInitialized)
        return -1;

    /* Route DevFS seeks (fds not in fd_table) */
    if (mkrn_devfs_is_devfs_fd(fd))
        return mkrn_devfs_lseek(fd, offset, whence);

    mkrn_file_desc_t *pFdp = &fd_table[fd];
    if (!pFdp->in_use || !pFdp->file)
        return -1;

    /* Pipe fds encode the pipe index in `offset` — seeking would
     * corrupt that encoding and redirect later reads/writes to the
     * wrong pipe.  Reject (ESPIPE-style). */
    if (pFdp->file == &file_table[0] && pFdp->yafs_inode == 0)
        return -1;

    /* YAFS fds: file_table[0] is the ramfs root sentinel whose size is
     * 0, so the generic clamp below forced every seek back to 0.
     * Compute against the inode's real size instead. */
    if (pFdp->yafs_inode != 0 && root_yafs_tree != 0) {
        struct yafs_inode_value iv;
        int64_t base;
        switch (whence) {
        case M4K_SEEK_SET:
            base = 0;
            break;
        case M4K_SEEK_CUR:
            base = (int64_t)pFdp->offset;
            break;
        case M4K_SEEK_END:
            if (read_inode_raw(pFdp->yafs_inode, &iv) != 0)
                return -1;
            base = (int64_t)iv.size;
            break;
        default:
            return -1;
        }
        int64_t new_off = base + offset;
        if (new_off < 0)
            new_off = 0;
        pFdp->offset = (uint32_t)new_off;
        return (int)pFdp->offset;
    }

    int new_offset;
    switch (whence) {
    case M4K_SEEK_SET:
        new_offset = offset;
        break;
    case M4K_SEEK_CUR:
        new_offset = (int)pFdp->offset + offset;
        break;
    case M4K_SEEK_END:
        new_offset = (int)pFdp->file->size + offset;
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
 * @brief Mount a filesystem into the mount table.
 * @param pSource  source device
 * @param pTarget  mount point
 * @param pFstype  filesystem type
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_mount(const char *pSource, const char *pTarget,
               const char *pFstype)
{
    if (!pSource || !pTarget || !pFstype)
        return -1;
    for (int i = 0; i < M4K_VFS_MAX_MOUNTS; i++) {
        if (!mount_table[i].mounted) {
            mkrn_strncpy(mount_table[i].source, pSource,
                    M4K_VFS_MAX_FILENAME - 1);
            mount_table[i].source[M4K_VFS_MAX_FILENAME - 1] =
                '\0';
            mkrn_strncpy(mount_table[i].target, pTarget,
                    M4K_VFS_MAX_FILENAME - 1);
            mount_table[i].target[M4K_VFS_MAX_FILENAME - 1] =
                '\0';
            mkrn_strncpy(mount_table[i].fstype, pFstype, 31);
            mount_table[i].fstype[31] = '\0';
            mount_table[i].mounted = true;
            mount_count++;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Unmount a filesystem.
 * @param pTarget  mount point
 * @return 0 on success, -1 on error.
 */
int
mkrn_vfs_umount(const char *pTarget)
{
    if (!pTarget)
        return -1;
    for (int i = 0; i < M4K_VFS_MAX_MOUNTS; i++) {
        if (mount_table[i].mounted
            && mkrn_strcmp(mount_table[i].target, pTarget)
                   == 0)
        {
            mkrn_memset(&mount_table[i], 0,
                   sizeof(mkrn_mount_ent_t));
            mount_count--;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Get mount information.
 * @param pBuf  output buffer
 * @param max   max entries to write
 * @return number of entries written.
 */
int
mkrn_vfs_get_mount_info(mkrn_mount_ent_t *pBuf, int max)
{
    if (!pBuf || max <= 0)
        return 0;
    int written = 0;
    for (int i = 0; i < M4K_VFS_MAX_MOUNTS && written < max;
         i++)
    {
        if (mount_table[i].mounted) {
            pBuf[written] = mount_table[i];
            written++;
        }
    }
    return written;
}

/**
 * @brief Get directory entries (YAFS or ramfs fallback).
 * @param fd     directory FD
 * @param pBuf   output dirent buffer
 * @param count  max entries
 * @return number of entries, or -1 on error.
 */
int
mkrn_vfs_getdents(int fd, struct mkrn_vfs_dirent *pBuf,
                  uint32_t count)
{
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !bVfsInitialized)
        return -1;

    /* Route ProcFS getdents */
    if (mkrn_procfs_is_procfs_fd(fd))
        return mkrn_procfs_getdents(fd, pBuf, count);

    /* Route device tree getdents */
    if (mkrn_device_tree_is_dt_fd(fd))
        return mkrn_device_tree_getdents(fd, pBuf, count);

    /* Route DevFS getdents */
    if (mkrn_devfs_is_devfs_fd(fd))
        return mkrn_devfs_getdents(fd, pBuf, count);

    if (root_yafs_tree != 0) {
        /* A pipe fd has file == root-sentinel AND yafs_inode == 0 —
         * without this guard getdents on it would silently list the
         * filesystem root (offset happens to hold the pipe index,
         * not a directory context at all). */
        if (fd_table[fd].file == &file_table[0]
            && fd_table[fd].yafs_inode == 0)
            return -1;
        uint64_t u64DirInode = fd_table[fd].yafs_inode;
        if (u64DirInode == 0)
            u64DirInode = 1;
        return mkrn_yafs_readdir(
            root_yafs_tree, u64DirInode, pBuf, count);
    }

    return -1;
}

/* ── chmod/chown/access (§6.12.2) ── */

static int read_inode_raw(uint64_t inode_nr, struct yafs_inode_value *iv_out)
{
    uint64_t key = mkrn_yafs_make_key(YAFS_KS_INODE, inode_nr);
    yafs_entry_t lba_val;
    if (mkrn_yafs_btree_lookup(root_yafs_tree, key, &lba_val) != 0)
        return -1;
    /* Static instead of stack: a 4 KB frame on every inode access
     * puts needless depth on the (finite) kernel stack.  The YAFS
     * data path is non-reentrant (syscall context only). */
    static uint8_t buf[4096];
    if (mkrn_yafs_dev_read(lba_val, buf) != 0)
        return -1;
    mkrn_memcpy(iv_out, buf, sizeof(struct yafs_inode_value));
    return 0;
}

static int write_inode_raw(uint64_t inode_nr, const struct yafs_inode_value *iv)
{
    uint64_t key = mkrn_yafs_make_key(YAFS_KS_INODE, inode_nr);
    yafs_entry_t lba_val;
    if (mkrn_yafs_btree_lookup(root_yafs_tree, key, &lba_val) != 0)
        return -1;
    static uint8_t buf[4096];
    mkrn_memset(buf, 0, sizeof(buf));
    mkrn_memcpy(buf, iv, sizeof(struct yafs_inode_value));
    return mkrn_yafs_dev_write(lba_val, buf);
}

int
mkrn_vfs_chmod(const char *path, uint32_t mode)
{
    if (!path || root_yafs_tree == 0)
        return -1;
    uint64_t inode_nr = mkrn_yafs_lookup_path(root_yafs_tree, path);
    if (inode_nr == 0)
        return -1;
    struct yafs_inode_value iv;
    if (read_inode_raw(inode_nr, &iv) != 0)
        return -1;
    /* Permission check: must be owner or root */
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (pCur && pCur->euid != M4K_UID_ROOT && pCur->euid != iv.uid)
        return -1;
    iv.mode = (iv.mode & ~0xFFFF) | (mode & 0xFFFF);
    return write_inode_raw(inode_nr, &iv);
}

int
mkrn_vfs_chown(const char *path, uint32_t uid, uint32_t gid)
{
    if (!path || root_yafs_tree == 0)
        return -1;
    uint64_t inode_nr = mkrn_yafs_lookup_path(root_yafs_tree, path);
    if (inode_nr == 0)
        return -1;
    /* Only root can chown */
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (pCur && pCur->euid != M4K_UID_ROOT)
        return -1;
    struct yafs_inode_value iv;
    if (read_inode_raw(inode_nr, &iv) != 0)
        return -1;
    iv.uid = uid;
    iv.gid = gid;
    return write_inode_raw(inode_nr, &iv);
}

int
mkrn_vfs_access(const char *path, int mode)
{
    if (!path || root_yafs_tree == 0)
        return -1;
    uint64_t inode_nr = mkrn_yafs_lookup_path(root_yafs_tree, path);
    if (inode_nr == 0)
        return -1;
    struct yafs_inode_value iv;
    if (read_inode_raw(inode_nr, &iv) != 0)
        return -1;
    return mkrn_check_access(&iv, (uint32_t)mode);
}



