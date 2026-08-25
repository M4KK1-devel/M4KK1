#!/bin/bash
# Temporary probe rebuild: copland.elf with -DCOPLAND_FRAME_DEBUG
set -e
cd /mnt/f/M4KK1
export PATH="$HOME/.local/bin:$PATH"
UCC=i386-pc-m4kk1-pcc
LD=ld
CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -DM4K_FULL -DM4K_TEST_AUTOLOGIN -Isys/src/include -I$PWD/sys/src/include -I$PWD/include -I$PWD/sys/src/arch/m4kk1/include -I$PWD/m4sh -I$PWD/usr/src/lib/m4k_libc/include"
$UCC $CFLAGS -DCOPLAND_FRAME_DEBUG -c usr/src/cmd/copland.c -o usr/src/cmd/copland.o
echo "copland.o probe-compiled: $(stat -c%s usr/src/cmd/copland.o) bytes"
