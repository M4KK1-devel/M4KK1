/*
 * M4KK1 4P1 - yafs_btree.h
 * Description: YAFS B+Tree — disk-resident, copy-on-write,
 *              logical-addressing B+Tree header.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================
 * Block size — currently fixed at 4 KB for the B+Tree
 * ============================================================ */
#define YAFS_BTREE_BLOCK_SIZE       4096
#define YAFS_BTREE_HEADER_SIZE      32
#define YAFS_BTREE_PAYLOAD_SIZE     4064
#define YAFS_DATA_BLOCK_SIZE        YAFS_BTREE_BLOCK_SIZE

#define YAFS_NODE_MAGIC             0x59414B55ULL

#define YAFS_NODE_LEAF              0x0001
#define YAFS_NODE_DELETED           0x0002

#define YAFS_BTREE_MAX_KEYS         248

/* ============================================================
 * Key space tags (upper 4 bits of the 64-bit key)
 * ============================================================ */
#define YAFS_KS_INODE       0ULL
#define YAFS_KS_DIR_ENTRY   1ULL
#define YAFS_KS_EXTENT      2ULL
#define YAFS_KS_SNAPSHOT    3ULL

#define YAFS_KS_TAG_SHIFT   60
#define YAFS_KS_TAG_MASK    0xF000000000000000ULL
#define YAFS_KS_VALUE_MASK  0x0FFFFFFFFFFFFFFFULL

typedef uint64_t yafs_entry_t;

#define YAFS_EXTENT_LBA_MASK    0x0000FFFFFFFFFFFFULL
#define YAFS_EXTENT_LEN_SHIFT   48

static inline yafs_entry_t
mkrn_yafs_extent_pack(uint64_t u64Lba, uint32_t u32Len)
{
    return (u64Lba & YAFS_EXTENT_LBA_MASK)
         | ((uint64_t)(u32Len & 0xFFFF)
            << YAFS_EXTENT_LEN_SHIFT);
}

static inline uint64_t
mkrn_yafs_extent_lba(yafs_entry_t e)
{
    return e & YAFS_EXTENT_LBA_MASK;
}

static inline uint32_t
mkrn_yafs_extent_length(yafs_entry_t e)
{
    return (uint32_t)((e >> YAFS_EXTENT_LEN_SHIFT)
                      & 0xFFFF);
}

static inline uint64_t
mkrn_yafs_key_tag(uint64_t u64Key)
{
    return u64Key >> YAFS_KS_TAG_SHIFT;
}

static inline uint64_t
mkrn_yafs_key_value(uint64_t u64Key)
{
    return u64Key & YAFS_KS_VALUE_MASK;
}

static inline uint64_t
mkrn_yafs_make_key(uint64_t u64Tag, uint64_t u64Val)
{
    return (u64Tag << YAFS_KS_TAG_SHIFT)
         | (u64Val & YAFS_KS_VALUE_MASK);
}

/* ============================================================
 * On-disk node structures
 * ============================================================ */

typedef struct {
    uint64_t magic;
    uint32_t checksum;
    uint16_t flags;
    uint16_t level;
    uint32_t entry_count;
    uint64_t parent_lba;
    uint8_t  _pad[4];
} __attribute__((packed, aligned(8)))
yafs_node_header_t;

_Static_assert(sizeof(yafs_node_header_t) == 32,
               "yafs_node_header_t must be 32 bytes");

#define YAFS_BTREE_INTERNAL_KEYS  \
    (YAFS_BTREE_PAYLOAD_SIZE / 16 - 1)
#define YAFS_BTREE_LEAF_KEYS  \
    ((YAFS_BTREE_PAYLOAD_SIZE - 16) / 16)

typedef struct {
    uint64_t keys[YAFS_BTREE_MAX_KEYS + 1];
    uint64_t children[YAFS_BTREE_MAX_KEYS + 2];
} __attribute__((aligned(8)))
yafs_internal_payload_t;

typedef struct {
    uint64_t    keys[YAFS_BTREE_MAX_KEYS + 1];
    yafs_entry_t values[YAFS_BTREE_MAX_KEYS + 1];
    uint64_t    next_leaf;
    uint64_t    prev_leaf;
} __attribute__((aligned(8)))
yafs_leaf_payload_t;

typedef struct {
    yafs_node_header_t header;
    union {
        yafs_internal_payload_t internal;
        yafs_leaf_payload_t     leaf;
    } payload;
    uint8_t _pad[YAFS_BTREE_BLOCK_SIZE
                 - sizeof(yafs_node_header_t)
                 - sizeof(yafs_leaf_payload_t)];
} __attribute__((aligned(8))) yafs_node_t;

_Static_assert(sizeof(yafs_node_t)
               == YAFS_BTREE_BLOCK_SIZE,
               "yafs_node_t must match block size");

/* ============================================================
 * Node-level API
 * ============================================================ */

int mkrn_yafs_node_read(uint64_t u64Lba,
                        yafs_node_t *pNode);
uint64_t mkrn_yafs_node_write(yafs_node_t *pNode);
void mkrn_yafs_node_clone(const yafs_node_t *pSrc,
                          yafs_node_t *pDst);
int mkrn_yafs_node_verify(const yafs_node_t *pNode);
void mkrn_yafs_node_checksum(yafs_node_t *pNode);

/* ============================================================
 * B+Tree API
 * ============================================================ */

int mkrn_yafs_btree_lookup(uint64_t u64RootLba,
                           uint64_t u64Key,
                           yafs_entry_t *pValueOut);
int mkrn_yafs_btree_insert(uint64_t *pRootLba,
                           uint64_t u64Key,
                           yafs_entry_t value,
                           bool *pInserted);
int mkrn_yafs_btree_delete(uint64_t *pRootLba,
                           uint64_t u64Key,
                           bool *pDeleted);
int mkrn_yafs_btree_walk(uint64_t u64RootLba,
                         uint64_t u64StartKey,
                         int (*cb)(uint64_t key,
                                   yafs_entry_t value,
                                   void *ctx),
                         void *pCtx);
int mkrn_yafs_btree_walk_range(uint64_t u64RootLba,
                               uint64_t u64Lo,
                               uint64_t u64Hi,
                               int (*cb)(uint64_t key,
                                         yafs_entry_t value,
                                         void *ctx),
                               void *pCtx);
uint64_t mkrn_yafs_btree_find_leaf(
    uint64_t u64RootLba, uint64_t u64Key);
