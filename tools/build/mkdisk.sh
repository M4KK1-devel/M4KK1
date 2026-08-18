#!/bin/bash
# M4KK1 4P1 - mkdisk.sh
# Description: Create a 64MB raw ATA disk image (disk.img)
#              with an MBR partition table for QEMU -hda.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

set -e

cd "$(dirname "$0")/../.."

DISK="disk.img"
SIZE_MB=64

echo "=== Creating ${SIZE_MB}MB raw disk image: ${DISK} ==="
python3 - "$DISK" "$SIZE_MB" << 'PYEOF'
import sys, struct

disk = sys.argv[1]
size_mb = int(sys.argv[2])
total_sectors = size_mb * 1024 * 1024 // 512

# Partition 1: starts at LBA 2048, spans rest of disk
part_start = 2048
part_count = total_sectors - part_start

mbr = bytearray(512)
# ASCII marker in bootstrap area so userspace `cat` can verify
# the exact bytes read back from /dev/hda.
marker = b'M4KK1MBR'
mbr[0:len(marker)] = marker
# Partition entry 1 at offset 446
e = 446
mbr[e + 0] = 0x00                 # status (non-bootable)
mbr[e + 1] = 0x00                 # CHS start head
mbr[e + 2] = 0x00                 # CHS start sector/cyl
mbr[e + 3] = 0x00                 # CHS start cyl
mbr[e + 4] = 0x83                 # type: Linux
mbr[e + 5] = 0xFE                 # CHS end head
mbr[e + 6] = 0xFF                 # CHS end sector
mbr[e + 7] = 0xFF                 # CHS end cyl
struct.pack_into('<I', mbr, e + 8, part_start)
struct.pack_into('<I', mbr, e + 12, part_count)
# Signature
mbr[510] = 0x55
mbr[511] = 0xAA

with open(disk, 'wb') as f:
    f.write(mbr)
    # Zero-fill the rest of the disk
    remaining = total_sectors * 512 - 512
    f.write(b'\x00' * remaining)

print("  total_sectors=%d  part1: lba=%d count=%d (type=0x83)"
      % (total_sectors, part_start, part_count))
PYEOF
echo "   disk.img created: $(stat -c%s "$DISK") bytes"
