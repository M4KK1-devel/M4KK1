/*
 * M4KK1 4P1 - vfs.h
 * Description: Virtual File System interface declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

struct yafs_inode_value;
#include <stddef.h>
#include <stdbool.h>

#define M4K_VFS_MAX_FILES   128
#define M4K_VFS_MAX_FILENAME 128
#define M4K_VFS_MAX_FDS     1024
#define M4K_VFS_MAX_MOUNTS  16

#define M4K_O_RDONLY        0x00000001
#define M4K_O_WRONLY        0x00000002
#define M4K_O_RDWR          0x00000004
#define M4K_O_CREAT         0x00000100
#define M4K_O_EXCL          0x00000200
#define M4K_O_TRUNC         0x00001000
#define M4K_O_APPEND        0x00002000
#define M4K_O_DIRECTORY     0x00004000

#define M4K_SEEK_SET        0
#define M4K_SEEK_CUR        1
#define M4K_SEEK_END        2

typedef enum {
    M4K_FILE_NONE,
    M4K_FILE_REGULAR,
    M4K_FILE_DIR
} mkrn_file_type_t;

typedef struct {
    char name[M4K_VFS_MAX_FILENAME];
    mkrn_file_type_t type;
    u8 *data;
    u32 size;
    u32 capacity;
    b in_use;
} mkrn_file_ent_t;

typedef struct {
    int fd;
    mkrn_file_ent_t *file;
    u32 offset;
    int flags;
    b in_use;
    u64 yafs_inode;
} mkrn_file_desc_t;

typedef struct {
    char source[M4K_VFS_MAX_FILENAME];
    char target[M4K_VFS_MAX_FILENAME];
    char fstype[32];
    b mounted;
} mkrn_mount_ent_t;

struct mkrn_vfs_dirent {
    u64 inode;
    u32 type;
    char name[256];
    u64 size;
};

struct yafs_mount;

/**
 * mkrn_vfs_init - Initialize the VFS subsystem
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_init(void);

/**
 * mkrn_vfs_find_file - Find a file by name
 * @name: File name
 *
 * Return: Pointer to file entry, NULL if not found
 */
mkrn_file_ent_t *mkrn_vfs_find_file(const char *name);

/**
 * mkrn_vfs_create_file - Create a new file
 * @name: File name
 * @type: File type
 *
 * Return: Pointer to new file entry, NULL on failure
 */
mkrn_file_ent_t *mkrn_vfs_create_file(const char *name, mkrn_file_type_t type);

/**
 * mkrn_vfs_delete_file - Delete a file by name
 * @name: File name
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_delete_file(const char *name);

/**
 * mkrn_vfs_open - Open a file
 * @pathname: Path to file
 * @flags: Open flags
 *
 * Return: File descriptor, -1 on error
 */
int mkrn_vfs_open(const char *pathname, int flags);

/**
 * mkrn_vfs_create_file_yafs - Create file in YAFS
 * @pathname: Path to file
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_create_file_yafs(const char *pathname);

/**
 * mkrn_vfs_read - Read from file
 * @fd: File descriptor
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 *
 * Return: Number of bytes read, -1 on error
 */
int mkrn_vfs_read(int fd, void *buf, size_t count);

/**
 * mkrn_vfs_write - Write to file
 * @fd: File descriptor
 * @buf: Buffer to write from
 * @count: Number of bytes to write
 *
 * Return: Number of bytes written, -1 on error
 */
int mkrn_vfs_write(int fd, const void *buf, size_t count);

/**
 * mkrn_vfs_close - Close a file
 * @fd: File descriptor
 *
 * Return: 0 on success, -1 on error
 */
int mkrn_vfs_close(int fd);

/**
 * mkrn_vfs_lseek - Reposition file offset
 * @fd: File descriptor
 * @offset: Offset value
 * @whence: Seek origin
 *
 * Return: New file offset, -1 on error
 */
int mkrn_vfs_lseek(int fd, int offset, int whence);

/**
 * mkrn_vfs_mount - Mount a filesystem
 * @source: Source device or file
 * @target: Mount point
 * @fstype: Filesystem type
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_mount(const char *source, const char *target, const char *fstype);

/**
 * mkrn_vfs_umount - Unmount a filesystem
 * @target: Mount point
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_umount(const char *target);

/**
 * mkrn_vfs_getdents - Get directory entries
 * @fd: File descriptor
 * @buf: Buffer for entries
 * @count: Maximum number of entries
 *
 * Return: Number of entries read, -1 on error
 */
int mkrn_vfs_getdents(int fd, struct mkrn_vfs_dirent *buf, u32 count);

/**
 * mkrn_vfs_pipe - Create a pipe
 * @fd: Array to store file descriptors
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_pipe(int fd[2]);

/**
 * mkrn_vfs_dup2 - Duplicate file descriptor
 * @oldfd: Old file descriptor
 * @newfd: New file descriptor
 *
 * Return: New file descriptor, -1 on error
 */
int mkrn_vfs_dup2(int oldfd, int newfd);

/**
 * mkrn_vfs_mkdir - Create a directory
 * @pathname: Path to directory
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_mkdir(const char *pathname);

/**
 * mkrn_vfs_unlink - Remove a file
 * @pathname: Path to file
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_unlink(const char *pathname);

/**
 * mkrn_vfs_rmdir - Remove a directory
 * @pathname: Path to directory
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_rmdir(const char *pathname);

/**
 * mkrn_vfs_rename - Rename a file or directory
 * @oldpath: Old path
 * @newpath: New path
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_vfs_rename(const char *oldpath, const char *newpath);

/**
 * mkrn_vfs_fd_in_use - Check if file descriptor is in use
 * @fd: File descriptor
 *
 * Return: 1 if in use, 0 otherwise
 */
int mkrn_vfs_fd_in_use(int fd);

/**
 * mkrn_vfs_set_root_yafs - Set YAFS as root filesystem
 * @m: YAFS mount structure
 * @root_tree_addr: Root tree address
 *
 * Return: void
 */
void mkrn_vfs_set_root_yafs(struct yafs_mount *m, u64 root_tree_addr);

/**
 * mkrn_vfs_yafs_getdents - Get directory entries from YAFS
 * @dir_inode: Directory inode
 * @buf: Buffer for entries
 * @max_entries: Maximum number of entries
 *
 * Return: Number of entries read, -1 on error
 */
int mkrn_vfs_yafs_getdents(u64 dir_inode, struct mkrn_vfs_dirent *buf, u32 max_entries);

/**
 * mkrn_vfs_get_mount_info - Get mount information
 * @buf: Buffer for mount entries
 * @max: Maximum number of entries
 *
 * Return: Number of entries filled
 */
int mkrn_vfs_get_mount_info(mkrn_mount_ent_t *buf, int max);

/**
 * mkrn_yafs_lookup_path - Look up a path in YAFS
 * @root_tree: Root tree identifier
 * @path: Path to look up
 *
 * Return: Inode number, 0 if not found
 */
u64 mkrn_yafs_lookup_path(u64 root_tree, const char *path);

/* ── Permission bits ── */

#define M4K_IRUSR   0x0100
#define M4K_IWUSR   0x0080
#define M4K_IXUSR   0x0040
#define M4K_IRGRP   0x0020
#define M4K_IWGRP   0x0010
#define M4K_IXGRP   0x0008
#define M4K_IROTH   0x0004
#define M4K_IWOTH   0x0002
#define M4K_IXOTH   0x0001
#define M4K_ISVTX   0x1000

#define M4K_ACCESS_READ   1
#define M4K_ACCESS_WRITE  2
#define M4K_ACCESS_EXEC   4

int mkrn_check_access(struct yafs_inode_value *inode, uint32_t mode);
int mkrn_vfs_chmod(const char *path, uint32_t mode);
int mkrn_vfs_chown(const char *path, uint32_t uid, uint32_t gid);
int mkrn_vfs_access(const char *path, int mode);

/* ── ProcFS hooks ── */

int mkrn_procfs_open(const char *path, int flags, int *out_fd);
int mkrn_procfs_close(int fd);
int mkrn_procfs_read(int fd, void *buf, uint32_t count);
int mkrn_procfs_write(int fd, const void *buf, uint32_t count);
int mkrn_procfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max);
int mkrn_procfs_is_procfs_fd(int fd);
void mkrn_procfs_init(void);

/* ── DevFS hooks ── */

int mkrn_devfs_open(const char *path, int flags, int *out_fd);
int mkrn_devfs_close(int fd);
int mkrn_devfs_read(int fd, void *buf, uint32_t count);
int mkrn_devfs_write(int fd, const void *buf, uint32_t count);
int mkrn_devfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max);
int mkrn_devfs_is_devfs_fd(int fd);
int mkrn_devfs_lseek(int fd, int offset, int whence);
void mkrn_devfs_init(void);
