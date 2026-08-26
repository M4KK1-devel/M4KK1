#!/bin/bash
# Fast iteration: compile only the failing userland unit with PCC.
cd /mnt/f/M4KK1 || exit 1
export PATH="$HOME/.local/bin:$PATH"
UCC="i386-pc-m4kk1-pcc"
M4SH_CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -Isys/src/include"
# reproduce exactly what build_krn.sh does for fm.c
$UCC $M4SH_CFLAGS -Isys/src/include -Im4sh -c usr/src/cmd/fm.c -o /tmp/fm.o 2>&1 | head -30
echo "FM_RC=$?"
