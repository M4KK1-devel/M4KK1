/*
 * M4KK1 4P1 - mkrn_devfs.c
 * Description: Device filesystem — exposes ATA block
 *              devices as /dev/hda, /dev/hda1, etc.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <vfs.h>
#include <kernel.h>
#include <ata.h>
#include <process.h>

#define DEVFS_MAX_FILES     64
#define DEVFS_MAX_NAME      32
#define DEVFS_FD_BASE       1000
#define DEVFS_FD_LIMIT      1024  /* == M4K_VFS_MAX_FDS boundary */
#define DEVFS_MAX_DRIVES    2
#define DEVFS_MAX_PARTS     4

typedef struct {
    int fd;
    char name[DEVFS_MAX_NAME];
    uint8_t drive;
    uint64_t start_lba;
    uint64_t size_sectors;
    uint64_t offset;
    bool is_dir;
    bool in_use;
} devfs_file_t;

static devfs_file_t devfs_files[DEVFS_MAX_FILES];
static int devfs_next_fd = DEVFS_FD_BASE;

static mkrn_mbr_partition_t devfs_parts[DEVFS_MAX_DRIVES][DEVFS_MAX_PARTS];
static int devfs_nparts[DEVFS_MAX_DRIVES];
static bool devfs_initialized = false;

void
mkrn_devfs_init(void)
{
    mkrn_memset(devfs_files, 0, sizeof(devfs_files));
    mkrn_memset(devfs_parts, 0, sizeof(devfs_parts));
    mkrn_memset(devfs_nparts, 0, sizeof(devfs_nparts));

    for (uint8_t d = 0; d < DEVFS_MAX_DRIVES; d++) {
        if (mkrn_ata_get_sectors(d) == 0)
            continue;
        int n = mkrn_ata_read_mbr(d, devfs_parts[d]);
        devfs_nparts[d] = (n > 0) ? n : 0;
    }

    devfs_initialized = true;
    M4K_LOG_INFO("DevFS initialized");
}

static devfs_file_t *
devfs_alloc_file(void)
{
    for (int i = 0; i < DEVFS_MAX_FILES; i++) {
        if (!devfs_files[i].in_use) {
            /* Wrap within our fd range: without this the monotonic
             * counter hits M4K_VFS_MAX_FDS (4096) after 96 leaked
             * fds and every /dev open fails permanently. */
            devfs_files[i].fd = devfs_next_fd++;
            if (devfs_next_fd >= DEVFS_FD_LIMIT)
                devfs_next_fd = DEVFS_FD_BASE;
            devfs_files[i].in_use = true;
            devfs_files[i].offset = 0;
            return &devfs_files[i];
        }
    }
    return NULL;
}

static devfs_file_t *
devfs_find_by_fd(int fd)
{
    for (int i = 0; i < DEVFS_MAX_FILES; i++) {
        if (devfs_files[i].in_use && devfs_files[i].fd == fd)
            return &devfs_files[i];
    }
    return NULL;
}

/**
 * devfs_parse_path - Parse /dev/<node> into drive/partition
 * @path: Full path (e.g. "/dev/hda1")
 * @pDrive: Output drive index
 * @pPart: Output partition index (-1 for raw disk)
 *
 * Return: 0 on success, -1 on unrecognized path
 */
static int
devfs_parse_path(const char *path, uint8_t *pDrive, int *pPart)
{
    const char *p = path;
    while (*p == '/')
        p++;
    if (strncmp(p, "dev/", 4) != 0)
        return -1;
    p += 4;

    /* Directory listing of /dev itself */
    if (*p == '\0')
        return -1;

    if (p[0] != 'h' || p[1] != 'd')
        return -1;

    uint8_t drive;
    if (p[2] == 'a')
        drive = 0;
    else if (p[2] == 'b')
        drive = 1;
    else
        return -1;

    int part = -1;
    if (p[3] >= '1' && p[3] <= '4')
        part = p[3] - '1';
    else if (p[3] != '\0')
        return -1;

    *pDrive = drive;
    *pPart = part;
    return 0;
}

int
mkrn_devfs_open(const char *path, int flags, int *out_fd)
{
    const char *p = path;
    while (*p == '/')
        p++;

    /* Directory listing of /dev */
    if (strcmp(p, "dev") == 0 || strcmp(p, "dev/") == 0) {
        devfs_file_t *df = devfs_alloc_file();
        if (!df)
            return -1;
        df->is_dir = true;
        df->drive = 0;
        df->start_lba = 0;
        df->size_sectors = 0;
        mkrn_strncpy(df->name, path, DEVFS_MAX_NAME - 1);
        df->name[DEVFS_MAX_NAME - 1] = '\0';
        *out_fd = df->fd;
        return 0;
    }

    /* Raw disk nodes are block-device access — restrict writes to
     * root.  Reads stay open to all (matches the /sys,/boot policy
     * in mkrn_vfs_open).  flags was previously ignored entirely,
     * letting any process open a raw disk O_RDWR. */
    if ((flags & (M4K_O_WRONLY | M4K_O_RDWR))
        && mkrn_process_get_euid() != M4K_UID_ROOT) {
        return -1;
    }

    uint8_t drive;
    int part;
    if (devfs_parse_path(path, &drive, &part) != 0)
        return -1;

    uint64_t drive_sectors = mkrn_ata_get_sectors(drive);
    if (drive_sectors == 0)
        return -1;

    uint64_t start_lba = 0;
    uint64_t size_sectors = drive_sectors;

    if (part >= 0) {
        if (part >= devfs_nparts[drive]
            || !devfs_parts[drive][part].valid)
            return -1;
        start_lba = devfs_parts[drive][part].lba_start;
        size_sectors = devfs_parts[drive][part].lba_count;
        if (start_lba + size_sectors > drive_sectors)
            size_sectors = drive_sectors - start_lba;
    }

    devfs_file_t *df = devfs_alloc_file();
    if (!df)
        return -1;
    df->is_dir = false;
    df->drive = drive;
    df->start_lba = start_lba;
    df->size_sectors = size_sectors;
    mkrn_strncpy(df->name, path, DEVFS_MAX_NAME - 1);
    df->name[DEVFS_MAX_NAME - 1] = '\0';
    *out_fd = df->fd;
    return 0;
}

int
mkrn_devfs_close(int fd)
{
    devfs_file_t *df = devfs_find_by_fd(fd);
    if (!df)
        return -1;
    df->in_use = false;
    return 0;
}

int
mkrn_devfs_is_devfs_fd(int fd)
{
    return devfs_find_by_fd(fd) ? 1 : 0;
}

int
mkrn_devfs_lseek(int fd, int offset, int whence)
{
    devfs_file_t *df = devfs_find_by_fd(fd);
    if (!df)
        return -1;
    if (df->is_dir)
        return -1;

    uint64_t size_bytes = df->size_sectors * M4K_ATA_SECTOR_SIZE;
    int64_t new_offset;
    switch (whence) {
    case M4K_SEEK_SET:
        new_offset = offset;
        break;
    case M4K_SEEK_CUR:
        new_offset = (int64_t)df->offset + offset;
        break;
    case M4K_SEEK_END:
        new_offset = (int64_t)size_bytes + offset;
        break;
    default:
        return -1;
    }
    if (new_offset < 0)
        new_offset = 0;
    if ((uint64_t)new_offset > size_bytes)
        new_offset = (int64_t)size_bytes;
    df->offset = (uint64_t)new_offset;
    return (int)new_offset;
}

int
mkrn_devfs_read(int fd, void *buf, uint32_t count)
{
    devfs_file_t *df = devfs_find_by_fd(fd);
    if (!df || !buf)
        return -1;
    if (df->is_dir)
        return -1;

    uint64_t size_bytes = df->size_sectors * M4K_ATA_SECTOR_SIZE;
    if (df->offset >= size_bytes)
        return 0;

    uint8_t *out = (uint8_t *)buf;
    uint32_t total = 0;
    uint8_t secbuf[M4K_ATA_SECTOR_SIZE];

    while (total < count && df->offset < size_bytes) {
        uint64_t abs_byte = df->start_lba * M4K_ATA_SECTOR_SIZE
                          + df->offset;
        uint32_t sector = (uint32_t)(abs_byte / M4K_ATA_SECTOR_SIZE);
        uint32_t off_in_sec = (uint32_t)(abs_byte % M4K_ATA_SECTOR_SIZE);

        /* Aligned full-sector run: batch the ATA command instead of
         * one command per sector (each single-sector PIO costs a
         * BSY+DRQ handshake — the dominant per-sector overhead). */
        if (off_in_sec == 0 && count - total >= M4K_ATA_SECTOR_SIZE) {
            uint32_t batch = (count - total) / M4K_ATA_SECTOR_SIZE;
            uint64_t remain = size_bytes - df->offset;
            if (batch > remain / M4K_ATA_SECTOR_SIZE)
                batch = (uint32_t)(remain / M4K_ATA_SECTOR_SIZE);
            if (batch > 0) {
                if (mkrn_ata_read_sectors(df->drive, sector, batch,
                                          out + total) != 0)
                    break;
                uint32_t bytes = batch * M4K_ATA_SECTOR_SIZE;
                total += bytes;
                df->offset += bytes;
                continue;
            }
        }

        if (mkrn_ata_read_sectors(df->drive, sector, 1, secbuf) != 0)
            break;

        uint32_t chunk = M4K_ATA_SECTOR_SIZE - off_in_sec;
        if (chunk > count - total)
            chunk = count - total;
        uint64_t remain = size_bytes - df->offset;
        if (chunk > remain)
            chunk = (uint32_t)remain;

        mkrn_memcpy(out + total, secbuf + off_in_sec, chunk);
        total += chunk;
        df->offset += chunk;
    }
    return (int)total;
}

int
mkrn_devfs_write(int fd, const void *buf, uint32_t count)
{
    devfs_file_t *df = devfs_find_by_fd(fd);
    if (!df || !buf)
        return -1;
    if (df->is_dir)
        return -1;

    uint64_t size_bytes = df->size_sectors * M4K_ATA_SECTOR_SIZE;
    if (df->offset >= size_bytes)
        return 0;

    const uint8_t *in = (const uint8_t *)buf;
    uint32_t total = 0;
    uint8_t secbuf[M4K_ATA_SECTOR_SIZE];

    while (total < count && df->offset < size_bytes) {
        uint64_t abs_byte = df->start_lba * M4K_ATA_SECTOR_SIZE
                          + df->offset;
        uint32_t sector = (uint32_t)(abs_byte / M4K_ATA_SECTOR_SIZE);
        uint32_t off_in_sec = (uint32_t)(abs_byte % M4K_ATA_SECTOR_SIZE);

        uint32_t chunk = M4K_ATA_SECTOR_SIZE - off_in_sec;
        if (chunk > count - total)
            chunk = count - total;
        uint64_t remain = size_bytes - df->offset;
        if (chunk > remain)
            chunk = (uint32_t)remain;

        if (off_in_sec == 0 && chunk >= M4K_ATA_SECTOR_SIZE
            && chunk - (chunk % M4K_ATA_SECTOR_SIZE) > 0) {
            /* Aligned full-sector run: write directly in one
             * multi-sector command instead of per-sector commands. */
            uint32_t batch = chunk / M4K_ATA_SECTOR_SIZE;
            if (mkrn_ata_write_sectors(df->drive, sector, batch,
                                       in + total) != 0)
                break;
        } else if (off_in_sec != 0 || chunk < M4K_ATA_SECTOR_SIZE) {
            /* Partial sector: read-modify-write */
            if (mkrn_ata_read_sectors(df->drive, sector, 1, secbuf) != 0)
                break;
            mkrn_memcpy(secbuf + off_in_sec, in + total, chunk);
            if (mkrn_ata_write_sectors(df->drive, sector, 1, secbuf) != 0)
                break;
        } else {
            if (mkrn_ata_write_sectors(df->drive, sector, 1, in + total) != 0)
                break;
        }
        total += chunk;
        df->offset += chunk;
    }
    return (int)total;
}

int
mkrn_devfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    devfs_file_t *df = devfs_find_by_fd(fd);
    if (!df || !buf || max == 0)
        return 0;
    if (!df->is_dir)
        return 0;

    uint32_t written = 0;
    for (uint8_t d = 0; d < DEVFS_MAX_DRIVES && written < max; d++) {
        if (mkrn_ata_get_sectors(d) == 0)
            continue;
        char base[4] = {'h', 'd', (char)('a' + d), '\0'};

        mkrn_memcpy(buf[written].name, base, 4);
        buf[written].type = 0;
        buf[written].size = mkrn_ata_get_sectors(d)
                            * M4K_ATA_SECTOR_SIZE;
        written++;
        if (written >= max)
            break;

        for (int p = 0; p < devfs_nparts[d] && written < max; p++) {
            if (!devfs_parts[d][p].valid)
                continue;
            char pname[6];
            pname[0] = 'h';
            pname[1] = 'd';
            pname[2] = (char)('a' + d);
            pname[3] = (char)('1' + p);
            pname[4] = '\0';
            mkrn_memcpy(buf[written].name, pname, 5);
            buf[written].type = 0;
            buf[written].size = devfs_parts[d][p].lba_count
                                * M4K_ATA_SECTOR_SIZE;
            written++;
        }
    }
    return (int)written;
}
