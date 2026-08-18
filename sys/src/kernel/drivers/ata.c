/*
 * M4KK1 4P1 - ata.c
 * Description: ATA/IDE PIO driver — IDENTIFY detection,
 *              28-bit LBA sector read/write, MBR parsing.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "ata.h"
#include <console.h>
#include <kernel.h>
#include <string.h>

static mkrn_ata_drive_t ata_drives[M4K_ATA_MAX_DRIVES];
static bool ata_initialized = false;

/* ── I/O port helpers ── */

static inline void
outb(uint16_t u16Port, uint8_t u8Value)
{
    __asm__ volatile("outb %0, %1"
                     :
                     : "a"(u8Value), "Nd"(u16Port));
}

static inline uint8_t
inb(uint16_t u16Port)
{
    uint8_t u8Value;
    __asm__ volatile("inb %1, %0"
                     : "=a"(u8Value)
                     : "Nd"(u16Port));
    return u8Value;
}

static inline void
insw(uint16_t u16Port, void *pBuf, uint32_t u32Count)
{
    __asm__ volatile("rep insw"
                     : "+D"(pBuf), "+c"(u32Count)
                     : "d"(u16Port)
                     : "memory");
}

static inline void
outsw(uint16_t u16Port, const void *pBuf, uint32_t u32Count)
{
    __asm__ volatile("rep outsw"
                     : "+S"(pBuf), "+c"(u32Count)
                     : "d"(u16Port)
                     : "memory");
}

static inline uint8_t
ata_read_status(void)
{
    return inb(M4K_ATA_PRIMARY_STATUS);
}

static inline uint8_t
ata_read_altstatus(void)
{
    return inb(M4K_ATA_PRIMARY_ALTSTATUS);
}

/**
 * ata_delay_400ns - ~400ns delay via 4 alt-status reads
 */
static void
ata_delay_400ns(void)
{
    (void)ata_read_altstatus();
    (void)ata_read_altstatus();
    (void)ata_read_altstatus();
    (void)ata_read_altstatus();
}

int
mkrn_ata_wait_bsy(uint32_t u32Timeout)
{
    while (u32Timeout > 0) {
        if (!(ata_read_altstatus() & M4K_ATA_SR_BSY))
            return 0;
        u32Timeout--;
    }
    return -1;
}

int
mkrn_ata_wait_drq(uint32_t u32Timeout)
{
    while (u32Timeout > 0) {
        uint8_t u8Status = ata_read_altstatus();
        if (u8Status & M4K_ATA_SR_ERR)
            return -1;
        if (u8Status & M4K_ATA_SR_DF)
            return -1;
        if (!(u8Status & M4K_ATA_SR_BSY)
            && (u8Status & M4K_ATA_SR_DRQ))
            return 0;
        u32Timeout--;
    }
    return -1;
}

/**
 * ata_select_drive - Select drive (master/slave), LBA mode
 * @u8Drive: 0 = master, 1 = slave
 */
static void
ata_select_drive(uint8_t u8Drive)
{
    outb(M4K_ATA_PRIMARY_DRIVE_HEAD,
         (uint8_t)(0xA0 | ((u8Drive & 1) << 4)));
    ata_delay_400ns();
}

/**
 * ata_identify - Issue IDENTIFY and fill drive descriptor
 * @u8Drive: 0 = master, 1 = slave
 * @pDrive: descriptor to fill
 *
 * Return: 0 if drive present, -1 otherwise
 */
static int
ata_identify(uint8_t u8Drive, mkrn_ata_drive_t *pDrive)
{
    ata_select_drive(u8Drive);

    outb(M4K_ATA_PRIMARY_SECCOUNT, 0);
    outb(M4K_ATA_PRIMARY_LBA_LO, 0);
    outb(M4K_ATA_PRIMARY_LBA_MID, 0);
    outb(M4K_ATA_PRIMARY_LBA_HI, 0);
    outb(M4K_ATA_PRIMARY_COMMAND, M4K_ATA_CMD_IDENTIFY);
    ata_delay_400ns();

    uint8_t u8Status = ata_read_status();
    if (u8Status == 0)
        return -1;

    if (mkrn_ata_wait_bsy(100000) != 0)
        return -1;

    /* Non-ATA device check: LBA mid/high must be 0 */
    uint8_t u8Mid = inb(M4K_ATA_PRIMARY_LBA_MID);
    uint8_t u8Hi = inb(M4K_ATA_PRIMARY_LBA_HI);
    if (u8Mid != 0 || u8Hi != 0)
        return -1;

    if (mkrn_ata_wait_drq(100000) != 0)
        return -1;

    uint16_t u16Ident[256];
    insw(M4K_ATA_PRIMARY_DATA, u16Ident, 256);

    pDrive->present = true;
    pDrive->index = u8Drive;
    pDrive->lba28 = true;

    /* Words 60-61: 28-bit LBA addressable sectors */
    pDrive->sectors = (uint64_t)u16Ident[60]
                    | ((uint64_t)u16Ident[61] << 16);

    /* Words 27-46: model string (byte-swapped) */
    for (int i = 0; i < 40; i += 2) {
        uint16_t u16Word = u16Ident[27 + i / 2];
        pDrive->model[i] = (char)(u16Word >> 8);
        pDrive->model[i + 1] = (char)(u16Word & 0xFF);
    }
    pDrive->model[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0; i--) {
        if (pDrive->model[i] == ' ' || pDrive->model[i] == '\0')
            pDrive->model[i] = '\0';
        else
            break;
    }

    return 0;
}

int
mkrn_ata_read_sectors(uint8_t u8Drive, uint32_t u32Lba,
                      uint32_t u32Count, void *pBuf)
{
    if (u8Drive >= M4K_ATA_MAX_DRIVES
        || !ata_drives[u8Drive].present)
        return -1;
    if (u32Count == 0 || pBuf == NULL)
        return -1;
    if (u32Lba >= 0x0FFFFFFF)
        return -1;

    if (mkrn_ata_wait_bsy(100000) != 0)
        return -1;

    outb(M4K_ATA_PRIMARY_SECCOUNT, (uint8_t)(u32Count & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_LO, (uint8_t)(u32Lba & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_MID, (uint8_t)((u32Lba >> 8) & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_HI, (uint8_t)((u32Lba >> 16) & 0xFF));
    outb(M4K_ATA_PRIMARY_DRIVE_HEAD,
         (uint8_t)(0xE0 | ((u8Drive & 1) << 4)
                   | ((u32Lba >> 24) & 0x0F)));
    outb(M4K_ATA_PRIMARY_COMMAND, M4K_ATA_CMD_READ_PIO);
    ata_delay_400ns();

    uint8_t *pOut = (uint8_t *)pBuf;
    for (uint32_t i = 0; i < u32Count; i++) {
        if (mkrn_ata_wait_drq(100000) != 0)
            return -1;
        insw(M4K_ATA_PRIMARY_DATA,
             pOut + i * M4K_ATA_SECTOR_SIZE, 256);
    }
    return 0;
}

int
mkrn_ata_write_sectors(uint8_t u8Drive, uint32_t u32Lba,
                       uint32_t u32Count, const void *pBuf)
{
    if (u8Drive >= M4K_ATA_MAX_DRIVES
        || !ata_drives[u8Drive].present)
        return -1;
    if (u32Count == 0 || pBuf == NULL)
        return -1;
    if (u32Lba >= 0x0FFFFFFF)
        return -1;

    if (mkrn_ata_wait_bsy(100000) != 0)
        return -1;

    outb(M4K_ATA_PRIMARY_SECCOUNT, (uint8_t)(u32Count & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_LO, (uint8_t)(u32Lba & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_MID, (uint8_t)((u32Lba >> 8) & 0xFF));
    outb(M4K_ATA_PRIMARY_LBA_HI, (uint8_t)((u32Lba >> 16) & 0xFF));
    outb(M4K_ATA_PRIMARY_DRIVE_HEAD,
         (uint8_t)(0xE0 | ((u8Drive & 1) << 4)
                   | ((u32Lba >> 24) & 0x0F)));
    outb(M4K_ATA_PRIMARY_COMMAND, M4K_ATA_CMD_WRITE_PIO);
    ata_delay_400ns();

    const uint8_t *pIn = (const uint8_t *)pBuf;
    for (uint32_t i = 0; i < u32Count; i++) {
        if (mkrn_ata_wait_drq(100000) != 0)
            return -1;
        outsw(M4K_ATA_PRIMARY_DATA,
              pIn + i * M4K_ATA_SECTOR_SIZE, 256);
    }
    /* Flush: wait for BSY to clear after last sector */
    return mkrn_ata_wait_bsy(100000);
}

const mkrn_ata_drive_t *
mkrn_ata_get_drive(uint8_t u8Drive)
{
    if (u8Drive >= M4K_ATA_MAX_DRIVES)
        return NULL;
    return &ata_drives[u8Drive];
}

uint64_t
mkrn_ata_get_sectors(uint8_t u8Drive)
{
    if (u8Drive >= M4K_ATA_MAX_DRIVES
        || !ata_drives[u8Drive].present)
        return 0;
    return ata_drives[u8Drive].sectors;
}

/**
 * ata_print_size - Print drive capacity in MB
 */
static void
ata_print_size(uint64_t u64Sectors)
{
    uint32_t u32Mb = (uint32_t)(u64Sectors / 2048);
    mkrn_console_write_dec(u32Mb);
    mkrn_console_write("MB");
}

/**
 * ata_print_detail - Print sector count detail line
 */
static void
ata_print_detail(uint64_t u64Sectors)
{
    mkrn_console_write("[ATA]   Capacity: ");
    mkrn_console_write_dec((uint32_t)u64Sectors);
    mkrn_console_write(" sectors (");
    mkrn_console_write_dec((uint32_t)(u64Sectors / 2));
    mkrn_console_write(" KB)\n");
}

int
mkrn_ata_init(void)
{
    mkrn_memset(ata_drives, 0, sizeof(ata_drives));

    mkrn_console_write("[ATA] Probing primary IDE channel...\n");

    /* Polling driver: set nIEN (bit 1 of the device control
     * register) to stop the drive asserting IRQ14 after every
     * command. */
    outb(M4K_ATA_PRIMARY_CONTROL, 0x02);
    ata_delay_400ns();

    int detected = 0;
    for (uint8_t d = 0; d < M4K_ATA_MAX_DRIVES; d++) {
        if (ata_identify(d, &ata_drives[d]) == 0) {
            detected++;
            mkrn_console_write("[ATA] ");
            mkrn_console_write(
                d == M4K_ATA_MASTER ? "Master" : "Slave");
            mkrn_console_write(" drive detected: ");
            ata_print_size(ata_drives[d].sectors);
            mkrn_console_write("\n");
            ata_print_detail(ata_drives[d].sectors);
            if (ata_drives[d].model[0] != '\0') {
                mkrn_console_write("[ATA]   Model: ");
                mkrn_console_write(ata_drives[d].model);
                mkrn_console_write("\n");
            }
        }
    }

    if (detected == 0)
        mkrn_console_write("[ATA] No ATA drives detected.\n");

    /* Parse MBR of master drive if present */
    if (ata_drives[M4K_ATA_MASTER].present) {
        mkrn_mbr_partition_t parts[M4K_MBR_MAX_PARTS];
        int n = mkrn_ata_read_mbr(M4K_ATA_MASTER, parts);
        if (n > 0) {
            mkrn_console_write("[ATA] MBR: ");
            mkrn_console_write_dec((uint32_t)n);
            mkrn_console_write(" partition(s) found\n");
            for (int i = 0; i < M4K_MBR_MAX_PARTS; i++) {
                if (!parts[i].valid)
                    continue;
                mkrn_console_write("[ATA]   part");
                mkrn_console_write_dec((uint32_t)(i + 1));
                mkrn_console_write(": type=0x");
                mkrn_console_write_hex(parts[i].type);
                mkrn_console_write(" lba=");
                mkrn_console_write_dec(parts[i].lba_start);
                mkrn_console_write(" size=");
                mkrn_console_write_dec(parts[i].lba_count);
                mkrn_console_write(" sectors\n");
            }
        } else if (n == 0) {
            mkrn_console_write(
                "[ATA] MBR: no valid partition table "
                "(raw disk)\n");
        }
    }

    ata_initialized = true;
    return detected;
}

int
mkrn_ata_read_mbr(uint8_t u8Drive, mkrn_mbr_partition_t *parts)
{
    if (parts == NULL)
        return -1;
    mkrn_memset(parts, 0,
                sizeof(mkrn_mbr_partition_t) * M4K_MBR_MAX_PARTS);

    uint8_t sector[M4K_ATA_SECTOR_SIZE];
    if (mkrn_ata_read_sectors(u8Drive, 0, 1, sector) != 0)
        return -1;

    uint16_t u16Sig = (uint16_t)(sector[510]
                                 | (sector[511] << 8));
    if (u16Sig != M4K_MBR_SIGNATURE)
        return 0;

    int valid = 0;
    for (int i = 0; i < M4K_MBR_MAX_PARTS; i++) {
        const uint8_t *e = sector + M4K_MBR_PART_OFFSET
                           + i * M4K_MBR_PART_SIZE;
        uint8_t u8Type = e[4];
        if (u8Type == 0)
            continue;
        parts[i].status = e[0];
        parts[i].type = u8Type;
        parts[i].lba_start = (uint32_t)e[8]
                           | ((uint32_t)e[9] << 8)
                           | ((uint32_t)e[10] << 16)
                           | ((uint32_t)e[11] << 24);
        parts[i].lba_count = (uint32_t)e[12]
                           | ((uint32_t)e[13] << 8)
                           | ((uint32_t)e[14] << 16)
                           | ((uint32_t)e[15] << 24);
        parts[i].valid = true;
        valid++;
    }
    return valid;
}
