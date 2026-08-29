#!/bin/bash
# M4KK1 4P1 - build_krn.sh
# Description: Kernel build script for M4KK1 OS.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

set -e

cd "$(dirname "$0")/../.."

# === Build modes ===
# full      : complete desktop system (graphics + Copland + Sprach + PCC + all tools) [default]
# cmd-only  : command-line system (no graphics; kernel + VFS + YAFS + m4sh + coreutils)
# minimal   : minimal kernel (boot + serial banner only, then halt)
# recovery  : like cmd-only, plus fsck / reset-passwd tools, forced serial login
usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Build modes:
  --full        Full desktop system: graphics drivers, Copland display server,
                Sprach window manager (stack/tiling/scroll), PCC compiler and
                all tools. This is the default mode.
  --cmd-only    Command-line only: no graphics. Kernel + VFS + YAFS + m4sh shell
                + text tools (ls, cat, echo, grep, wc, ...). Boots straight to
                a serial login/shell. Best for demos, perf tests, low memory.
  --minimal     Minimal kernel: boot + serial banner only, prints
                "[MINIMAL] System ready" and halts. For boot tests.
  --recovery    Recovery mode: like --cmd-only (serial login), plus
                /bin/fsck and /bin/reset-passwd tools. Run fsck at init.

Common flags:
  --output <dir>  ISO output directory (default: output)
  --help          Show this help and exit.
  --version       Print build version and exit.

Example:
  $0 --cmd-only
  $0 --full --output dist
EOF
}

BUILD_MODE="full"      # default
OUTPUT_DIR="output"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cmd-only) BUILD_MODE="cmd-only"; shift ;;
        --full) BUILD_MODE="full"; shift ;;
        --full-test) BUILD_MODE="full-test"; shift ;;
        --minimal) BUILD_MODE="minimal"; shift ;;
        --recovery) BUILD_MODE="recovery"; shift ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        --help) usage; exit 0 ;;
        --version)
            if [ -f VERSION ]; then
                MAJOR=$(grep ^MAJOR= VERSION | cut -d= -f2)
                MINOR=$(grep ^MINOR= VERSION | cut -d= -f2)
                PATCH=$(grep ^PATCH= VERSION | cut -d= -f2)
                BUILD=$(grep ^BUILD= VERSION | cut -d= -f2)
                echo "M4KK1 Build v${MAJOR}.${MINOR}.${PATCH} (build ${BUILD})"
            else
                echo "M4KK1 Build v0.0.0"
            fi
            exit 0 ;;
        *) echo "ERROR: Unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

case "$BUILD_MODE" in
    minimal)   MODE_DEFINES="-DM4K_MINIMAL";   NEED_USER=0; NEED_GRAPHICS=0; NEED_RECOVERY=0 ;;
    cmd-only)  MODE_DEFINES="-DM4K_CMD_ONLY";  NEED_USER=1; NEED_GRAPHICS=0; NEED_RECOVERY=0 ;;
    recovery)  MODE_DEFINES="-DM4K_RECOVERY -DM4K_CMD_ONLY"; NEED_USER=1; NEED_GRAPHICS=0; NEED_RECOVERY=1 ;;
    full)      MODE_DEFINES="-DM4K_FULL";      NEED_USER=1; NEED_GRAPHICS=1; NEED_RECOVERY=0 ;;
    full-test) MODE_DEFINES="-DM4K_FULL -DM4K_TEST_AUTOLOGIN -DM4SH_NO_LOGIN_GATE"; NEED_USER=1; NEED_GRAPHICS=1; NEED_RECOVERY=0 ;;
    *) echo "ERROR: Invalid build mode '$BUILD_MODE'" >&2; exit 1 ;;
esac
echo "=== Build mode: $BUILD_MODE ==="

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

# Build ISO name per versions_standard.md (mode-tagged)
if [ -n "$CLASSIFIER" ]; then
    ISO_NAME="m4kk1_${MAJOR}.${MINOR}.${PATCH}_build${BUILD}-${CLASSIFIER}-${BUILD_MODE}.iso"
else
    ISO_NAME="m4kk1_${MAJOR}.${MINOR}.${PATCH}_build${BUILD}-${BUILD_MODE}.iso"
fi

# 检测PCC交叉编译器（用户态优先使用 i386-pc-m4kk1-pcc，回退宿主 gcc）
if command -v i386-pc-m4kk1-pcc >/dev/null 2>&1; then
    UCC=i386-pc-m4kk1-pcc
    echo "Userspace compiler (PCC cross): $UCC"
elif [ -x "$HOME/.local/bin/i386-pc-m4kk1-pcc" ]; then
    export PATH="$HOME/.local/bin:$PATH"
    UCC=i386-pc-m4kk1-pcc
    echo "Userspace compiler (PCC cross from ~/.local): $UCC"
else
    UCC=gcc
    echo "PCC not found, userspace compiler (native gcc): $UCC"
fi
# 内核仍使用宿主 gcc：内核头文件（multiboot.h 等）为 GCC 专用，pcc 无法编译
KCC=gcc
echo "Kernel compiler (native gcc): $KCC"

# 检测Make工具
if command -v make >/dev/null 2>&1; then
    MAKE=make
    echo "Using Make: $MAKE"
else
    MAKE=
    echo "WARNING: Make not found, using manual build"
fi

AS=nasm
LD=ld
CFLAGS="-Wall -Wextra -O2 -g -ffreestanding -nostdlib -nostdinc -m32 -mno-sse -mno-sse2 -mno-mmx -std=gnu99 -fno-stack-protector -fno-pic -fno-pie"
CFLAGS="$CFLAGS $MODE_DEFINES"
CFLAGS="$CFLAGS -I$PWD/sys/src/include -I$PWD/include -I$PWD/init -I$PWD/sys/src/arch/m4kk1/include -I$PWD/sys/src/fs/yafs/include -I$PWD/sys/src/drivers/mouse -I$PWD/sys/src/drivers/keyboard -I$PWD/sys/src/kernel/drivers"
ASFLAGS="-f elf32"
LDFLAGS="-T $PWD/sys/src/init/linker.ld -nostdlib -z max-page-size=0x1000 -m elf_i386 -g -no-pie"

OBJDIR=build_objs
mkdir -p $OBJDIR
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/*.krn "$OUTPUT_DIR"/*.iso

# === Clean ./usr/bin/ before build ===
if [ -d ./usr/bin ]; then
    rm -rf ./usr/bin/*
else
    mkdir -p ./usr/bin
fi
echo "   ./usr/bin/ cleaned"

echo "=== Compiling kernel C files ==="
# Manual pages: bake docs/zh_CN/man into init/man_pages.[ch] before
# kmain.c is compiled (it includes man_pages.h in non-minimal builds).
bash tools/build/gen_man_pages.sh
if [ "$NEED_GRAPHICS" = 1 ]; then
    $KCC $CFLAGS -c sys/src/kernel/video/vesa.c -o $OBJDIR/vesa.o
fi
$KCC $CFLAGS -c sys/src/kernel/kmain.c -o $OBJDIR/kmain.o
$KCC $CFLAGS -c sys/src/kernel/process.c -o $OBJDIR/process.o
$KCC $CFLAGS -c sys/src/kernel/execve.c -o $OBJDIR/execve.o
$KCC $CFLAGS -c sys/src/kernel/ldso.c -o $OBJDIR/ldso.o
$KCC $CFLAGS -c sys/src/kernel/syscall.c -o $OBJDIR/syscall.o
$KCC $CFLAGS -c sys/src/kernel/syscall_m4k.c -o $OBJDIR/syscall_m4k.o
$KCC $CFLAGS -c sys/src/kernel/m4k_syscall.c -o $OBJDIR/m4k_syscall.o 2>/dev/null || true
# Note: m4k_syscall.c may not exist; that's OK
$KCC $CFLAGS -c sys/src/kernel/signal.c -o $OBJDIR/signal.o
$KCC $CFLAGS -c sys/src/kernel/namespace.c -o $OBJDIR/namespace.o
$KCC $CFLAGS -c sys/src/kernel/syscall/mkrn_syscall.c -o $OBJDIR/mkrn_syscall.o
$KCC $CFLAGS -c sys/src/kernel/session.c -o $OBJDIR/session.o
$KCC $CFLAGS -c sys/src/kernel/vfs/mkrn_vfs_perms.c -o $OBJDIR/mkrn_vfs_perms.o
$KCC $CFLAGS -c sys/src/fs/procfs/mkrn_procfs.c -o $OBJDIR/mkrn_procfs.o
$KCC $CFLAGS -c sys/src/fs/devfs/mkrn_devfs.c -o $OBJDIR/mkrn_devfs.o
$KCC $CFLAGS -c sys/src/fs/sessions/mkrn_sessions.c -o $OBJDIR/mkrn_sessions.o
$KCC $CFLAGS -c sys/src/fs/device_tree/mkrn_device_tree.c -o $OBJDIR/mkrn_device_tree.o

echo "=== Compiling kernel shell ==="
$KCC $CFLAGS -c sys/src/kernel/shell.c -o $OBJDIR/shell.o

echo "=== Compiling driver files ==="
$KCC $CFLAGS -c sys/src/drivers/console.c -o $OBJDIR/console.o
$KCC $CFLAGS -c sys/src/drivers/mouse/mouse.c -o $OBJDIR/mouse.o
$KCC $CFLAGS -c sys/src/drivers/keyboard/keyboard.c -o $OBJDIR/keyboard.o
$KCC $CFLAGS -c sys/src/kernel/drivers/pci.c -o $OBJDIR/pci.o
$KCC $CFLAGS -c sys/src/kernel/drivers/ata.c -o $OBJDIR/ata.o
$KCC $CFLAGS -c sys/src/kernel/drivers/sb16.c -o $OBJDIR/sb16.o

echo "=== Compiling network stack ==="
$KCC $CFLAGS -c sys/src/net/e1000.c -o $OBJDIR/e1000.o
$KCC $CFLAGS -c sys/src/net/net.c -o $OBJDIR/net.o
$KCC $CFLAGS -c sys/src/net/arp.c -o $OBJDIR/arp.o
$KCC $CFLAGS -c sys/src/net/tcp.c -o $OBJDIR/tcp.o
$KCC $CFLAGS -c sys/src/kernel/syscall/syscall_net.c -o $OBJDIR/syscall_net.o

echo "=== Compiling MM files ==="
$KCC $CFLAGS -c sys/src/mm/memory.c -o $OBJDIR/memory.o

echo "=== Compiling arch files ==="
$KCC $CFLAGS -c sys/src/arch/m4kk1/timer.c -o $OBJDIR/timer.o
$KCC $CFLAGS -c sys/src/arch/m4kk1/gdt.c -o $OBJDIR/gdt_c.o
$KCC $CFLAGS -c sys/src/arch/m4kk1/idt.c -o $OBJDIR/idt_c.o

echo "=== Assembling arch files ==="
$AS $ASFLAGS sys/src/arch/m4kk1/gdt.asm -o $OBJDIR/gdt_asm.o
$AS $ASFLAGS sys/src/arch/m4kk1/idt.asm -o $OBJDIR/idt_asm.o

echo "=== Assembling init entry ==="
$AS $ASFLAGS sys/src/init/entry.asm -o $OBJDIR/entry.o

if [ "$NEED_USER" = 1 ]; then
echo "=== Building M4SH userspace shell ==="
M4SH_CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 -fno-stack-protector -fno-pic -fno-pie -Isys/src/include"
M4SH_CFLAGS="$M4SH_CFLAGS $MODE_DEFINES"
M4SH_CFLAGS="$M4SH_CFLAGS -I$PWD/sys/src/include -I$PWD/include -I$PWD/sys/src/arch/m4kk1/include -I$PWD/m4sh -Isys/src/include -I$PWD/usr/src/lib/m4k_libc/include"
# PCC 编译产物需要编译器运行时（__divdi3 等），等价于 gcc 隐式链接的 libgcc
PCC_RUNTIME="$PWD/usr/src/lib/m4k_libc/libpcc.a"
OBJS=""
for f in $(find m4sh usr/src/cmd -name '*.c' -type f ! -path 'm4sh/login/*' ! -path 'usr/src/cmd/mdm.c' ! -path 'usr/src/cmd/mdm_mini.c' ! -path 'usr/src/cmd/flip_test.c' ! -path 'usr/src/cmd/copland.c' ! -path 'usr/src/cmd/cptest.c' ! -path 'usr/src/cmd/terminal.c' ! -path 'usr/src/cmd/fm.c' ! -path 'usr/src/cmd/altr.c' ! -path 'usr/src/cmd/calc_gui.c' ! -path 'usr/src/lib/*' | sort); do
    o="${f%.c}.o"
    $UCC $M4SH_CFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o m4sh/m4sh.elf $OBJS $PCC_RUNTIME
echo "   M4SH ELF: m4sh/m4sh.elf ($(stat -c%s m4sh/m4sh.elf) bytes)"

echo "=== Building M4SHG (graphical-terminal shell) ==="
# Same sources, -DM4SH_GRAPHICAL redirects ser_* I/O onto pipe fd 0/1
# and skips the serial login gate.  Linked at 0x1000000 so the exec'd
# image never overlaps any live process (see m4sh/m4shg.ld).
OBJS_G=""
for f in $(find m4sh usr/src/cmd -name '*.c' -type f ! -path 'm4sh/login/*' ! -path 'usr/src/cmd/mdm.c' ! -path 'usr/src/cmd/mdm_mini.c' ! -path 'usr/src/cmd/flip_test.c' ! -path 'usr/src/cmd/copland.c' ! -path 'usr/src/cmd/cptest.c' ! -path 'usr/src/cmd/terminal.c' ! -path 'usr/src/cmd/fm.c' ! -path 'usr/src/cmd/altr.c' ! -path 'usr/src/cmd/calc_gui.c' ! -path 'usr/src/lib/*' | sort); do
    o="${f%.c}.g.o"
    $UCC $M4SH_CFLAGS -DM4SH_GRAPHICAL -c "$f" -o "$o"
    OBJS_G="$OBJS_G $o"
done
$LD -m elf_i386 -T m4sh/m4shg.ld -nostdlib -z max-page-size=0x1000 -o m4sh/m4shg.elf $OBJS_G $PCC_RUNTIME
echo "   M4SHG ELF: m4sh/m4shg.elf ($(stat -c%s m4sh/m4shg.elf) bytes)"
find m4sh usr/src/cmd -name '*.g.o' -type f -delete

echo "=== Building M4SHT (serial test shell, full-test builds) ==="
# Same sources as m4sh, but linked at 0xF00000: MDM itself occupies
# 0x800000, so spawning a second m4sh there overwrites live MDM code
# (observed: EIP in set_env mid-instruction -> GPF).  This variant is
# spawned by MDM under M4K_TEST_AUTOLOGIN for headless shell tests.
OBJS_T=""
for f in $(find m4sh usr/src/cmd -name '*.c' -type f ! -path 'm4sh/login/*' ! -path 'usr/src/cmd/mdm.c' ! -path 'usr/src/cmd/mdm_mini.c' ! -path 'usr/src/cmd/flip_test.c' ! -path 'usr/src/cmd/copland.c' ! -path 'usr/src/cmd/cptest.c' ! -path 'usr/src/cmd/terminal.c' ! -path 'usr/src/cmd/fm.c' ! -path 'usr/src/cmd/altr.c' ! -path 'usr/src/cmd/calc_gui.c' ! -path 'usr/src/lib/*' | sort); do
    o="${f%.c}.t.o"
    $UCC $M4SH_CFLAGS -DM4SH_NO_LOGIN_GATE -c "$f" -o "$o"
    OBJS_T="$OBJS_T $o"
done
$LD -m elf_i386 -T m4sh/m4sht.ld -nostdlib -z max-page-size=0x1000 -o m4sh/m4sht.elf $OBJS_T $PCC_RUNTIME
echo "   M4SHT ELF: m4sh/m4sht.elf ($(stat -c%s m4sh/m4sht.elf) bytes)"
find m4sh usr/src/cmd -name '*.t.o' -type f -delete

echo "=== Building standalone login ELF ==="
$UCC $M4SH_CFLAGS -c m4sh/login/login_standalone.c -o m4sh/login/login_standalone.o
$UCC $M4SH_CFLAGS -c m4sh/lib/pwd.c -o m4sh/lib/pwd_login.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o m4sh/login/login.elf m4sh/login/login_standalone.o m4sh/lib/pwd_login.o $PCC_RUNTIME
echo "   Login ELF: m4sh/login/login.elf ($(stat -c%s m4sh/login/login.elf) bytes)"

if [ "$NEED_RECOVERY" = 1 ]; then
echo "=== Building Recovery tools (fsck, reset-passwd) ELFs ==="
if [ -d usr/src/tools/recovery ] && [ -f usr/src/tools/recovery/fsck.c ]; then
    $UCC $M4SH_CFLAGS -c usr/src/tools/recovery/fsck.c -o usr/src/tools/recovery/fsck.o
    $UCC $M4SH_CFLAGS -c m4sh/lib/pwd.c -o m4sh/lib/pwd_fsck.o
    $LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 \
        -o usr/src/tools/recovery/fsck.elf \
        usr/src/tools/recovery/fsck.o m4sh/lib/pwd_fsck.o $PCC_RUNTIME
    echo "   fsck ELF: usr/src/tools/recovery/fsck.elf ($(stat -c%s usr/src/tools/recovery/fsck.elf) bytes)"
else
    echo "   WARNING: usr/src/tools/recovery/fsck.c not found, /bin/fsck will not be built"
fi
if [ -d usr/src/tools/recovery ] && [ -f usr/src/tools/recovery/reset-passwd.c ]; then
    $UCC $M4SH_CFLAGS -c usr/src/tools/recovery/reset-passwd.c -o usr/src/tools/recovery/reset-passwd.o
    $UCC $M4SH_CFLAGS -c m4sh/lib/pwd.c -o m4sh/lib/pwd_reset.o
    $LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 \
        -o usr/src/tools/recovery/reset-passwd.elf \
        usr/src/tools/recovery/reset-passwd.o m4sh/lib/pwd_reset.o $PCC_RUNTIME
    echo "   reset-passwd ELF: usr/src/tools/recovery/reset-passwd.elf ($(stat -c%s usr/src/tools/recovery/reset-passwd.elf) bytes)"
else
    echo "   WARNING: usr/src/tools/recovery/reset-passwd.c not found, /bin/reset-passwd will not be built"
fi
fi

if [ "$NEED_GRAPHICS" = 1 ]; then
$UCC $M4SH_CFLAGS -c usr/src/lib/libgui.c -o usr/src/lib/libgui.o
$UCC $M4SH_CFLAGS -c usr/src/cmd/mdm.c -o usr/src/cmd/mdm.o
$UCC $M4SH_CFLAGS -c m4sh/lib/pwd.c -o m4sh/lib/pwd_mdm.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/mdm.elf usr/src/cmd/mdm.o usr/src/lib/libgui.o m4sh/lib/pwd_mdm.o $PCC_RUNTIME
echo "   MDM ELF: usr/src/cmd/mdm.elf ($(stat -c%s usr/src/cmd/mdm.elf) bytes)"

echo "=== Building MDM Mini (Test) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/mdm_mini.c -o usr/src/cmd/mdm_mini.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/mdm_mini.elf usr/src/cmd/mdm_mini.o usr/src/lib/libgui.o $PCC_RUNTIME
echo "   MDM Mini ELF: usr/src/cmd/mdm_mini.elf ($(stat -c%s usr/src/cmd/mdm_mini.elf) bytes)"

echo "=== Building Flip Test ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/flip_test.c -o usr/src/cmd/flip_test.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/flip_test.elf usr/src/cmd/flip_test.o $PCC_RUNTIME
echo "   Flip Test ELF: usr/src/cmd/flip_test.elf ($(stat -c%s usr/src/cmd/flip_test.elf) bytes)"

echo "=== Building Copland (display server) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/lib/libgui.c -o usr/src/lib/libgui.o
$UCC $M4SH_CFLAGS -c sys/src/copland/copland_proto.c -o sys/src/copland/copland_proto.o
$UCC $M4SH_CFLAGS -c sys/src/copland/compositor.c -o sys/src/copland/compositor.o
$UCC $M4SH_CFLAGS -c usr/src/cmd/copland.c -o usr/src/cmd/copland.o
$LD -m elf_i386 -T usr/src/cmd/copland.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/copland.elf usr/src/cmd/copland.o sys/src/copland/compositor.o sys/src/copland/copland_proto.o usr/src/lib/libgui.o $PCC_RUNTIME
echo "   Copland ELF: usr/src/cmd/copland.elf ($(stat -c%s usr/src/cmd/copland.elf) bytes)"

echo "=== Building CPTEST (copland protocol smoke client) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/cptest.c -o usr/src/cmd/cptest.o
$LD -m elf_i386 -T usr/src/cmd/cptest.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/cptest.elf usr/src/cmd/cptest.o sys/src/copland/copland_proto.o usr/src/lib/libgui.o $PCC_RUNTIME
echo "   CPTEST ELF: usr/src/cmd/cptest.elf ($(stat -c%s usr/src/cmd/cptest.elf) bytes)"

echo "=== Building Terminal (graphical terminal emulator) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/terminal.c -o usr/src/cmd/terminal.o
$LD -m elf_i386 -T usr/src/cmd/terminal.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/terminal.elf usr/src/cmd/terminal.o $PCC_RUNTIME
echo "   Terminal ELF: usr/src/cmd/terminal.elf ($(stat -c%s usr/src/cmd/terminal.elf) bytes)"

echo "=== Building File Manager (tabbed graphical FM) ELF ==="
# icons.c: shared pixel-icon library, linked into the standalone
# GUI clients (fm) and the WM (sprach) below.
# icons_data.c: PNG-converted 32x32 RGBA bitmaps (generated by
# tools/build/gen_icons.py from the icon asset tree).
$UCC $M4SH_CFLAGS -c usr/src/lib/icons.c -o usr/src/lib/icons.o
$UCC $M4SH_CFLAGS -c usr/src/lib/icons_data.c -o usr/src/lib/icons_data.o

$UCC $M4SH_CFLAGS -c usr/src/cmd/fm.c -o usr/src/cmd/fm.o
$LD -m elf_i386 -T usr/src/cmd/fm.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/fm.elf usr/src/cmd/fm.o usr/src/lib/icons.o usr/src/lib/icons_data.o $PCC_RUNTIME
echo "   File Manager ELF: usr/src/cmd/fm.elf ($(stat -c%s usr/src/cmd/fm.elf) bytes)"

echo "=== Building ALTR (text editor) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/altr.c -o usr/src/cmd/altr.o
$LD -m elf_i386 -T usr/src/cmd/altr.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/altr.elf usr/src/cmd/altr.o $PCC_RUNTIME
echo "   ALTR ELF: usr/src/cmd/altr.elf ($(stat -c%s usr/src/cmd/altr.elf) bytes)"

echo "=== Building CALC GUI (calculator) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/cmd/calc_gui.c -o usr/src/cmd/calc_gui.o
$LD -m elf_i386 -T usr/src/cmd/calc_gui.ld -nostdlib -z max-page-size=0x1000 -o usr/src/cmd/calc_gui.elf usr/src/cmd/calc_gui.o $PCC_RUNTIME
echo "   CALC GUI ELF: usr/src/cmd/calc_gui.elf ($(stat -c%s usr/src/cmd/calc_gui.elf) bytes)"

echo "=== Building Sprach (window manager) ELFs ==="
# Zig hot-path pixel primitives (docs/zig-standard.md): i386
# freestanding object linked beside the C objects; falls back to
# pure-C primitives when the toolchain is absent.
ZIG_BIN="${ZIG_BIN:-$HOME/zig-0.13.0/zig}"
SPR_DRAW_OBJS=""
if [ -x "$ZIG_BIN" ]; then
    "$ZIG_BIN" build-obj usr/src/sprach/spr_draw.zig \
        -target x86-freestanding -mcpu "baseline-sse-sse2" -O ReleaseFast \
        -femit-bin=usr/src/sprach/spr_draw.o \
        && nm -u usr/src/sprach/spr_draw.o | grep -q . \
        && { echo "   ERROR: spr_draw.o has undefined symbols (missing builtin lib)"; nm -u usr/src/sprach/spr_draw.o; SPR_DRAW_OBJS=""; } \
        || true
    if [ -f usr/src/sprach/spr_draw.o ] && [ -z "$SPR_DRAW_OBJS" ]; then
        if nm -u usr/src/sprach/spr_draw.o | grep -q .; then
            rm -f usr/src/sprach/spr_draw.o
        else
            SPR_DRAW_OBJS="usr/src/sprach/spr_draw.o"
            echo "   spr_draw.zig -> spr_draw.o (Zig hot path)"
        fi
    fi
else
    echo "   WARN: zig not found at $ZIG_BIN; using C primitives"
fi
if [ -n "$SPR_DRAW_OBJS" ]; then
    UCC_FLAGS_ZIG="-DUSE_ZIG_DRAW"
else
    UCC_FLAGS_ZIG=""
fi
$UCC $M4SH_CFLAGS $UCC_FLAGS_ZIG -c usr/src/sprach/sprach.c -o usr/src/sprach/sprach.o
for m in stack; do
    $UCC $M4SH_CFLAGS -c "usr/src/sprach/sprach_mode_${m}.c" -o "usr/src/sprach/sprach_mode_${m}.o"
    $UCC $M4SH_CFLAGS -c sys/src/copland/copland_proto.c -o usr/src/sprach/copland_proto.o
    $LD -m elf_i386 -T usr/src/sprach/sprach.ld -nostdlib -z max-page-size=0x1000 \
        -o "usr/src/sprach/sprach_${m}" \
        "usr/src/sprach/sprach.o" "usr/src/sprach/sprach_mode_${m}.o" usr/src/lib/icons.o usr/src/lib/icons_data.o usr/src/lib/libgui.o usr/src/sprach/copland_proto.o $SPR_DRAW_OBJS $PCC_RUNTIME
    echo "   Sprach ${m} ELF: usr/src/sprach/sprach_${m} ($(stat -c%s "usr/src/sprach/sprach_${m}") bytes)"
done
cp -f usr/src/sprach/sprach_stack ./usr/bin/sprach

echo "=== Building PCC (self-hosted C compiler) ELF ==="
$UCC $M4SH_CFLAGS -c usr/src/tools/pcc/pcc.c -o usr/src/tools/pcc/pcc.o
$UCC $M4SH_CFLAGS -c usr/src/tools/pcc/pcc_shims.c -o usr/src/tools/pcc/pcc_shims.o
$UCC $M4SH_CFLAGS -c usr/src/lib/m4k_libc/stdio.c -o usr/src/lib/m4k_libc/stdio_pcc.o
$UCC $M4SH_CFLAGS -c usr/src/lib/m4k_libc/string.c -o usr/src/lib/m4k_libc/string_pcc.o
$UCC $M4SH_CFLAGS -c usr/src/lib/m4k_libc/stdlib.c -o usr/src/lib/m4k_libc/stdlib_pcc.o
$UCC $M4SH_CFLAGS -c usr/src/lib/m4k_libc/unistd.c -o usr/src/lib/m4k_libc/unistd_pcc.o
$UCC $M4SH_CFLAGS -c usr/src/lib/m4k_libc/errno.c -o usr/src/lib/m4k_libc/errno_pcc.o
$LD -m elf_i386 -T usr/src/tools/pcc/pcc.ld -nostdlib -z max-page-size=0x1000 \
    -o usr/src/tools/pcc/pcc.elf usr/src/lib/m4k_libc/crt0.o usr/src/tools/pcc/pcc.o \
    usr/src/tools/pcc/pcc_shims.o \
    usr/src/lib/m4k_libc/stdio_pcc.o usr/src/lib/m4k_libc/string_pcc.o \
    usr/src/lib/m4k_libc/stdlib_pcc.o usr/src/lib/m4k_libc/unistd_pcc.o \
    usr/src/lib/m4k_libc/errno_pcc.o $PCC_RUNTIME
echo "   PCC ELF: usr/src/tools/pcc/pcc.elf ($(stat -c%s usr/src/tools/pcc/pcc.elf) bytes)"
fi

echo "=== Building init ELF ==="
$UCC $M4SH_CFLAGS -c init/init.c -o init/init.o
$UCC $M4SH_CFLAGS -c m4sh/lib/pwd.c -o m4sh/lib/pwd_init.o
$LD -m elf_i386 -T m4sh/m4sh.ld -nostdlib -z max-page-size=0x1000 -o init/init.elf init/init.o m4sh/lib/pwd_init.o $PCC_RUNTIME
echo "   Init ELF: init/init.elf ($(stat -c%s init/init.elf) bytes)"
fi

echo "=== Generating ELF data arrays ==="
if [ "$NEED_USER" = 1 ]; then
xxd -i m4sh/m4sh.elf > init/m4sh_elf_.c
sed 's/m4sh_m4sh_elf/m4sh_init_elf/g; s/m4sh_m4sh_elf_len/m4sh_init_elf_len/g' init/m4sh_elf_.c > init/m4sh_elf.c
rm -f init/m4sh_elf_.c
echo "   Generated init/m4sh_elf.c"

xxd -i m4sh/m4shg.elf > init/m4shg_elf_.c
sed 's/m4sh_m4shg_elf/m4shg_init_elf/g; s/m4sh_m4shg_elf_len/m4shg_init_elf_len/g' init/m4shg_elf_.c > init/m4shg_elf.c
rm -f init/m4shg_elf_.c
echo "   Generated init/m4shg_elf.c"

if [ "$BUILD_MODE" = "full-test" ] && [ -f m4sh/m4sht.elf ]; then
xxd -i m4sh/m4sht.elf > init/m4sht_elf_.c
sed 's/m4sh_m4sht_elf/m4sht_init_elf/g; s/m4sh_m4sht_elf_len/m4sht_init_elf_len/g' init/m4sht_elf_.c > init/m4sht_elf.c
rm -f init/m4sht_elf_.c
echo "   Generated init/m4sht_elf.c"
fi

xxd -i m4sh/login/login.elf > init/login_elf_.c
sed 's/m4sh_login_login_elf/login_init_elf/g; s/m4sh_login_login_elf_len/login_init_elf_len/g' init/login_elf_.c > init/login_elf.c
rm -f init/login_elf_.c
echo "   Generated init/login_elf.c"
fi

if [ "$NEED_GRAPHICS" = 1 ]; then
xxd -i usr/src/cmd/mdm.elf > init/mdm_elf_.c
sed 's/usr_src_cmd_mdm_elf/mdm_init_elf/g; s/usr_src_cmd_mdm_elf_len/mdm_init_elf_len/g' init/mdm_elf_.c > init/mdm_elf.c
rm -f init/mdm_elf_.c
echo "   Generated init/mdm_elf.c"

xxd -i usr/src/cmd/mdm_mini.elf > init/mdm_mini_elf_.c
sed 's/usr_src_cmd_mdm_mini_elf/mdm_mini_init_elf/g; s/usr_src_cmd_mdm_mini_elf_len/mdm_mini_init_elf_len/g' init/mdm_mini_elf_.c > init/mdm_mini_elf.c
rm -f init/mdm_mini_elf_.c
echo "   Generated init/mdm_mini_elf.c"

xxd -i usr/src/cmd/flip_test.elf > init/flip_test_elf_.c
sed 's/usr_src_cmd_flip_test_elf/flip_test_init_elf/g; s/usr_src_cmd_flip_test_elf_len/flip_test_init_elf_len/g' init/flip_test_elf_.c > init/flip_test_elf.c
rm -f init/flip_test_elf_.c
echo "   Generated init/flip_test_elf.c"

xxd -i usr/src/cmd/copland.elf > init/copland_elf_.c
sed 's/usr_src_cmd_copland_elf/copland_init_elf/g; s/usr_src_cmd_copland_elf_len/copland_init_elf_len/g' init/copland_elf_.c > init/copland_elf.c
rm -f init/copland_elf_.c
echo "   Generated init/copland_elf.c"

xxd -i usr/src/cmd/terminal.elf > init/terminal_elf_.c
sed 's/usr_src_cmd_terminal_elf/terminal_init_elf/g; s/usr_src_cmd_terminal_elf_len/terminal_init_elf_len/g' init/terminal_elf_.c > init/terminal_elf.c
rm -f init/terminal_elf_.c
echo "   Generated init/terminal_elf.c"

xxd -i usr/src/cmd/cptest.elf > init/cptest_elf_.c
sed 's/usr_src_cmd_cptest_elf/cptest_init_elf/g; s/usr_src_cmd_cptest_elf_len/cptest_init_elf_len/g' init/cptest_elf_.c > init/cptest_elf.c
rm -f init/cptest_elf_.c
echo "   Generated init/cptest_elf.c"

xxd -i usr/src/cmd/fm.elf > init/fm_elf_.c
sed 's/usr_src_cmd_fm_elf/fm_init_elf/g; s/usr_src_cmd_fm_elf_len/fm_init_elf_len/g' init/fm_elf_.c > init/fm_elf.c
rm -f init/fm_elf_.c
echo "   Generated init/fm_elf.c"

xxd -i usr/src/cmd/altr.elf > init/altr_elf_.c
sed 's/usr_src_cmd_altr_elf/altr_init_elf/g; s/usr_src_cmd_altr_elf_len/altr_init_elf_len/g' init/altr_elf_.c > init/altr_elf.c
rm -f init/altr_elf_.c
echo "   Generated init/altr_elf.c"

xxd -i usr/src/cmd/calc_gui.elf > init/calc_elf_.c
sed 's/usr_src_cmd_calc_gui_elf/calc_init_elf/g; s/usr_src_cmd_calc_gui_elf_len/calc_init_elf_len/g' init/calc_elf_.c > init/calc_elf.c
rm -f init/calc_elf_.c
echo "   Generated init/calc_elf.c"

for m in stack; do
    xxd -i "usr/src/sprach/sprach_${m}" > "init/sprach_${m}_elf_.c"
    sed "s/usr_src_sprach_sprach_${m}/sprach_${m}_init_elf/g; s/usr_src_sprach_sprach_${m}_len/sprach_${m}_init_elf_len/g" "init/sprach_${m}_elf_.c" > "init/sprach_${m}_elf.c"
    rm -f "init/sprach_${m}_elf_.c"
    echo "   Generated init/sprach_${m}_elf.c"
done

xxd -i usr/src/tools/pcc/pcc.elf > init/pcc_elf_.c
sed 's/usr_src_tools_pcc_pcc_elf/pcc_init_elf/g' init/pcc_elf_.c > init/pcc_elf.c
rm -f init/pcc_elf_.c
echo "   Generated init/pcc_elf.c"
fi

if [ "$NEED_RECOVERY" = 1 ]; then
if [ -f usr/src/tools/recovery/fsck.elf ]; then
    xxd -i usr/src/tools/recovery/fsck.elf > init/fsck_elf_.c
    sed 's/usr_src_tools_recovery_fsck_elf/fsck_init_elf/g; s/usr_src_tools_recovery_fsck_elf_len/fsck_init_elf_len/g' init/fsck_elf_.c > init/fsck_elf.c
    rm -f init/fsck_elf_.c
    echo "   Generated init/fsck_elf.c"
fi
if [ -f usr/src/tools/recovery/reset-passwd.elf ]; then
    xxd -i usr/src/tools/recovery/reset-passwd.elf > init/reset_passwd_elf_.c
    sed 's/usr_src_tools_recovery_reset_passwd_elf/reset_passwd_init_elf/g; s/usr_src_tools_recovery_reset_passwd_elf_len/reset_passwd_init_elf_len/g' init/reset_passwd_elf_.c > init/reset_passwd_elf.c
    rm -f init/reset_passwd_elf_.c
    echo "   Generated init/reset_passwd_elf.c"
fi
fi

if [ "$NEED_USER" = 1 ]; then
xxd -i init/init.elf > init/init_elf.c
echo "   Generated init/init_elf.c"
fi

echo "=== Flat-address-space ELF overlap check ==="
python3 tools/build/check_elf_overlap.py || exit 1

echo "=== Copying ELFs to ./usr/bin/ ==="
if [ "$NEED_USER" = 1 ]; then
cp -f m4sh/m4sh.elf ./usr/bin/m4sh
cp -f m4sh/login/login.elf ./usr/bin/login
cp -f init/init.elf ./usr/bin/init
if [ -f m4sh/m4sht.elf ]; then
    cp -f m4sh/m4sht.elf ./usr/bin/m4sht
fi
fi
if [ "$NEED_GRAPHICS" = 1 ]; then
cp -f usr/src/cmd/mdm.elf ./usr/bin/mdm
cp -f usr/src/cmd/flip_test.elf ./usr/bin/flip_test
cp -f usr/src/cmd/copland.elf ./usr/bin/copland
cp -f usr/src/cmd/copland.elf ./usr/bin/copland_status
cp -f usr/src/cmd/terminal.elf ./usr/bin/terminal
cp -f usr/src/cmd/fm.elf ./usr/bin/fm
cp -f usr/src/cmd/altr.elf ./usr/bin/altr
cp -f usr/src/cmd/calc_gui.elf ./usr/bin/calcg
cp -f usr/src/sprach/sprach_stack ./usr/bin/sprach_stack
cp -f usr/src/tools/pcc/pcc.elf ./usr/bin/pcc
cp -f usr/src/tools/pcc/pcc.elf ./usr/bin/cc
fi
if [ "$NEED_RECOVERY" = 1 ]; then
if [ -f usr/src/tools/recovery/fsck.elf ]; then
    cp -f usr/src/tools/recovery/fsck.elf ./usr/bin/fsck
fi
if [ -f usr/src/tools/recovery/reset-passwd.elf ]; then
    cp -f usr/src/tools/recovery/reset-passwd.elf ./usr/bin/reset-passwd
fi
fi
for f in usr/src/cmd/*.elf; do
    [ -f "$f" ] && cp -f "$f" ./usr/bin/ || true
done
echo "   Copied $(ls ./usr/bin/ | wc -l) ELF(s) to ./usr/bin/"

echo "=== Compiling ELF data arrays ==="
if [ "$NEED_USER" = 1 ]; then
$KCC $CFLAGS -c init/init_elf.c -o $OBJDIR/init_elf.o
$KCC $CFLAGS -c init/m4sh_elf.c -o $OBJDIR/m4sh_elf.o
$KCC $CFLAGS -c init/m4shg_elf.c -o $OBJDIR/m4shg_elf.o
if [ "$BUILD_MODE" = "full-test" ] && [ -f init/m4sht_elf.c ]; then
    $KCC $CFLAGS -c init/m4sht_elf.c -o $OBJDIR/m4sht_elf.o
fi
$KCC $CFLAGS -c init/login_elf.c -o $OBJDIR/login_elf.o
fi
if [ "$NEED_GRAPHICS" = 1 ]; then
$KCC $CFLAGS -c init/mdm_elf.c -o $OBJDIR/mdm_elf.o
$KCC $CFLAGS -c init/mdm_mini_elf.c -o $OBJDIR/mdm_mini_elf.o
$KCC $CFLAGS -c init/flip_test_elf.c -o $OBJDIR/flip_test_elf.o
$KCC $CFLAGS -c init/copland_elf.c -o $OBJDIR/copland_elf.o
$KCC $CFLAGS -c init/terminal_elf.c -o $OBJDIR/terminal_elf.o
$KCC $CFLAGS -c init/cptest_elf.c -o $OBJDIR/cptest_elf.o
$KCC $CFLAGS -c init/fm_elf.c -o $OBJDIR/fm_elf.o
$KCC $CFLAGS -c init/altr_elf.c -o $OBJDIR/altr_elf.o
$KCC $CFLAGS -c init/calc_elf.c -o $OBJDIR/calc_elf.o
$KCC $CFLAGS -c init/sprach_stack_elf.c -o $OBJDIR/sprach_stack_elf.o
$KCC $CFLAGS -c init/pcc_elf.c -o $OBJDIR/pcc_elf.o
fi
$KCC $CFLAGS -c init/man_pages.c -o $OBJDIR/man_pages.o
if [ "$NEED_RECOVERY" = 1 ]; then
if [ -f init/fsck_elf.c ]; then
    $KCC $CFLAGS -c init/fsck_elf.c -o $OBJDIR/fsck_elf.o
fi
if [ -f init/reset_passwd_elf.c ]; then
    $KCC $CFLAGS -c init/reset_passwd_elf.c -o $OBJDIR/reset_passwd_elf.o
fi
fi

echo "=== Compiling VFS ==="
$KCC $CFLAGS -c sys/src/fs/vfs.c -o $OBJDIR/vfs.o

echo "=== Compiling YAFS filesystem ==="
YAFS_CFLAGS="$CFLAGS -I$PWD/sys/src/fs/yafs/include"
$KCC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/btree.c -o $OBJDIR/btree.o
$KCC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/yafs_test.c -o $OBJDIR/yafs_test.o
$KCC $YAFS_CFLAGS -c sys/src/fs/yafs/core/yafs/yafs_vfs.c -o $OBJDIR/yafs_vfs.o

echo "=== Compiling lib files ==="
$KCC $CFLAGS -c sys/src/lib/string.c -o $OBJDIR/string.o
$KCC $CFLAGS -c sys/src/lib/debug.c -o $OBJDIR/debug.o
$KCC $CFLAGS -c sys/src/lib/kstrtox.c -o $OBJDIR/kstrtox.o

echo "=== Linking kernel ==="
KRN_OBJS="$OBJDIR/entry.o \
    $OBJDIR/kmain.o \
    $OBJDIR/process.o \
    $OBJDIR/execve.o \
    $OBJDIR/console.o \
    $OBJDIR/mouse.o \
    $OBJDIR/keyboard.o \
    $OBJDIR/pci.o \
    $OBJDIR/ata.o \
    $OBJDIR/sb16.o \
    $OBJDIR/e1000.o \
    $OBJDIR/net.o \
    $OBJDIR/arp.o \
    $OBJDIR/tcp.o \
    $OBJDIR/syscall_net.o \
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
    $OBJDIR/btree.o \
    $OBJDIR/yafs_test.o \
    $OBJDIR/yafs_vfs.o \
    $OBJDIR/vfs.o \
    $OBJDIR/signal.o \
    $OBJDIR/namespace.o \
    $OBJDIR/mkrn_syscall.o \
    $OBJDIR/session.o \
    $OBJDIR/mkrn_vfs_perms.o \
    $OBJDIR/mkrn_procfs.o \
    $OBJDIR/mkrn_devfs.o \
    $OBJDIR/mkrn_sessions.o \
    $OBJDIR/mkrn_device_tree.o \
    $OBJDIR/string.o \
    $OBJDIR/kstrtox.o \
    $OBJDIR/debug.o"
if [ "$NEED_GRAPHICS" = 1 ]; then
    KRN_OBJS="$KRN_OBJS $OBJDIR/vesa.o \
    $OBJDIR/mdm_elf.o \
    $OBJDIR/mdm_mini_elf.o \
    $OBJDIR/flip_test_elf.o \
    $OBJDIR/copland_elf.o \
    $OBJDIR/terminal_elf.o \
    $OBJDIR/cptest_elf.o \
    $OBJDIR/altr_elf.o \
    $OBJDIR/calc_elf.o \
    $OBJDIR/fm_elf.o \
    $OBJDIR/sprach_stack_elf.o \
    $OBJDIR/pcc_elf.o"
fi
if [ "$NEED_USER" = 1 ]; then
    KRN_OBJS="$KRN_OBJS $OBJDIR/init_elf.o \
    $OBJDIR/m4sh_elf.o \
    $OBJDIR/m4shg_elf.o \
    $OBJDIR/login_elf.o \
    $OBJDIR/man_pages.o"
    if [ "$BUILD_MODE" = "full-test" ] && [ -f $OBJDIR/m4sht_elf.o ]; then
        KRN_OBJS="$KRN_OBJS $OBJDIR/m4sht_elf.o"
    fi
fi
if [ "$NEED_RECOVERY" = 1 ]; then
    if [ -f $OBJDIR/fsck_elf.o ]; then
        KRN_OBJS="$KRN_OBJS $OBJDIR/fsck_elf.o"
    fi
    if [ -f $OBJDIR/reset_passwd_elf.o ]; then
        KRN_OBJS="$KRN_OBJS $OBJDIR/reset_passwd_elf.o"
    fi
fi
 $LD $LDFLAGS -o "$OUTPUT_DIR/m4kk1.krn" $KRN_OBJS

 echo "=== Kernel built: $OUTPUT_DIR/m4kk1.krn ($(stat -c%s "$OUTPUT_DIR/m4kk1.krn") bytes) ==="

echo "=== Building ISO image ==="
ISODIR=$(mktemp -d /tmp/m4kk1_isodir.XXXXXX)
mkdir -p "$ISODIR/boot/grub" "$ISODIR/boot/kernel"
cp "$OUTPUT_DIR/m4kk1.krn" "$ISODIR/boot/kernel/m4kk1.krn"
cat > "$ISODIR/boot/grub/grub.cfg" << GRUB
set timeout=0
set default=0
menuentry "M4KK1" {
    multiboot /boot/kernel/m4kk1.krn
    boot
}
GRUB
 rm -f "$OUTPUT_DIR/$ISO_NAME"
 if grub-mkrescue -o "$OUTPUT_DIR/$ISO_NAME" "$ISODIR" 2>/tmp/m4kk1_grub_err.txt; then
     echo "=== ISO built: $OUTPUT_DIR/$ISO_NAME ($(stat -c%s "$OUTPUT_DIR/$ISO_NAME") bytes) ==="
 else
     echo "WARNING: ISO build failed (grub-mkrescue needs mtools/mformat)."
     echo "         Kernel is still available: $OUTPUT_DIR/m4kk1.krn"
     echo "         Boot it directly: qemu-system-i386 -kernel $OUTPUT_DIR/m4kk1.krn -serial stdio"
     cat /tmp/m4kk1_grub_err.txt >&2
 fi
rm -rf "$ISODIR" /tmp/m4kk1_grub_err.txt
