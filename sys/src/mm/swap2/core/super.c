/*
 * M4KK1 4P1 - super.c
 * Description: Swap2 superblock operations — read, write,
 *              verify, create, and dump.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "swap2.h"
#include "../include/swap2.h"
#include "../../y4ku/include/console.h"

int
mkrn_swap2_read_superblock(
    swap2_superblock_t *pSuper, void *pDevice)
{
    if (!pSuper || !pDevice)
        return -1;

    int ret = pDevice->read(
        pDevice, 0, (uint8_t *)pSuper,
        SWAP2_SUPERBLOCK_SIZE);
    if (ret != SWAP2_SUPERBLOCK_SIZE) {
        mkrn_console_write(
            "Failed to read Swap2 superblock\n");
        return -1;
    }

    if (pSuper->magic != SWAP2_MAGIC) {
        mkrn_console_write(
            "Invalid Swap2 magic: 0x");
        mkrn_console_write_hex(pSuper->magic);
        mkrn_console_write("\n");
        return -1;
    }

    uint32_t u32SavedChecksum = pSuper->checksum;
    pSuper->checksum = 0;
    uint32_t u32Calculated =
        mkrn_swap2_checksum_crc32c(
            pSuper, SWAP2_SUPERBLOCK_SIZE);

    if (u32SavedChecksum != u32Calculated) {
        mkrn_console_write(
            "Swap2 superblock checksum mismatch\n");
        return -1;
    }

    pSuper->checksum = u32SavedChecksum;

    mkrn_console_write(
        "Swap2 superblock read successfully\n");
    mkrn_console_write("Page size: ");
    mkrn_console_write_dec(pSuper->page_size);
    mkrn_console_write(" bytes\n");
    mkrn_console_write("Total pages: ");
    mkrn_console_write_dec(pSuper->total_pages);
    mkrn_console_write("\n");
    mkrn_console_write("Free pages: ");
    mkrn_console_write_dec(pSuper->free_pages);
    mkrn_console_write("\n");

    return 0;
}

int
mkrn_swap2_write_superblock(
    swap2_superblock_t *pSuper, void *pDevice)
{
    if (!pSuper || !pDevice)
        return -1;

    pSuper->checksum = 0;
    pSuper->checksum = mkrn_swap2_checksum_crc32c(
        pSuper, SWAP2_SUPERBLOCK_SIZE);

    int ret = pDevice->write(
        pDevice, 0, (uint8_t *)pSuper,
        SWAP2_SUPERBLOCK_SIZE);
    if (ret != SWAP2_SUPERBLOCK_SIZE) {
        mkrn_console_write(
            "Failed to write Swap2 superblock\n");
        return -1;
    }

    mkrn_console_write(
        "Swap2 superblock written successfully\n");
    return 0;
}

int
mkrn_swap2_verify_superblock(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return -1;

    if (pSuper->magic != SWAP2_MAGIC)
        return -1;

    if (pSuper->version
        != (SWAP2_VERSION_MAJOR << 16
            | SWAP2_VERSION_MINOR << 8
            | SWAP2_VERSION_PATCH))
    {
        mkrn_console_write(
            "Swap2 version mismatch\n");
        return -1;
    }

    if (pSuper->page_size
        != SWAP2_DEFAULT_PAGE_SIZE)
    {
        mkrn_console_write(
            "Unsupported page size: ");
        mkrn_console_write_dec(pSuper->page_size);
        mkrn_console_write("\n");
        return -1;
    }

    uint32_t u32SavedChecksum = pSuper->checksum;
    pSuper->checksum = 0;
    uint32_t u32Calculated =
        mkrn_swap2_checksum_crc32c(
            pSuper, SWAP2_SUPERBLOCK_SIZE);

    if (u32SavedChecksum != u32Calculated) {
        mkrn_console_write(
            "Swap2 superblock checksum "
            "verification failed\n");
        return -1;
    }

    pSuper->checksum = u32SavedChecksum;
    return 0;
}

int
mkrn_swap2_create_superblock(
    swap2_superblock_t *pSuper,
    uint64_t u64TotalPages, uint32_t u32PageSize,
    uint32_t u32CompressionAlg,
    uint32_t u32ChecksumAlg)
{
    if (!pSuper)
        return -1;

    mkrn_swap2_memset(pSuper, 0,
                      SWAP2_SUPERBLOCK_SIZE);

    pSuper->magic = SWAP2_MAGIC;
    pSuper->version =
        (SWAP2_VERSION_MAJOR << 16)
        | (SWAP2_VERSION_MINOR << 8)
        | SWAP2_VERSION_PATCH;
    pSuper->total_pages = u64TotalPages;
    pSuper->free_pages = u64TotalPages - 1;
    pSuper->used_pages = 1;
    pSuper->journal_pages = 32768;
    pSuper->metadata_pages = 65536;
    pSuper->page_size = u32PageSize;
    pSuper->compression_alg = u32CompressionAlg;
    pSuper->checksum_alg = u32ChecksumAlg;

    mkrn_swap2_uuid_generate(pSuper->uuid);
    pSuper->creation_time =
        mkrn_swap2_time_current();
    pSuper->flags =
        SWAP2_FLAG_COMPRESSED | SWAP2_FLAG_DIRTY;
    pSuper->checksum = mkrn_swap2_checksum_crc32c(
        pSuper, SWAP2_SUPERBLOCK_SIZE);

    return 0;
}

void
mkrn_swap2_update_superblock_stats(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return;

    pSuper->checksum = 0;
    pSuper->checksum = mkrn_swap2_checksum_crc32c(
        pSuper, SWAP2_SUPERBLOCK_SIZE);
}

void
mkrn_swap2_dump_superblock(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return;

    mkrn_console_write(
        "=== Swap2 Superblock Dump ===\n");
    mkrn_console_write("Magic: 0x");
    mkrn_console_write_hex(pSuper->magic);
    mkrn_console_write("\n");

    mkrn_console_write("Version: ");
    mkrn_console_write_dec(
        pSuper->version >> 16);
    mkrn_console_write(".");
    mkrn_console_write_dec(
        (pSuper->version >> 8) & 0xFF);
    mkrn_console_write(".");
    mkrn_console_write_dec(
        pSuper->version & 0xFF);
    mkrn_console_write("\n");

    mkrn_console_write("Page size: ");
    mkrn_console_write_dec(pSuper->page_size);
    mkrn_console_write(" bytes\n");
    mkrn_console_write("Total pages: ");
    mkrn_console_write_dec(pSuper->total_pages);
    mkrn_console_write("\n");
    mkrn_console_write("Free pages: ");
    mkrn_console_write_dec(pSuper->free_pages);
    mkrn_console_write("\n");
    mkrn_console_write("Used pages: ");
    mkrn_console_write_dec(pSuper->used_pages);
    mkrn_console_write("\n");
    mkrn_console_write("Journal pages: ");
    mkrn_console_write_dec(pSuper->journal_pages);
    mkrn_console_write("\n");
    mkrn_console_write("Metadata pages: ");
    mkrn_console_write_dec(pSuper->metadata_pages);
    mkrn_console_write("\n");
    mkrn_console_write(
        "Compression algorithm: ");
    mkrn_console_write_dec(
        pSuper->compression_alg);
    mkrn_console_write("\n");
    mkrn_console_write(
        "Checksum algorithm: ");
    mkrn_console_write_dec(pSuper->checksum_alg);
    mkrn_console_write("\n");

    mkrn_console_write("UUID: ");
    for (int i = 0; i < 16; i++) {
        mkrn_console_write_hex(pSuper->uuid[i]);
        if (i < 15)
            mkrn_console_write("-");
    }
    mkrn_console_write("\n");

    mkrn_console_write("Creation time: ");
    mkrn_console_write_dec(pSuper->creation_time);
    mkrn_console_write("\n");
    mkrn_console_write("Flags: 0x");
    mkrn_console_write_hex(pSuper->flags);
    mkrn_console_write("\n");
    mkrn_console_write("Checksum: 0x");
    mkrn_console_write_hex(pSuper->checksum);
    mkrn_console_write("\n");
    mkrn_console_write(
        "=== End Superblock Dump ===\n");
}

uint64_t
mkrn_swap2_get_swap_size_bytes(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return 0;
    return pSuper->total_pages * pSuper->page_size;
}

uint64_t
mkrn_swap2_get_swap_size_mb(
    swap2_superblock_t *pSuper)
{
    return mkrn_swap2_get_swap_size_bytes(pSuper)
           / (1024 * 1024);
}

uint32_t
mkrn_swap2_get_swap_utilization(
    swap2_superblock_t *pSuper)
{
    if (!pSuper || pSuper->total_pages == 0)
        return 0;
    return (uint32_t)(
        (pSuper->used_pages * 100)
        / pSuper->total_pages);
}

bool
mkrn_swap2_is_swap_available(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return false;
    return (pSuper->magic == SWAP2_MAGIC)
           && (pSuper->free_pages > 0)
           && ((pSuper->flags & SWAP2_FLAG_DIRTY)
               == 0);
}

void
mkrn_swap2_mark_dirty(swap2_superblock_t *pSuper)
{
    if (pSuper)
        pSuper->flags |= SWAP2_FLAG_DIRTY;
}

void
mkrn_swap2_clear_dirty(swap2_superblock_t *pSuper)
{
    if (pSuper)
        pSuper->flags &= ~SWAP2_FLAG_DIRTY;
}

const char *
mkrn_swap2_get_status_string(
    swap2_superblock_t *pSuper)
{
    if (!pSuper)
        return "INVALID";
    if (pSuper->magic != SWAP2_MAGIC)
        return "INVALID_MAGIC";
    if (pSuper->flags & SWAP2_FLAG_DIRTY)
        return "DIRTY";
    if (pSuper->free_pages == 0)
        return "FULL";
    return "CLEAN";
}
