/*
 * M4KK1 4P1 - btree.c
 * Description: YAFS B+Tree — disk-resident, copy-on-write,
 *              logical-addressing B+Tree implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <yafs_btree.h>
#include <stdint.h>
#include <string.h>

extern int      mkrn_yafs_dev_read(uint64_t u64Lba,
                                   void *pBuf);
extern int      mkrn_yafs_dev_write(uint64_t u64Lba,
                                    const void *pBuf);
extern uint64_t mkrn_yafs_dev_alloc_block(void);
extern void     mkrn_yafs_dev_free_block(uint64_t u64Lba);

static int
key_search(const uint64_t *pKeys, uint32_t u32Count,
           uint64_t u64Key, bool *pExact)
{
    int lo = 0;
    int hi = (int)u32Count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (pKeys[mid] == u64Key) {
            *pExact = true;
            return mid;
        } else if (pKeys[mid] < u64Key) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    *pExact = false;
    return lo;
}

static void
node_init(yafs_node_t *pNode, uint16_t u16Flags,
          uint16_t u16Level)
{
    mkrn_memset(pNode, 0, sizeof(yafs_node_t));
    pNode->header.magic = YAFS_NODE_MAGIC;
    pNode->header.flags = u16Flags;
    pNode->header.level = u16Level;
    pNode->header.entry_count = 0;
    pNode->header.parent_lba = 0;
}

static void
leaf_insert_at(yafs_leaf_payload_t *pLeaf,
               uint32_t u32EntryCount,
               uint32_t u32Pos, uint64_t u64Key,
               yafs_entry_t value)
{
    for (uint32_t i = u32EntryCount; i > u32Pos; i--) {
        pLeaf->keys[i] = pLeaf->keys[i - 1];
        pLeaf->values[i] = pLeaf->values[i - 1];
    }
    pLeaf->keys[u32Pos] = u64Key;
    pLeaf->values[u32Pos] = value;
}

static void
internal_insert_at(yafs_internal_payload_t *pInternal,
                   uint32_t u32EntryCount,
                   uint32_t u32Pos, uint64_t u64Key,
                   uint64_t u64Child)
{
    for (uint32_t i = u32EntryCount; i > u32Pos; i--) {
        pInternal->keys[i] = pInternal->keys[i - 1];
        pInternal->children[i + 1] =
            pInternal->children[i];
    }
    pInternal->keys[u32Pos] = u64Key;
    pInternal->children[u32Pos + 1] = u64Child;
}

int
mkrn_yafs_node_read(uint64_t u64Lba,
                    yafs_node_t *pNode)
{
    return mkrn_yafs_dev_read(u64Lba, pNode);
}

uint64_t
mkrn_yafs_node_write(yafs_node_t *pNode)
{
    uint64_t u64Lba = mkrn_yafs_dev_alloc_block();
    if (u64Lba == 0)
        return 0;
    mkrn_yafs_node_checksum(pNode);
    if (mkrn_yafs_dev_write(u64Lba, pNode) != 0) {
        mkrn_yafs_dev_free_block(u64Lba);
        return 0;
    }
    return u64Lba;
}

void
mkrn_yafs_node_clone(const yafs_node_t *pSrc,
                     yafs_node_t *pDst)
{
    mkrn_memcpy(pDst, pSrc, sizeof(yafs_node_t));
}

int
mkrn_yafs_node_verify(const yafs_node_t *pNode)
{
    if (pNode->header.magic != YAFS_NODE_MAGIC)
        return -1;
    (void)pNode;
    return 0;
}

void
mkrn_yafs_node_checksum(yafs_node_t *pNode)
{
    pNode->header.checksum = 0;
}

static uint64_t
btree_insert_node(uint64_t u64NodeLba, uint64_t u64Key,
                  yafs_entry_t value,
                  uint64_t *pSplitKey,
                  uint64_t *pSplitLba, bool *pInserted)
{
    yafs_node_t node_buf;
    yafs_node_t *pNode = &node_buf;

    if (mkrn_yafs_node_read(u64NodeLba, pNode) != 0)
        return 0;

    yafs_node_t new_node;
    mkrn_yafs_node_clone(pNode, &new_node);

    if (new_node.header.flags & YAFS_NODE_LEAF) {
        yafs_leaf_payload_t *pLeaf =
            &new_node.payload.leaf;
        bool bExact = false;
        int pos = key_search(
            pLeaf->keys, new_node.header.entry_count,
            u64Key, &bExact);

        if (bExact) {
            pLeaf->values[pos] = value;
            *pInserted = false;
        } else {
            leaf_insert_at(
                pLeaf, new_node.header.entry_count,
                (uint32_t)pos, u64Key, value);
            new_node.header.entry_count++;
            *pInserted = true;
        }

        uint64_t u64NewLba =
            mkrn_yafs_node_write(&new_node);
        if (u64NewLba == 0)
            return 0;

        if (new_node.header.entry_count
            > YAFS_BTREE_MAX_KEYS)
        {
            uint32_t u32Mid = YAFS_BTREE_MAX_KEYS / 2;

            yafs_node_t right_node;
            node_init(&right_node, YAFS_NODE_LEAF, 0);
            yafs_leaf_payload_t *pRight =
                &right_node.payload.leaf;

            uint32_t u32Moved =
                new_node.header.entry_count - u32Mid;
            for (uint32_t i = 0; i < u32Moved; i++) {
                pRight->keys[i] =
                    pLeaf->keys[u32Mid + i];
                pRight->values[i] =
                    pLeaf->values[u32Mid + i];
            }
            right_node.header.entry_count = u32Moved;
            new_node.header.entry_count = u32Mid;

            uint64_t u64RightLba =
                mkrn_yafs_node_write(&right_node);
            if (u64RightLba == 0)
                return 0;

            uint64_t u64NewNewLba =
                mkrn_yafs_node_write(&new_node);
            if (u64NewNewLba == 0) {
                mkrn_yafs_dev_free_block(u64RightLba);
                return 0;
            }
            u64NewLba = u64NewNewLba;

            *pSplitKey = pRight->keys[0];
            *pSplitLba = u64RightLba;
        } else {
            *pSplitKey = 0;
            *pSplitLba = 0;
        }

        return u64NewLba;
    } else {
        yafs_internal_payload_t *pInternal =
            &new_node.payload.internal;
        bool bExact = false;
        int pos = key_search(
            pInternal->keys,
            new_node.header.entry_count, u64Key,
            &bExact);

        uint64_t u64ChildLba =
            pInternal->children[pos];
        uint64_t u64ChildSplitKey = 0;
        uint64_t u64ChildSplitLba = 0;
        bool bChildInserted = false;

        uint64_t u64NewChildLba = btree_insert_node(
            u64ChildLba, u64Key, value,
            &u64ChildSplitKey, &u64ChildSplitLba,
            &bChildInserted);
        if (u64NewChildLba == 0)
            return 0;

        pInternal->children[pos] = u64NewChildLba;

        if (u64ChildSplitLba != 0) {
            internal_insert_at(
                pInternal,
                new_node.header.entry_count,
                (uint32_t)pos + 1,
                u64ChildSplitKey, u64ChildSplitLba);
            new_node.header.entry_count++;
            if (pInserted && bChildInserted)
                *pInserted = true;
        }

        uint64_t u64NewLba =
            mkrn_yafs_node_write(&new_node);
        if (u64NewLba == 0)
            return 0;

        if (new_node.header.entry_count
            > YAFS_BTREE_MAX_KEYS)
        {
            uint32_t u32Mid =
                YAFS_BTREE_MAX_KEYS / 2;

            yafs_node_t right_node;
            node_init(&right_node, 0,
                      new_node.header.level);
            yafs_internal_payload_t *pRight =
                &right_node.payload.internal;

            uint64_t u64PromoteKey =
                pInternal->keys[u32Mid];
            uint32_t u32Moved =
                new_node.header.entry_count
                - u32Mid - 1;

            for (uint32_t i = 0; i < u32Moved; i++) {
                pRight->keys[i] =
                    pInternal->keys[u32Mid + 1 + i];
            }
            for (uint32_t i = 0; i <= u32Moved; i++) {
                pRight->children[i] =
                    pInternal->children[u32Mid + 1 + i];
            }
            right_node.header.entry_count = u32Moved;
            new_node.header.entry_count = u32Mid;

            uint64_t u64RightLba =
                mkrn_yafs_node_write(&right_node);
            if (u64RightLba == 0)
                return 0;

            uint64_t u64NewNewLba =
                mkrn_yafs_node_write(&new_node);
            if (u64NewNewLba == 0) {
                mkrn_yafs_dev_free_block(u64RightLba);
                return 0;
            }
            u64NewLba = u64NewNewLba;

            *pSplitKey = u64PromoteKey;
            *pSplitLba = u64RightLba;
        } else {
            *pSplitKey = 0;
            *pSplitLba = 0;
        }

        return u64NewLba;
    }
}

int
mkrn_yafs_btree_lookup(uint64_t u64RootLba,
                       uint64_t u64Key,
                       yafs_entry_t *pValueOut)
{
    yafs_node_t node;
    uint64_t u64Current = u64RootLba;

    if (u64RootLba == 0)
        return -1;

    while (1) {
        if (mkrn_yafs_node_read(u64Current, &node) != 0)
            return -1;
        if (node.header.flags & YAFS_NODE_LEAF)
            break;

        bool bExact = false;
        int pos = key_search(
            node.payload.internal.keys,
            node.header.entry_count, u64Key, &bExact);
        u64Current =
            node.payload.internal.children[pos];
    }

    bool bExact = false;
    int pos = key_search(
        node.payload.leaf.keys,
        node.header.entry_count, u64Key, &bExact);
    if (!bExact)
        return -1;

    *pValueOut = node.payload.leaf.values[pos];
    return 0;
}

int
mkrn_yafs_btree_insert(uint64_t *pRootLba,
                       uint64_t u64Key,
                       yafs_entry_t value,
                       bool *pInserted)
{
    if (pRootLba == NULL)
        return -1;

    uint64_t u64SplitKey = 0;
    uint64_t u64SplitLba = 0;
    bool bLocalInserted = false;

    if (*pRootLba == 0) {
        yafs_node_t root;
        node_init(&root, YAFS_NODE_LEAF, 0);
        root.payload.leaf.keys[0] = u64Key;
        root.payload.leaf.values[0] = value;
        root.header.entry_count = 1;

        *pRootLba = mkrn_yafs_node_write(&root);
        if (*pRootLba == 0)
            return -1;
        if (pInserted)
            *pInserted = true;
        return 0;
    }

    uint64_t u64NewRoot = btree_insert_node(
        *pRootLba, u64Key, value, &u64SplitKey,
        &u64SplitLba, &bLocalInserted);
    if (u64NewRoot == 0)
        return -1;

    if (u64SplitLba != 0) {
        yafs_node_t new_root_node;
        node_init(&new_root_node, 0, 1);

        new_root_node.payload.internal.children[0] =
            u64NewRoot;
        new_root_node.payload.internal.keys[0] =
            u64SplitKey;
        new_root_node.payload.internal.children[1] =
            u64SplitLba;
        new_root_node.header.entry_count = 1;

        uint64_t u64SuperRoot =
            mkrn_yafs_node_write(&new_root_node);
        if (u64SuperRoot == 0) {
            mkrn_yafs_dev_free_block(u64NewRoot);
            mkrn_yafs_dev_free_block(u64SplitLba);
            return -1;
        }
        *pRootLba = u64SuperRoot;
    } else {
        *pRootLba = u64NewRoot;
    }

    if (pInserted)
        *pInserted = bLocalInserted;
    return 0;
}

int
mkrn_yafs_btree_delete(uint64_t *pRootLba,
                       uint64_t u64Key, bool *pDeleted)
{
    if (pRootLba == NULL || *pRootLba == 0) {
        if (pDeleted)
            *pDeleted = false;
        return -1;
    }

    yafs_node_t root_node;
    if (mkrn_yafs_node_read(*pRootLba, &root_node)
        != 0)
    {
        if (pDeleted)
            *pDeleted = false;
        return -1;
    }

    yafs_node_t new_root;
    mkrn_yafs_node_clone(&root_node, &new_root);

#define YAFS_BTREE_MAX_DEPTH 32

    struct path_entry {
        uint64_t lba;
        int      child_idx;
    };

    struct path_entry path[YAFS_BTREE_MAX_DEPTH];
    int depth = 0;
    uint64_t u64Current = *pRootLba;

    while (1) {
        yafs_node_t n;
        if (mkrn_yafs_node_read(u64Current, &n) != 0) {
            if (pDeleted)
                *pDeleted = false;
            return -1;
        }
        path[depth].lba = u64Current;

        if (n.header.flags & YAFS_NODE_LEAF)
            break;

        bool bExact = false;
        int pos = key_search(
            n.payload.internal.keys,
            n.header.entry_count, u64Key, &bExact);
        path[depth].child_idx = pos;
        u64Current =
            n.payload.internal.children[pos];
        depth++;

        if (depth >= YAFS_BTREE_MAX_DEPTH) {
            if (pDeleted)
                *pDeleted = false;
            return -1;
        }
    }

    yafs_node_t leaf_node;
    if (mkrn_yafs_node_read(path[depth].lba,
                            &leaf_node)
        != 0)
    {
        if (pDeleted)
            *pDeleted = false;
        return -1;
    }

    bool bExact = false;
    int pos = key_search(
        leaf_node.payload.leaf.keys,
        leaf_node.header.entry_count, u64Key,
        &bExact);
    if (!bExact) {
        if (pDeleted)
            *pDeleted = false;
        return 0;
    }

    yafs_node_t new_leaf;
    mkrn_yafs_node_clone(&leaf_node, &new_leaf);

    uint32_t u32Count = new_leaf.header.entry_count;
    for (uint32_t i = (uint32_t)pos; i < u32Count - 1;
         i++)
    {
        new_leaf.payload.leaf.keys[i] =
            new_leaf.payload.leaf.keys[i + 1];
        new_leaf.payload.leaf.values[i] =
            new_leaf.payload.leaf.values[i + 1];
    }
    new_leaf.header.entry_count--;

    uint64_t u64NewChildLba =
        mkrn_yafs_node_write(&new_leaf);
    if (u64NewChildLba == 0) {
        if (pDeleted)
            *pDeleted = false;
        return -1;
    }

    for (int d = depth - 1; d >= 0; d--) {
        yafs_node_t parent;
        if (mkrn_yafs_node_read(path[d].lba, &parent)
            != 0)
        {
            mkrn_yafs_dev_free_block(u64NewChildLba);
            if (pDeleted)
                *pDeleted = false;
            return -1;
        }
        yafs_node_t new_parent;
        mkrn_yafs_node_clone(&parent, &new_parent);

        int ci = path[d].child_idx;
        new_parent.payload.internal.children[ci] =
            u64NewChildLba;

        uint64_t u64NewParentLba =
            mkrn_yafs_node_write(&new_parent);
        if (u64NewParentLba == 0) {
            mkrn_yafs_dev_free_block(u64NewChildLba);
            if (pDeleted)
                *pDeleted = false;
            return -1;
        }
        u64NewChildLba = u64NewParentLba;
    }

    *pRootLba = u64NewChildLba;
    if (pDeleted)
        *pDeleted = true;
    return 0;

#undef YAFS_BTREE_MAX_DEPTH
}

static int
btree_walk_recursive(
    uint64_t u64NodeLba,
    int (*cb)(uint64_t key, yafs_entry_t value,
              void *ctx),
    void *pCtx, bool *pDone)
{
    if (u64NodeLba == 0 || *pDone)
        return 0;

    yafs_node_t node;
    if (mkrn_yafs_node_read(u64NodeLba, &node) != 0)
        return -1;

    if (node.header.flags & YAFS_NODE_LEAF) {
        for (uint32_t i = 0; i < node.header.entry_count;
             i++)
        {
            int ret = cb(node.payload.leaf.keys[i],
                         node.payload.leaf.values[i],
                         pCtx);
            if (ret != 0) {
                *pDone = true;
                return 0;
            }
        }
        return 0;
    }

    for (uint32_t i = 0;
         i <= node.header.entry_count; i++)
    {
        int ret = btree_walk_recursive(
            node.payload.internal.children[i], cb,
            pCtx, pDone);
        if (ret != 0 || *pDone)
            return ret;
    }
    return 0;
}

int
mkrn_yafs_btree_walk(uint64_t u64RootLba,
                     uint64_t u64StartKey,
                     int (*cb)(uint64_t key,
                               yafs_entry_t value,
                               void *ctx),
                     void *pCtx)
{
    if (u64RootLba == 0)
        return 0;
    (void)u64StartKey;

    bool bDone = false;
    return btree_walk_recursive(u64RootLba, cb, pCtx,
                                &bDone);
}

uint64_t
mkrn_yafs_btree_find_leaf(uint64_t u64RootLba,
                          uint64_t u64Key)
{
    if (u64RootLba == 0)
        return 0;

    yafs_node_t node;
    uint64_t u64Current = u64RootLba;

    while (1) {
        if (mkrn_yafs_node_read(u64Current, &node) != 0)
            return 0;
        if (node.header.flags & YAFS_NODE_LEAF)
            return u64Current;

        bool bExact = false;
        int pos = key_search(
            node.payload.internal.keys,
            node.header.entry_count, u64Key, &bExact);
        u64Current =
            node.payload.internal.children[pos];
    }
}
