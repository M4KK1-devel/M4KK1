/*
 * M4KK1 4P1 - ata.h
 * Description: ATA/IDE PIO driver interface (primary
 *              channel, 28-bit LBA) with MBR parsing.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ATA sector size in bytes */
#define M4K_ATA_SECTOR_SIZE     512

/* Drive indices */
#define M4K_ATA_MASTER          0
#define M4K_ATA_SLAVE           1
#define M4K_ATA_MAX_DRIVES      2

/* Primary channel I/O ports (legacy ISA) */
#define M4K_ATA_PRIMARY_DATA        0x1F0
#define M4K_ATA_PRIMARY_ERROR       0x1F1
#define M4K_ATA_PRIMARY_FEATURES    0x1F1
#define M4K_ATA_PRIMARY_SECCOUNT    0x1F2
#define M4K_ATA_PRIMARY_LBA_LO      0x1F3
#define M4K_ATA_PRIMARY_LBA_MID     0x1F4
#define M4K_ATA_PRIMARY_LBA_HI      0x1F5
#define M4K_ATA_PRIMARY_DRIVE_HEAD  0x1F6
#define M4K_ATA_PRIMARY_STATUS      0x1F7
#define M4K_ATA_PRIMARY_COMMAND     0x1F7
#define M4K_ATA_PRIMARY_ALTSTATUS   0x3F6
#define M4K_ATA_PRIMARY_CONTROL     0x3F6

/* Status register bits */
#define M4K_ATA_SR_ERR      0x01
#define M4K_ATA_SR_IDX      0x02
#define M4K_ATA_SR_CORR     0x04
#define M4K_ATA_SR_DRQ      0x08
#define M4K_ATA_SR_SRV      0x10
#define M4K_ATA_SR_DF       0x20
#define M4K_ATA_SR_RDY      0x40
#define M4K_ATA_SR_BSY      0x80

/* Commands */
#define M4K_ATA_CMD_READ_PIO        0x20
#define M4K_ATA_CMD_WRITE_PIO       0x30
#define M4K_ATA_CMD_IDENTIFY        0xEC

/* MBR constants */
#define M4K_MBR_SIGNATURE       0xAA55
#define M4K_MBR_PART_OFFSET     446
#define M4K_MBR_PART_SIZE       16
#define M4K_MBR_MAX_PARTS       4

typedef struct mkrn_ata_drive {
    bool     present;
    bool     lba28;
    uint8_t  index;
    uint64_t sectors;
    char     model[41];
} mkrn_ata_drive_t;

typedef struct mkrn_mbr_partition {
    uint8_t  status;
    uint8_t  type;
    uint32_t lba_start;
    uint32_t lba_count;
    bool     valid;
} mkrn_mbr_partition_t;

/**
 * mkrn_ata_init - Detect drives on the primary channel
 *
 * Issues IDENTIFY to master and slave, records capacity.
 * Prints "[ATA] Master drive detected: NMB" per drive.
 *
 * Return: Number of drives detected (>= 0)
 */
int mkrn_ata_init(void);

/**
 * mkrn_ata_wait_bsy - Wait until BSY clears
 * @timeout: Max poll iterations
 *
 * Return: 0 if BSY cleared, -1 on timeout
 */
int mkrn_ata_wait_bsy(uint32_t timeout);

/**
 * mkrn_ata_wait_drq - Wait until DRQ sets (data ready)
 * @timeout: Max poll iterations
 *
 * Return: 0 if DRQ set, -1 on timeout/error
 */
int mkrn_ata_wait_drq(uint32_t timeout);

/**
 * mkrn_ata_read_sectors - Read sectors via PIO (LBA28)
 * @drive: M4K_ATA_MASTER or M4K_ATA_SLAVE
 * @lba: Starting logical block address
 * @count: Number of sectors to read
 * @buf: Destination buffer (count * 512 bytes)
 *
 * Return: 0 on success, -1 on error
 */
int mkrn_ata_read_sectors(uint8_t drive, uint32_t lba,
                          uint32_t count, void *buf);

/**
 * mkrn_ata_write_sectors - Write sectors via PIO (LBA28)
 * @drive: M4K_ATA_MASTER or M4K_ATA_SLAVE
 * @lba: Starting logical block address
 * @count: Number of sectors to write
 * @buf: Source buffer (count * 512 bytes)
 *
 * Return: 0 on success, -1 on error
 */
int mkrn_ata_write_sectors(uint8_t drive, uint32_t lba,
                           uint32_t count, const void *buf);

/**
 * mkrn_ata_get_drive - Get drive descriptor
 * @drive: Drive index
 *
 * Return: Pointer to drive descriptor, NULL if invalid
 */
const mkrn_ata_drive_t *mkrn_ata_get_drive(uint8_t drive);

/**
 * mkrn_ata_get_sectors - Get total sector count of a drive
 * @drive: Drive index
 *
 * Return: Sector count, 0 if drive absent/invalid
 */
uint64_t mkrn_ata_get_sectors(uint8_t drive);

/**
 * mkrn_ata_read_mbr - Read and parse the MBR of a drive
 * @drive: Drive index
 * @parts: Output array of M4K_MBR_MAX_PARTS partitions
 *
 * Return: Number of valid partitions found, -1 on error
 */
int mkrn_ata_read_mbr(uint8_t drive,
                      mkrn_mbr_partition_t *parts);
