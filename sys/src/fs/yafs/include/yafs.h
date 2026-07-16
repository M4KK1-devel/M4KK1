/*
 * M4KK1 4P1 - yafs.h
 * Description: YAFS — Yet Another File System header.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <yafs_btree.h>

#define YAFS_MAGIC              0x59414653
#define YAFS_SUPERBLOCK_VERSION  1

#define YAFS_BLOCK_SIZE_MIN      512
#define YAFS_BLOCK_SIZE_4K       4096
#define YAFS_BLOCK_SIZE_MAX      65536
#define YAFS_DEFAULT_BLOCK_SIZE  YAFS_BLOCK_SIZE_4K

#define YAFS_NR_ROOTS            8

#define YAFS_BLOCK_FREE          0
#define YAFS_BTREE_NODE          1
#define YAFS_DATA_BLOCK          2

#define YAFS_MAX_NAME_LEN        255

#define YAFS_FT_UNKNOWN          0
#define YAFS_FT_REG_FILE         1
#define YAFS_FT_DIR              2
#define YAFS_FT_SYMLINK          3

#define YAFS_ENC_NONE            0
#define YAFS_ENC_AES128_XTS      1

#define YAFS_SNAP_READONLY       0x0001
#define YAFS_SNAP_ENCRYPTED      0x0002

#define YAFS_STATE_CLEAN         0x0001
#define YAFS_STATE_DIRTY         0x0002

#define YAFS_IOCTL_CREATE_SNAP   0xAF01
#define YAFS_IOCTL_ROLLBACK      0xAF02
#define YAFS_IOCTL_SET_PARALLEL  0xAF03

#define YAFS_ROLLBACK_FULL       0
#define YAFS_ROLLBACK_FAST       1

#define YAFS_DEFAULT_CHUNK_SIZE  (1024 * 1024)
#define YAFS_MAX_PARALLEL_WORKERS  16

#define YAFS_PBKDF2_ITERATIONS   100000
#define YAFS_KMK_SALT_SIZE       16
#define YAFS_KMK_HASH_SIZE       32
#define YAFS_KEY_SIZE_128        16
#define YAFS_FEK_SIZE            32

/* ============================================================
 * On-disk Structures
 * ============================================================ */

struct yafs_superblock {
    uint64_t magic;
    uint64_t version;
    uint64_t block_size;
    uint64_t total_blocks;
    uint64_t blocks_used;
    uint64_t root_tree_addr;
    uint64_t snapshot_tree_addr;
    uint64_t free_tree_addr;
    uint64_t reserved_roots[YAFS_NR_ROOTS - 3];
    uint64_t encryption_flags;
    uint8_t  kmk_salt[YAFS_KMK_SALT_SIZE];
    uint8_t  kmk_hash[YAFS_KMK_HASH_SIZE];
    uint64_t enc_fek_blk;
    uint64_t sb_backup_addr;
    uint64_t creation_time;
    uint64_t mount_time;
    uint64_t mount_count;
    uint64_t state_flags;
    uint32_t checksum;
    uint8_t  padding[4];
} __attribute__((aligned(8)));

_Static_assert(sizeof(struct yafs_superblock) <= 4096,
               "yafs_superblock too large");

struct yafs_block_header {
    uint64_t checksum;
    uint64_t refcount;
    uint32_t block_type;
    uint32_t logical_lba;
    uint64_t sequence;
} __attribute__((aligned(8)));

_Static_assert(sizeof(struct yafs_block_header) <= 4096,
               "yafs_block_header too large");

struct yafs_inode_value {
    uint64_t size;
    uint64_t blocks;
    uint64_t ctime;
    uint64_t mtime;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint32_t file_type;
    uint32_t link_count;
    uint32_t encryption_flags;
    char     name[128];
    uint8_t  pad[8];
} __attribute__((aligned(8)));

_Static_assert(sizeof(struct yafs_inode_value)
               == 64 + 128,
               "yafs_inode_value must be 192 bytes");

struct yafs_snapshot_value {
    uint64_t root_addr;
    uint64_t timestamp;
    uint64_t parent_snap_addr;
    uint64_t flags;
    uint8_t  fek[YAFS_FEK_SIZE];
    char     name[56];
    uint32_t checksum;
} __attribute__((aligned(8)));

_Static_assert(sizeof(struct yafs_snapshot_value) == 128,
               "yafs_snapshot_value must be 128 bytes");

struct yafs_free_space_value {
    uint64_t length;
} __attribute__((aligned(8)));

/* ============================================================
 * In-memory structures
 * ============================================================ */

struct yafs_mount {
    struct yafs_superblock *sb;
    uint32_t block_size;
    uint64_t num_blocks;
    uint64_t root_tree_addr;
    uint64_t snapshot_tree_addr;
    uint64_t free_tree_addr;
    void     *block_cache;
    uint32_t cache_size;
    bool     encrypted;
    uint8_t  fek[YAFS_FEK_SIZE];
    bool     fek_loaded;
    uint32_t chunk_size;
    uint32_t parallel_workers;
    void     *device;
    void     (*dev_read)(void *dev, uint64_t block,
                         void *buf, uint32_t count);
    void     (*dev_write)(void *dev, uint64_t block,
                          const void *buf,
                          uint32_t count);
    uint64_t next_sequence;
    bool     read_only;
    bool     mounted;
};

struct yafs_file {
    struct yafs_mount *mount;
    uint64_t inode;
    uint64_t offset;
    uint64_t flags;
};

/* ============================================================
 * Helper inline functions
 * ============================================================ */

static inline uint64_t
mkrn_yafs_name_hash(const char *pName)
{
    uint64_t u64H = 5381;
    while (*pName)
        u64H = ((u64H << 5) + u64H)
               + (unsigned char)*pName++;
    return u64H;
}

/* ============================================================
 * Function declarations
 * ============================================================ */

int mkrn_yafs_read_superblock(struct yafs_mount *pM);
int mkrn_yafs_write_superblock(struct yafs_mount *pM);
int mkrn_yafs_init_superblock(
    struct yafs_mount *pM, uint64_t u64TotalBlocks,
    bool bEncrypt);

int mkrn_yafs_read_block(struct yafs_mount *pM,
                         uint64_t u64Addr, void *pBuf);
int mkrn_yafs_write_block(struct yafs_mount *pM,
                          uint64_t u64Addr,
                          const void *pBuf);
uint64_t mkrn_yafs_alloc_block(struct yafs_mount *pM);
void mkrn_yafs_free_block(struct yafs_mount *pM,
                          uint64_t u64Addr);

struct mkrn_vfs_dirent;

int mkrn_yafs_readdir(uint64_t u64RootTree,
                      uint64_t u64DirInode,
                      struct mkrn_vfs_dirent *pBuf,
                      uint32_t u32MaxEntries);
int mkrn_yafs_create_fhs(uint64_t *pRootLba);
int mkrn_yafs_lookup_name(uint64_t u64RootTree,
                          uint64_t u64DirInode,
                          const char *pName,
                          uint64_t *pChildInode);
uint64_t mkrn_yafs_lookup_path(
    uint64_t u64RootTree, const char *pPath);

int mkrn_yafs_mkdir(uint64_t *pRoot,
                    uint64_t u64ParentInode,
                    const char *pName,
                    uint64_t u64InodeNr,
                    uint64_t *pNewInodeOut);
int mkrn_yafs_unlink(uint64_t *pRoot,
                     uint64_t u64ParentInode,
                     const char *pName);
int mkrn_yafs_rmdir(uint64_t *pRoot,
                    uint64_t u64ParentInode,
                    const char *pName);
int mkrn_yafs_rename(uint64_t *pRoot,
                     uint64_t u64OldParent,
                     const char *pOldName,
                     uint64_t u64NewParent,
                     const char *pNewName);

int mkrn_yafs_lookup(struct yafs_mount *pM,
                     uint64_t u64DirInode,
                     const char *pName,
                     uint64_t *pChildInode,
                     uint32_t *pFileType);
int mkrn_yafs_create_dirent(struct yafs_mount *pM,
                            uint64_t u64DirInode,
                            const char *pName,
                            uint64_t u64ChildInode,
                            uint32_t u32FileType);
int mkrn_yafs_remove_dirent(struct yafs_mount *pM,
                            uint64_t u64DirInode,
                            const char *pName);

int mkrn_yafs_create_inode(struct yafs_mount *pM,
                           uint64_t *pInodeOut,
                           uint32_t u32FileType,
                           uint32_t u32Mode);
int mkrn_yafs_read_inode(struct yafs_mount *pM,
                         uint64_t u64Inode,
                         struct yafs_inode_value *pVal);
int mkrn_yafs_write_inode(struct yafs_mount *pM,
                          uint64_t u64Inode,
                          struct yafs_inode_value *pVal);
int mkrn_yafs_unlink_inode(struct yafs_mount *pM,
                           uint64_t u64Inode);

int mkrn_yafs_read_file_data(struct yafs_mount *pM,
                             uint64_t u64Inode,
                             void *pBuf,
                             uint64_t u64Offset,
                             uint32_t u32Size,
                             uint32_t *pRead);
int mkrn_yafs_write_file_data(struct yafs_mount *pM,
                              uint64_t u64Inode,
                              const void *pBuf,
                              uint64_t u64Offset,
                              uint32_t u32Size,
                              uint32_t *pWritten);

int mkrn_yafs_read_file_parallel(
    struct yafs_mount *pM, uint64_t u64Inode,
    void *pBuf, uint64_t u64Offset, uint32_t u32Size,
    uint32_t *pRead);

int mkrn_yafs_create_snapshot(struct yafs_mount *pM,
                              const char *pName);
int mkrn_yafs_delete_snapshot(struct yafs_mount *pM,
                              const char *pName);
int mkrn_yafs_find_snapshot(
    struct yafs_mount *pM, const char *pName,
    struct yafs_snapshot_value *pSnap);

int mkrn_yafs_full_rollback(struct yafs_mount *pM,
                            const char *pSnapName);
int mkrn_yafs_fast_rollback(struct yafs_mount *pM,
                            const char *pSnapName);

int mkrn_yafs_mark_blocks_free(struct yafs_mount *pM,
                               uint64_t u64Start,
                               uint64_t u64Len);
uint64_t mkrn_yafs_find_free_blocks(
    struct yafs_mount *pM, uint64_t u64Count);

int mkrn_yafs_encrypt_block(
    const uint8_t pFek[YAFS_FEK_SIZE],
    uint64_t u64Lba, const void *pPlaintext,
    void *pCiphertext, uint32_t u32BlockSize);
int mkrn_yafs_decrypt_block(
    const uint8_t pFek[YAFS_FEK_SIZE],
    uint64_t u64Lba, const void *pCiphertext,
    void *pPlaintext, uint32_t u32BlockSize);
int mkrn_yafs_setup_encryption(struct yafs_mount *pM,
                               const char *pPassword);
int mkrn_yafs_verify_password(struct yafs_mount *pM,
                              const char *pPassword);

int mkrn_yafs_format(struct yafs_mount *pM,
                     uint64_t u64TotalBlocks,
                     bool bEncrypt);
int mkrn_yafs_mount_fs(struct yafs_mount *pM,
                       const char *pPassword);
int mkrn_yafs_umount(struct yafs_mount *pM);

int mkrn_yafs_fs_stats(uint32_t *pBlockSize,
                       uint32_t *pTotalBlocks,
                       uint32_t *pFreeBlocks,
                       uint32_t *pUsedBlocks);

uint32_t mkrn_yafs_crc32c(const void *pData,
                          uint32_t u32Len);
uint64_t mkrn_yafs_block_checksum(
    const void *pBlock, uint32_t u32BlockSize);
