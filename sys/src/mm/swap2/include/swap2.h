/*
 * M4KK1 4P1 - swap2.h
 * Description: Swap2 — improved swap system header.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SWAP2_MAGIC 0x53573221

#define SWAP2_VERSION_MAJOR 0
#define SWAP2_VERSION_MINOR 1
#define SWAP2_VERSION_PATCH 0

#define SWAP2_PAGE_SIZE_4K   4096
#define SWAP2_PAGE_SIZE_8K   8192
#define SWAP2_PAGE_SIZE_16K  16384
#define SWAP2_PAGE_SIZE_32K  32768

#define SWAP2_DEFAULT_PAGE_SIZE SWAP2_PAGE_SIZE_4K
#define SWAP2_SUPERBLOCK_SIZE 65536
#define SWAP2_MAX_DEVICES 16

#define SWAP2_COMPRESSION_NONE    0
#define SWAP2_COMPRESSION_LZ4     1
#define SWAP2_COMPRESSION_ZSTD    2
#define SWAP2_COMPRESSION_LZMA    3
#define SWAP2_COMPRESSION_ADAPTIVE 4

#define SWAP2_CHECKSUM_NONE       0
#define SWAP2_CHECKSUM_CRC32C     1
#define SWAP2_CHECKSUM_SHA256     2
#define SWAP2_CHECKSUM_BLAKE3     3

#define SWAP2_PRIORITY_LOW        0
#define SWAP2_PRIORITY_NORMAL     1
#define SWAP2_PRIORITY_HIGH       2
#define SWAP2_PRIORITY_CRITICAL   3

#define SWAP2_FLAG_COMPRESSED     (1 << 0)
#define SWAP2_FLAG_ENCRYPTED      (1 << 1)
#define SWAP2_FLAG_DIRTY          (1 << 2)
#define SWAP2_FLAG_LOCKED         (1 << 3)
#define SWAP2_FLAG_REFERENCED     (1 << 4)
#define SWAP2_FLAG_ACCESSED       (1 << 5)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t used_pages;
    uint64_t journal_pages;
    uint64_t metadata_pages;
    uint32_t page_size;
    uint32_t compression_alg;
    uint32_t checksum_alg;
    uint8_t  uuid[16];
    uint64_t creation_time;
    uint32_t flags;
    uint8_t  reserved[4040];
    uint32_t checksum;
} __attribute__((packed)) swap2_superblock_t;

typedef struct {
    uint64_t virtual_address;
    uint32_t process_id;
    uint32_t original_size;
    uint32_t compressed_size;
    uint32_t compression_alg;
    uint32_t checksum_alg;
    uint64_t swap_time;
    uint32_t access_count;
    uint32_t flags;
    uint8_t  reserved[16];
    uint32_t header_checksum;
    uint32_t data_checksum;
} __attribute__((packed)) swap2_page_header_t;

typedef struct {
    uint64_t page_address;
    uint64_t swap_location;
    uint32_t swap_size;
    uint32_t compression_ratio;
    uint64_t last_access;
    uint32_t access_frequency;
    uint32_t priority;
    uint8_t  flags;
    uint8_t  reserved[7];
} __attribute__((packed)) swap2_metadata_t;

typedef struct {
    char    *device_name;
    void    *device_handle;
    uint64_t start_sector;
    uint64_t total_sectors;
    uint64_t free_sectors;
    uint32_t priority;
    bool     active;
    uint8_t  reserved[3];
} swap2_device_t;

typedef struct {
    uint64_t total_pages_swapped_in;
    uint64_t total_pages_swapped_out;
    uint64_t total_bytes_swapped_in;
    uint64_t total_bytes_swapped_out;
    uint64_t compressed_pages;
    uint64_t total_compression_ratio;
    uint64_t checksum_errors;
    uint64_t journal_entries;
    uint32_t active_devices;
    uint32_t average_swap_time;
} swap2_stats_t;

typedef struct {
    uint32_t page_size;
    uint32_t compression_alg;
    uint32_t checksum_alg;
    uint32_t max_devices;
    uint32_t journal_size_mb;
    uint32_t metadata_size_mb;
    bool     enable_compression;
    bool     enable_encryption;
    bool     enable_priority;
    bool     enable_snapshot;
    uint8_t  reserved[4];
} swap2_config_t;

int mkrn_swap2_read_superblock(
    swap2_superblock_t *pSuper, void *pDevice);
int mkrn_swap2_write_superblock(
    swap2_superblock_t *pSuper, void *pDevice);
int mkrn_swap2_verify_superblock(
    swap2_superblock_t *pSuper);
int mkrn_swap2_create_superblock(
    swap2_superblock_t *pSuper,
    uint64_t u64TotalPages, uint32_t u32PageSize,
    uint32_t u32CompressionAlg,
    uint32_t u32ChecksumAlg);

int mkrn_swap2_add_device(
    const char *pDeviceName, uint64_t u64StartSector,
    uint64_t u64SectorCount, uint32_t u32Priority);
int mkrn_swap2_remove_device(const char *pDeviceName);
int mkrn_swap2_activate_device(
    const char *pDeviceName);
int mkrn_swap2_deactivate_device(
    const char *pDeviceName);
swap2_device_t *mkrn_swap2_get_device_info(
    const char *pDeviceName);

int mkrn_swap2_swap_out(
    uint64_t u64VirtualAddress,
    uint32_t u32ProcessId, void *pPageData,
    uint32_t u32PageSize, uint32_t u32Priority);
int mkrn_swap2_swap_in(
    uint64_t u64VirtualAddress,
    uint32_t u32ProcessId, void *pPageBuffer,
    uint32_t u32BufferSize);
int mkrn_swap2_swap_free(
    uint64_t u64VirtualAddress,
    uint32_t u32ProcessId);

int mkrn_swap2_compress_page(
    void *pInput, uint32_t u32InputSize,
    void *pOutput, uint32_t *pOutputSize,
    uint32_t u32Algorithm);
int mkrn_swap2_decompress_page(
    void *pInput, uint32_t u32InputSize,
    void *pOutput, uint32_t *pOutputSize,
    uint32_t u32Algorithm);
uint32_t mkrn_swap2_calculate_checksum(
    void *pData, uint32_t u32Size,
    uint32_t u32Algorithm);

int mkrn_swap2_create_snapshot(const char *pName);
int mkrn_swap2_restore_snapshot(const char *pName);
int mkrn_swap2_list_snapshots(char *names[],
                              int *pCount);
int mkrn_swap2_delete_snapshot(const char *pName);

int mkrn_swap2_prepare_hibernate(void);
int mkrn_swap2_enter_hibernate(void);
int mkrn_swap2_resume_hibernate(void);

int mkrn_swap2_get_stats(swap2_stats_t *pStats);
int mkrn_swap2_reset_stats(void);
int mkrn_swap2_dump_stats(void);

int mkrn_swap2_set_config(swap2_config_t *pConfig);
int mkrn_swap2_get_config(swap2_config_t *pConfig);
int mkrn_swap2_save_config(const char *pConfigFile);
int mkrn_swap2_load_config(const char *pConfigFile);

void mkrn_swap2_dump_superblock(
    swap2_superblock_t *pSuper);
void mkrn_swap2_dump_page_header(
    swap2_page_header_t *pHeader);
void mkrn_swap2_dump_metadata(
    swap2_metadata_t *pMetadata);
void mkrn_swap2_dump_device_info(
    swap2_device_t *pDevice);

uint64_t mkrn_swap2_align_pages(uint64_t u64Size);
uint32_t mkrn_swap2_get_page_count(uint64_t u64Size);
uint64_t mkrn_swap2_get_swap_size(
    uint32_t u32Pages);
void mkrn_swap2_uuid_generate(uint8_t *pUuid);
uint64_t mkrn_swap2_time_current(void);

void mkrn_swap2_update_superblock_stats(
    swap2_superblock_t *pSuper);
uint64_t mkrn_swap2_get_swap_size_bytes(
    swap2_superblock_t *pSuper);
uint64_t mkrn_swap2_get_swap_size_mb(
    swap2_superblock_t *pSuper);
uint32_t mkrn_swap2_get_swap_utilization(
    swap2_superblock_t *pSuper);
bool mkrn_swap2_is_swap_available(
    swap2_superblock_t *pSuper);
void mkrn_swap2_mark_dirty(
    swap2_superblock_t *pSuper);
void mkrn_swap2_clear_dirty(
    swap2_superblock_t *pSuper);
const char *mkrn_swap2_get_status_string(
    swap2_superblock_t *pSuper);
