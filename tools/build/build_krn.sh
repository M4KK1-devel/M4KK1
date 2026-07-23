#!/bin/bash
# M4KK1 4P1 - build_krn.sh
# Description: Kernel build script for M4KK1 OS.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

set -e

cd "$(dirname "$0")/../.."

# === Prerequisite checks ===
check_prereqs() {
    local fail=0
    for cmd in gcc ld xxd find mkdir rm cp; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            echo "ERROR: Required command '$cmd' not found" >&2
            fail=1
        fi
    done
    for dir in sys/src/kernel m4sh init; do
        if [ ! -d "$dir" ]; then
            echo "ERROR: Required directory '$dir' not found (run from project root)" >&2
            fail=1
        fi
    done
    if [ -d ./usr/bin ] && [ ! -w ./usr/bin ]; then
        echo "ERROR: ./usr/bin exists but is not writable" >&2
        fail=1
    fi
    if [ ! -d ./usr/bin ]; then
        if [ -d ./usr ] && [ ! -w ./usr ]; then
            echo "ERROR: ./usr exists but is not writable, cannot create ./usr/bin" >&2
            fail=1
        fi
    fi
    if ! ld -m elf_i386 -v >/dev/null 2>&1; then
        echo "WARNING: ld -m elf_i386 not supported, linking may fail" >&2
    fi
    if [ "$fail" -ne 0 ]; then
        echo "Prerequisite check FAILED. Aborting." >&2
        exit 1
    fi
}
check_prereqs

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

# === Clean ./usr/bin/ before build ===
if [ -d ./usr/bin ]; then
    rm -rf ./usr/bin/*
else
    mkdir -p ./usr/bin
fi
echo "   ./usr/bin/ cleaned"

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
$CC $CFLAGS -c sys/src/kernel/session.c -o $OBJDIR/session.o
$CC $CFLAGS -c sys/src/kernel/vfs/mkrn_vfs_perms.c -o $OBJDIR/mkrn_vfs_perms.o
$CC $CFLAGS -c sys/src/kernel/proc/mkrn_sched.c -o $OBJDIR/mkrn_sched.o
$CC $CFLAGS -c sys/src/fs/procfs/mkrn_procfs.c -o $OBJDIR/mkrn_procfs.o
$CC $CFLAGS -c sys/src/fs/sessions/mkrn_sessions.c -o $OBJDIR/mkrn_sessions.o
$CC $CFLAGS -c sys/src/fs/device_tree/mkrn_device_tree.c -o $OBJDIR/mkrn_device_tree.o

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
M4SH_CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -Isys/src/include"
M4SH_CFLAGS="$M4SH_CFLAGS -I$PWD/sys/src/include -I$PWD/include -I$PWD/sys/src/arch/m4kk1/include -I$PWD/m4sh -Isys/src/include"
OBJS=""
for f in $(find m4sh usr/src/cmd -name '*.c' -type f ! -path 'm4sh/login/*' | sort); do
    o="${f%.c}.o"
    $CC $M4SH_CFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o m4sh/m4sh.elf $OBJS
echo "   M4SH ELF: m4sh/m4sh.elf ($(stat -c%s m4sh/m4sh.elf) bytes)"

echo "=== Building standalone login ELF ==="
$CC $M4SH_CFLAGS -c m4sh/login/login_standalone.c -o m4sh/login/login_standalone.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o m4sh/login/login.elf m4sh/login/login_standalone.o
echo "   Login ELF: m4sh/login/login.elf ($(stat -c%s m4sh/login/login.elf) bytes)"

echo "=== Building init ELF ==="
$CC $M4SH_CFLAGS -c init/init.c -o init/init.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o init/init.elf init/init.o
echo "   Init ELF: init/init.elf ($(stat -c%s init/init.elf) bytes)"

echo "=== Generating ELF data arrays ==="
xxd -i m4sh/m4sh.elf > init/m4sh_elf_.c
sed 's/m4sh_m4sh_elf/m4sh_init_elf/g; s/m4sh_m4sh_elf_len/m4sh_init_elf_len/g' init/m4sh_elf_.c > init/m4sh_elf.c
rm -f init/m4sh_elf_.c
echo "   Generated init/m4sh_elf.c"

xxd -i m4sh/login/login.elf > init/login_elf_.c
sed 's/m4sh_login_login_elf/login_init_elf/g; s/m4sh_login_login_elf_len/login_init_elf_len/g' init/login_elf_.c > init/login_elf.c
rm -f init/login_elf_.c
echo "   Generated init/login_elf.c"

xxd -i init/init.elf > init/init_elf.c
echo "   Generated init/init_elf.c"

echo "=== Copying ELFs to ./usr/bin/ ==="
cp -f m4sh/m4sh.elf ./usr/bin/m4sh
cp -f m4sh/login/login.elf ./usr/bin/login
cp -f init/init.elf ./usr/bin/init
for f in usr/src/cmd/*.elf; do
    [ -f "$f" ] && cp -f "$f" ./usr/bin/ || true
done
echo "   Copied $(ls ./usr/bin/ | wc -l) ELF(s) to ./usr/bin/"

echo "=== Compiling ELF data arrays ==="
$CC $CFLAGS -c init/init_elf.c -o $OBJDIR/init_elf.o
$CC $CFLAGS -c init/m4sh_elf.c -o $OBJDIR/m4sh_elf.o
$CC $CFLAGS -c init/login_elf.c -o $OBJDIR/login_elf.o

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
    $OBJDIR/m4sh_elf.o \
    $OBJDIR/login_elf.o \
    $OBJDIR/btree.o \
    $OBJDIR/yafs_test.o \
    $OBJDIR/yafs_vfs.o \
    $OBJDIR/vfs.o \
    $OBJDIR/signal.o \
    $OBJDIR/namespace.o \
    $OBJDIR/mkrn_syscall.o \
    $OBJDIR/session.o \
    $OBJDIR/mkrn_vfs_perms.o \
    $OBJDIR/mkrn_sched.o \
    $OBJDIR/mkrn_procfs.o \
    $OBJDIR/mkrn_sessions.o \
    $OBJDIR/mkrn_device_tree.o \
    $OBJDIR/string.o \
    $OBJDIR/debug.o

echo "=== Kernel built: m4kk1.krn ($(stat -c%s m4kk1.krn) bytes) ==="

echo "=== Building ISO image ==="
ISODIR=$(mktemp -d /tmp/m4kk1_isodir.XXXXXX)
mkdir -p "$ISODIR/boot/grub" "$ISODIR/boot/kernel"
cp m4kk1.krn "$ISODIR/boot/kernel/m4kk1.krn"
cat > "$ISODIR/boot/grub/grub.cfg" << GRUB
set timeout=0
set default=0
menuentry "M4KK1" {
    multiboot /boot/kernel/m4kk1.krn
    boot
}
GRUB
mkdir -p iso
rm -f iso/"$ISO_NAME"
grub-mkrescue -o iso/"$ISO_NAME" "$ISODIR"
rm -rf "$ISODIR"
echo "=== ISO built: iso/$ISO_NAME ($(stat -c%s "iso/$ISO_NAME") bytes) ==="
