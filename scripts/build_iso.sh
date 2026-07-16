#!/bin/bash
# M4KK1 4P1 - build_iso.sh
# Description: ISO image build script for M4KK1 OS.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Build configuration
ISO_DIR="$PROJECT_ROOT/iso"
BUILD_DIR="$PROJECT_ROOT/build"
SYSROOT_DIR="$BUILD_DIR/sysroot"
KERNEL_DIR="$BUILD_DIR/kernel"

# Toolchain
MKISOFS="genisoimage"
GRUB_MKRESCUE="grub-mkrescue"

# ISO filename
ISO_NAME="m4kk1-$(date +%Y%m%d).iso"

echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}    M4KK1 ISO Image Builder${NC}"
echo -e "${BLUE}=======================================${NC}"
echo

# Check dependencies
check_dependencies() {
    echo -e "${YELLOW}Checking build dependencies...${NC}"

    if ! command -v nasm &> /dev/null; then
        echo -e "${RED}Error: nasm assembler not found${NC}"
        exit 1
    fi

    if ! command -v gcc &> /dev/null; then
        echo -e "${RED}Error: gcc compiler not found${NC}"
        exit 1
    fi

    if ! command -v $MKISOFS &> /dev/null; then
        echo -e "${RED}Error: genisoimage tool not found${NC}"
        exit 1
    fi

    if ! command -v $GRUB_MKRESCUE &> /dev/null; then
        echo -e "${RED}Error: grub-mkrescue tool not found${NC}"
        exit 1
    fi

    echo -e "${GREEN}All dependencies passed${NC}"
}

# Clean old build files
clean_build() {
    echo -e "${YELLOW}Cleaning old build files...${NC}"

    rm -rf "$BUILD_DIR"
    rm -rf "$ISO_DIR/boot/kernel"
    rm -f "$ISO_DIR/$ISO_NAME"

    echo -e "${GREEN}Cleanup completed${NC}"
}

# Create directory structure
create_directories() {
    echo -e "${YELLOW}Creating directory structure...${NC}"

    mkdir -p "$SYSROOT_DIR"
    mkdir -p "$KERNEL_DIR"
    mkdir -p "$ISO_DIR/boot/grub"
    mkdir -p "$ISO_DIR/boot/kernel"

    echo -e "${GREEN}Directory structure created${NC}"
}

# Build bootloader
build_bootloader() {
    echo -e "${YELLOW}Building bootloader...${NC}"

    cd "$PROJECT_ROOT/sys/boot/bootcamp"

    make clean
    make all

    cp m4kk1.img "$ISO_DIR/boot/"

    echo -e "${GREEN}Bootloader build completed${NC}"
}

# Build kernel
build_kernel() {
    echo -e "${YELLOW}Building kernel...${NC}"

    cd "$PROJECT_ROOT/sys/src"

    make clean
    make all

    find . -name "*.kernel" -exec cp {} "$ISO_DIR/boot/kernel/" \;

    echo -e "${GREEN}Kernel build completed${NC}"
}

# Build user programs
build_userland() {
    echo -e "${YELLOW}Building user programs...${NC}"

    cd "$PROJECT_ROOT/usr/bin/copland"
    make clean
    make all
    cp copland "$SYSROOT_DIR/usr/bin/"

    for dir in "$PROJECT_ROOT/usr/bin"/*; do
        if [ -d "$dir" ] && [ "$dir" != "$PROJECT_ROOT/usr/bin/copland" ]; then
            echo -e "${BLUE}Building $(basename "$dir")...${NC}"
            cd "$dir"
            if [ -f "Makefile" ]; then
                make clean
                make all
                find . -type f -executable -exec cp {} "$SYSROOT_DIR/usr/bin/" \; 2>/dev/null || true
            fi
            cd "$PROJECT_ROOT"
        fi
    done

    echo -e "${GREEN}User programs build completed${NC}"
}

# Create initrd image
create_initrd() {
    echo -e "${YELLOW}Creating initrd image...${NC}"

    INITRD_DIR="$BUILD_DIR/initrd"
    mkdir -p "$INITRD_DIR"

    cp -r "$SYSROOT_DIR"/* "$INITRD_DIR/"

    cd "$INITRD_DIR"
    find . | cpio -o -H newc > "$ISO_DIR/boot/initrd.img"

    echo -e "${GREEN}initrd image created${NC}"
}

# Create GRUB configuration
create_grub_config() {
    echo -e "${YELLOW}Creating GRUB configuration...${NC}"

    cat > "$ISO_DIR/boot/grub/grub.cfg" << EOF
# M4KK1 GRUB configuration
set timeout=5
set default=0

menuentry "M4KK1 Operating System" {
    multiboot /boot/kernel/m4kk1.kernel
    boot
}

menuentry "M4KK1 with initrd" {
    multiboot /boot/kernel/m4kk1.kernel
    module /boot/initrd.img
    boot
}

menuentry "M4KK1 (Text Mode)" {
    multiboot /boot/kernel/m4kk1.kernel nomodeset
    boot
}
EOF

    echo -e "${GREEN}GRUB configuration created${NC}"
}

# Create ISO image
create_iso() {
    echo -e "${YELLOW}Creating ISO image...${NC}"

    cd "$ISO_DIR"

    $GRUB_MKRESCUE -o "$ISO_NAME" . 2>/dev/null || {
        echo -e "${YELLOW}Using genisoimage to create ISO...${NC}"
        $MKISOFS -R -b boot/grub/i386-pc/eltorito.img \
                 -no-emul-boot -boot-load-size 4 \
                 -boot-info-table -o "$ISO_NAME" . 2>/dev/null || {
            echo -e "${RED}Error: Could not create ISO image${NC}"
            exit 1
        }
    }

    echo -e "${GREEN}ISO image created: $ISO_NAME${NC}"
}

# Main build flow
main() {
    echo -e "${BLUE}Starting M4KK1 ISO image build...${NC}"

    check_dependencies
    clean_build
    create_directories
    build_bootloader
    build_kernel
    build_userland
    create_initrd
    create_grub_config
    create_iso

    echo
    echo -e "${GREEN}=======================================${NC}"
    echo -e "${GREEN}    M4KK1 ISO Image Build Complete!${NC}"
    echo -e "${GREEN}=======================================${NC}"
    echo
    echo -e "${BLUE}Image file: ${ISO_DIR}/${ISO_NAME}${NC}"
    echo -e "${BLUE}Size: $(du -h "${ISO_DIR}/${ISO_NAME}" | cut -f1)${NC}"
    echo
    echo -e "${YELLOW}Test command:${NC}"
    echo "  qemu-system-i386 -cdrom ${ISO_DIR}/${ISO_NAME}"
    echo
}

# Show help information
show_help() {
    echo "M4KK1 ISO image build script"
    echo
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  clean     - Clean build files"
    echo "  kernel    - Build kernel only"
    echo "  userland  - Build user programs only"
    echo "  help      - Show this help"
    echo
    echo "Examples:"
    echo "  $0              # Build full ISO"
    echo "  $0 clean        # Clean build files"
    echo "  $0 kernel       # Build kernel only"
}

# Handle command line arguments
case "${1:-}" in
    "clean")
        clean_build
        ;;
    "kernel")
        build_kernel
        ;;
    "userland")
        build_userland
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    "")
        main
        ;;
    *)
        echo -e "${RED}Unknown option: $1${NC}"
        echo
        show_help
        exit 1
        ;;
esac
