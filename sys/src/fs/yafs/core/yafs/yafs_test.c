/*
 * M4KK1 4P1 - yafs_test.c
 * Description: YAFS test harness — ramdisk-backed
 *              B+Tree test for QEMU.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <yafs.h>
#include <yafs_btree.h>

/* 4096 blocks: this ramdisk IS the production root filesystem
 * backing store (FHS tree + /bin ELFs live here).  Do not shrink
 * for the boot-time self-test — it shares the device. */
#define RAMDISK_BLOCKS  4096
#define RAMDISK_SIZE    (RAMDISK_BLOCKS * YAFS_BTREE_BLOCK_SIZE)

/* The ramdisk lives at a FIXED address (0x2000000, 32MB), outside the
 * kernel image/BSS/heap.  A 16MB array inside .bss used to push the
 * kernel top to 0x127F79C — straight into the user ELF window
 * (sprach BSS ends at 0x127BE40, terminal loads at 0xC00000), so any
 * execve of a desktop binary scribbled over kernel data (EXC #NP,
 * "vanished" processes).  The buddy zone starts above this address
 * (see mkrn_memory_init), so the array is now a plain pointer into
 * permanently-reserved RAM instead of BSS. */
#define RAMDISK_ADDR    0x2000000u
static uint8_t *const ramdisk = (uint8_t *const)RAMDISK_ADDR;

uint64_t next_free_block = 3;
uint64_t total_allocated = 0;

int
mkrn_yafs_dev_read(uint64_t u64Lba, void *pBuf)
{
    if (u64Lba >= RAMDISK_BLOCKS)
        return -1;
    mkrn_memcpy(pBuf,
           ramdisk + u64Lba * YAFS_BTREE_BLOCK_SIZE,
           YAFS_BTREE_BLOCK_SIZE);
    return 0;
}

int
mkrn_yafs_dev_write(uint64_t u64Lba, const void *pBuf)
{
    if (u64Lba >= RAMDISK_BLOCKS)
        return -1;
    mkrn_memcpy(ramdisk + u64Lba * YAFS_BTREE_BLOCK_SIZE,
           pBuf, YAFS_BTREE_BLOCK_SIZE);
    return 0;
}

uint64_t
mkrn_yafs_dev_alloc_block(void)
{
    if (next_free_block >= RAMDISK_BLOCKS)
        return 0;
    uint64_t u64Blk = next_free_block++;
    /* lazy zeroing: fresh blocks are handed out clean (format
     * only zeroes the metadata head) */
    mkrn_memset(ramdisk + u64Blk * YAFS_BTREE_BLOCK_SIZE, 0,
               YAFS_BTREE_BLOCK_SIZE);
    total_allocated++;
    return u64Blk;
}

void
mkrn_yafs_dev_free_block(uint64_t u64Lba)
{
    (void)u64Lba;
    if (total_allocated > 0)
        total_allocated--;
}

static void
yafs_test_format(void)
{
    /* Zero only the metadata head (superblocks + first b-tree
     * blocks).  Data blocks are zeroed on first allocation in
     * mkrn_yafs_dev_alloc_block(), so no 16MB memset at boot. */
    mkrn_memset(ramdisk, 0, 4 * YAFS_BTREE_BLOCK_SIZE);

    struct yafs_superblock sb;
    mkrn_memset(&sb, 0, sizeof(sb));
    sb.magic = YAFS_MAGIC;
    sb.version = YAFS_SUPERBLOCK_VERSION;
    sb.block_size = YAFS_BTREE_BLOCK_SIZE;
    sb.total_blocks = RAMDISK_BLOCKS;
    sb.blocks_used = 3;
    sb.root_tree_addr = 0;
    sb.snapshot_tree_addr = 0;
    sb.free_tree_addr = 0;
    sb.sb_backup_addr = 1;
    sb.state_flags = YAFS_STATE_CLEAN;

    mkrn_yafs_dev_write(0, &sb);
    mkrn_yafs_dev_write(1, &sb);

    mkrn_console_write("  YAFS formatted: ");
    mkrn_console_write_dec(RAMDISK_BLOCKS);
    mkrn_console_write(" blocks, ");
    mkrn_console_write_dec(RAMDISK_SIZE / 1024);
    mkrn_console_write(" KB ramdisk\n");
}

struct walk_print_ctx {
    uint32_t count;
};

static int
walk_print(uint64_t u64Key, yafs_entry_t value,
           void *pCtx)
{
    struct walk_print_ctx *pW =
        (struct walk_print_ctx *)pCtx;
    uint64_t u64Tag = mkrn_yafs_key_tag(u64Key);
    uint64_t u64K = mkrn_yafs_key_value(u64Key);
    char tChar = '?';
    if (u64Tag == YAFS_KS_INODE)
        tChar = 'I';
    else if (u64Tag == YAFS_KS_DIR_ENTRY)
        tChar = 'D';
    else if (u64Tag == YAFS_KS_EXTENT)
        tChar = 'E';
    else if (u64Tag == YAFS_KS_SNAPSHOT)
        tChar = 'S';

    mkrn_console_write("    [");
    mkrn_console_write_dec(pW->count);
    mkrn_console_write("] tag=");
    mkrn_console_write_hex(u64Tag);
    mkrn_console_write(" (");
    mkrn_console_write(&tChar);
    mkrn_console_write(") key=0x");
    mkrn_console_write_hex(u64K);
    mkrn_console_write(" val=0x");
    mkrn_console_write_hex(value);
    mkrn_console_write("\n");

    pW->count++;
    return 0;
}

void
mkrn_yafs_test(void)
{
    mkrn_console_write(
        "\n=== YAFS B+Tree Test ===\n");

    yafs_test_format();

    next_free_block = 3;
    total_allocated = 0;

    uint64_t u64Root = 0;
    bool bInserted = false;

    mkrn_console_write(
        "  Inserting test entries...\n");

    uint64_t u64Key1 = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (1ULL << 16)
            | (mkrn_yafs_name_hash("hello") & 0xFFFF));
    mkrn_yafs_btree_insert(&u64Root, u64Key1, 100,
                           &bInserted);
    mkrn_console_write(
        "    Insert dir 1/hello -> inode 100: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");

    uint64_t u64Key2 = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (1ULL << 16)
            | (mkrn_yafs_name_hash("world") & 0xFFFF));
    mkrn_yafs_btree_insert(&u64Root, u64Key2, 200,
                           &bInserted);
    mkrn_console_write(
        "    Insert dir 1/world -> inode 200: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");

    uint64_t u64Key3 = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (1ULL << 16)
            | (mkrn_yafs_name_hash("test") & 0xFFFF));
    mkrn_yafs_btree_insert(&u64Root, u64Key3, 300,
                           &bInserted);
    mkrn_console_write(
        "    Insert dir 1/test -> inode 300: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");

    uint64_t u64InodeKey = mkrn_yafs_make_key(
        YAFS_KS_INODE, 1);
    mkrn_yafs_btree_insert(&u64Root, u64InodeKey,
                           0xABC0, &bInserted);
    mkrn_console_write(
        "    Insert inode 1 -> LBA 0xABC0: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");

    uint64_t u64ExtKey = mkrn_yafs_make_key(
        YAFS_KS_EXTENT, (1ULL << 16) | 0);
    yafs_entry_t extVal =
        mkrn_yafs_extent_pack(500, 16);
    mkrn_yafs_btree_insert(&u64Root, u64ExtKey,
                           extVal, &bInserted);
    mkrn_console_write(
        "    Insert extent 1/0 -> LBA=500 len=16: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");

    mkrn_console_write("  Lookup tests...\n");
    yafs_entry_t val;

    int ok = mkrn_yafs_btree_lookup(u64Root, u64Key1,
                                    &val);
    mkrn_console_write(
        "    Lookup dir/hello: ");
    if (ok == 0) {
        mkrn_console_write("FOUND val=");
        mkrn_console_write_dec(val);
    } else {
        mkrn_console_write("NOT FOUND");
    }
    mkrn_console_write("\n");

    ok = mkrn_yafs_btree_lookup(u64Root, u64Key2,
                                &val);
    mkrn_console_write(
        "    Lookup dir/world: ");
    if (ok == 0) {
        mkrn_console_write("FOUND val=");
        mkrn_console_write_dec(val);
    } else {
        mkrn_console_write("NOT FOUND");
    }
    mkrn_console_write("\n");

    ok = mkrn_yafs_btree_lookup(u64Root, u64Key3,
                                &val);
    mkrn_console_write(
        "    Lookup dir/test: ");
    if (ok == 0) {
        mkrn_console_write("FOUND val=");
        mkrn_console_write_dec(val);
    } else {
        mkrn_console_write("NOT FOUND");
    }
    mkrn_console_write("\n");

    uint64_t u64NotAKey = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (1ULL << 16)
            | (mkrn_yafs_name_hash("nope")
               & 0xFFFF));
    ok = mkrn_yafs_btree_lookup(u64Root, u64NotAKey,
                                &val);
    mkrn_console_write(
        "    Lookup dir/nope (should be missing): ");
    mkrn_console_write(
        ok == 0 ? "FOUND (ERROR)"
                : "not found (OK)");
    mkrn_console_write("\n");

    mkrn_console_write(
        "  Tree walk (all entries):\n");
    struct walk_print_ctx wctx = { .count = 0 };
    mkrn_yafs_btree_walk(u64Root, 0, walk_print,
                         &wctx);
    mkrn_console_write("  Total entries: ");
    mkrn_console_write_dec(wctx.count);
    mkrn_console_write("\n");

    mkrn_yafs_btree_insert(&u64Root, u64Key1, 999,
                           &bInserted);
    mkrn_console_write(
        "  Update dir/hello -> 999: ");
    mkrn_console_write(bInserted ? "CREATED"
                                 : "UPDATED");
    mkrn_console_write("\n");
    mkrn_yafs_btree_lookup(u64Root, u64Key1, &val);
    mkrn_console_write(
        "    Verifying: val=");
    mkrn_console_write_dec(val);
    mkrn_console_write(
        val == 999 ? " (correct)" : " (WRONG!)");
    mkrn_console_write("\n");

    bool bDeleted = false;
    mkrn_yafs_btree_delete(&u64Root, u64Key2,
                           &bDeleted);
    mkrn_console_write(
        "  Delete dir/world: ");
    mkrn_console_write(
        bDeleted ? "DELETED" : "not found");
    mkrn_console_write("\n");
    ok = mkrn_yafs_btree_lookup(u64Root, u64Key2,
                                &val);
    mkrn_console_write(
        "    Verifying deleted: ");
    mkrn_console_write(
        ok == 0 ? "STILL EXISTS (ERROR)"
                : "gone (OK)");
    mkrn_console_write("\n");

    mkrn_console_write(
        "  Final tree walk (after update + delete):\n");
    wctx.count = 0;
    mkrn_yafs_btree_walk(u64Root, 0, walk_print,
                         &wctx);
    mkrn_console_write("  Total entries: ");
    mkrn_console_write_dec(wctx.count);
    mkrn_console_write("\n");

    mkrn_console_write("  Blocks allocated: ");
    mkrn_console_write_dec(next_free_block - 3);
    mkrn_console_write("\n");
    mkrn_console_write("  Root LBA: 0x");
    mkrn_console_write_hex(u64Root);
    mkrn_console_write("\n");

    {
        bool bDummy;
        mkrn_yafs_btree_delete(&u64Root, u64Key1,
                               &bDummy);
        mkrn_yafs_btree_delete(&u64Root, u64Key3,
                               &bDummy);
        mkrn_yafs_btree_delete(&u64Root, u64InodeKey,
                               &bDummy);
        mkrn_yafs_btree_delete(&u64Root, u64ExtKey,
                               &bDummy);
    }

    struct yafs_superblock sb;
    mkrn_yafs_dev_read(0, &sb);
    sb.root_tree_addr = u64Root;
    mkrn_yafs_dev_write(0, &sb);

    mkrn_console_write(
        "=== YAFS Test Complete ===\n");
}
