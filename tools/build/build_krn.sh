#!/bin/bash
# M4KK1 4P1 - build_krn.sh
# Description: Kernel build script for M4KK1 OS.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

set -e

cd "$(dirname "$0")/../.."

# Read version from VERSION file
if [ -f VERSION ]; then
    MAJOR=$(grep ^MAJOR= VERSION | cut -d= -f2)
    MINOR=$(grep ^MINOR= VERSION | cut -d= -f2)
    PATCH=$(grep ^PATCH= VERSION | cut -d= -f2)
    BUILD=$(grep ^BUILD= VERSION | cut -d= -f2)
    CLASSIFIER=$(grep ^CLASSIFIER= VERSION | cut -d= -f2)
else
    MAJOR=0; MINOR=0; PATCH=0; BUILD=0; CLASSIFIER=dev
fi

# Build ISO name per versions_standard.md
if [ -n "$CLASSIFIER" ]; then
    ISO_NAME="m4kk1_${MAJOR}.${MINOR}.${PATCH}_build${BUILD}-${CLASSIFIER}.iso"
else
    ISO_NAME="m4kk1_${MAJOR}.${MINOR}.${PATCH}_build${BUILD}.iso"
fi

CC=gcc
AS=nasm
LD=ld
CFLAGS="-Wall -Wextra -O2 -g -ffreestanding -nostdlib -nostdinc -m32 -mno-sse -mno-sse2 -mno-mmx -std=gnu99 -fno-stack-protector -fno-pic -fno-pie"
CFLAGS="$CFLAGS -I$PWD/sys/src/include -I$PWD/include -I$PWD/sys/src/arch/m4kk1/include -I$PWD/sys/src/fs/yafs/include"
ASFLAGS="-f elf32"
LDFLAGS="-T $PWD/sys/src/init/linker.ld -nostdlib -z max-page-size=0x1000 -m elf_i386 -g -no-pie"

OBJDIR=build_objs
mkdir -p $OBJDIR

echo "=== Compiling kernel C files ==="
$CC $CFLAGS -c sys/src/kernel/kmain.c -o $OBJDIR/kmain.o
$CC $CFLAGS -c sys/src/kernel/process.c -o $OBJDIR/process.o
$CC $CFLAGS -c sys/src/kernel/execve.c -o $OBJDIR/execve.o
$CC $CFLAGS -c sys/src/kernel/ldso.c -o $OBJDIR/ldso.o
$CC $CFLAGS -c sys/src/kernel/syscall.c -o $OBJDIR/syscall.o
$CC $CFLAGS -c sys/src/kernel/syscall_m4k.c -o $OBJDIR/syscall_m4k.o
$CC $CFLAGS -c sys/src/kernel/m4k_syscall.c -o $OBJDIR/m4k_syscall.o 2>/dev/null || true
# Note: m4k_syscall.c may not exist; that's OK
$CC $CFLAGS -c sys/src/kernel/signal.c -o $OBJDIR/signal.o
$CC $CFLAGS -c sys/src/kernel/namespace.c -o $OBJDIR/namespace.o
$CC $CFLAGS -c sys/src/kernel/syscall/mkrn_syscall.c -o $OBJDIR/mkrn_syscall.o
$CC $CFLAGS -c sys/src/kernel/vfs/mkrn_vfs_perms.c -o $OBJDIR/mkrn_vfs_perms.o
$CC $CFLAGS -c sys/src/kernel/proc/mkrn_sched.c -o $OBJDIR/mkrn_sched.o
$CC $CFLAGS -c sys/src/fs/procfs/mkrn_procfs.c -o $OBJDIR/mkrn_procfs.o

echo "=== Compiling kernel shell ==="
$CC $CFLAGS -c sys/src/kernel/shell.c -o $OBJDIR/shell.o

echo "=== Compiling driver files ==="
$CC $CFLAGS -c sys/src/drivers/console.c -o $OBJDIR/console.o

echo "=== Compiling MM files ==="
$CC $CFLAGS -c sys/src/mm/memory.c -o $OBJDIR/memory.o

echo "=== Compiling arch files ==="
$CC $CFLAGS -c sys/src/arch/m4kk1/timer.c -o $OBJDIR/timer.o
$CC $CFLAGS -c sys/src/arch/m4kk1/gdt.c -o $OBJDIR/gdt_c.o
$CC $CFLAGS -c sys/src/arch/m4kk1/idt.c -o $OBJDIR/idt_c.o

echo "=== Assembling arch files ==="
$AS $ASFLAGS sys/src/arch/m4kk1/gdt.asm -o $OBJDIR/gdt_asm.o
$AS $ASFLAGS sys/src/arch/m4kk1/idt.asm -o $OBJDIR/idt_asm.o

echo "=== Assembling init entry ==="
$AS $ASFLAGS sys/src/init/entry.asm -o $OBJDIR/entry.o

echo "=== Building M4SH userspace shell ==="
M4SH_CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie"
M4SH_CFLAGS="$M4SH_CFLAGS -I$PWD/sys/src/include -I$PWD/include -I$PWD/sys/src/arch/m4kk1/include"
OBJS=""
for f in $(find m4sh -name '*.c' -type f | sort); do
    o="${f%.c}.o"
    $CC $M4SH_CFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o m4sh/m4sh.elf $OBJS
echo "   M4SH ELF: m4sh/m4sh.elf ($(stat -c%s m4sh/m4sh.elf) bytes)"

echo "=== Generating init ELF data from M4SH ==="
xxd -i m4sh/m4sh.elf > init/init_elf.c
sed -i 's/m4sh_m4sh_elf/init_init_elf/g' init/init_elf.c
sed -i 's/m4sh_m4sh_elf_len/init_init_elf_len/g' init/init_elf.c
echo "   Generated init/init_elf.c ($(stat -c%s init/init_elf.c) bytes)"

echo "=== Compiling init ELF data ==="
$CC $CFLAGS -c init/init_elf.c -o $OBJDIR/init_elf.o

echo "=== Compiling VFS ==="
$CC $CFLAGS -c sys/src/fs/vfs.c -o $OBJDIR/vfs.o

echo "=== Compiling YAFS filesystem ==="
YAFS_CFLAGS="$CFLAGS -I$PWD/sys/src/fs/yafs/include"
$CC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/btree.c -o $OBJDIR/btree.o
$CC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/yafs_test.c -o $OBJDIR/yafs_test.o
$CC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/yafs_vfs.c -o $OBJDIR/yafs_vfs.o

echo "=== Compiling lib files ==="
$CC $CFLAGS -c sys/src/lib/string.c -o $OBJDIR/string.o
$CC $CFLAGS -c sys/src/lib/debug.c -o $OBJDIR/debug.o

echo "=== Linking kernel ==="
$LD $LDFLAGS -o m4kk1.krn \
    $OBJDIR/entry.o \
    $OBJDIR/kmain.o \
    $OBJDIR/process.o \
    $OBJDIR/execve.o \
    $OBJDIR/console.o \
    $OBJDIR/memory.o \
    $OBJDIR/timer.o \
    $OBJDIR/gdt_c.o \
    $OBJDIR/gdt_asm.o \
    $OBJDIR/idt_c.o \
    $OBJDIR/idt_asm.o \
    $OBJDIR/ldso.o \
    $OBJDIR/syscall.o \
    $OBJDIR/syscall_m4k.o \
    $OBJDIR/shell.o \
    $OBJDIR/init_elf.o \
    $OBJDIR/btree.o \
    $OBJDIR/yafs_test.o \
    $OBJDIR/yafs_vfs.o \
    $OBJDIR/vfs.o \
    $OBJDIR/signal.o \
    $OBJDIR/namespace.o \
    $OBJDIR/mkrn_syscall.o \
    $OBJDIR/mkrn_vfs_perms.o \
    $OBJDIR/mkrn_sched.o \
    $OBJDIR/mkrn_procfs.o \
    $OBJDIR/string.o \
    $OBJDIR/debug.o

echo "=== Kernel built: m4kk1.krn ($(stat -c%s m4kk1.krn) bytes) ==="

echo "=== Building ISO image ==="
cp m4kk1.krn iso/boot/kernel/m4kk1.krn
rm -f "$ISO_NAME"
grub-mkrescue -o "$ISO_NAME" iso 2>/dev/null
echo "=== ISO built: $ISO_NAME ($(stat -c%s "$ISO_NAME") bytes) ==="
