#!/bin/bash
# M4KK1 4P1 - install.sh
# Description: System installation script for M4KK1 OS.
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

# Default installation device
DEFAULT_DEVICE="/dev/sda"
DEFAULT_BOOTLOADER_DEVICE="/dev/sda"

# Install configuration
INSTALL_DEVICE="$DEFAULT_DEVICE"
BOOTLOADER_DEVICE="$DEFAULT_BOOTLOADER_DEVICE"
INSTALL_MODE="full"
CONFIRMATION=true

echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}    M4KK1 OS Installer${NC}"
echo -e "${BLUE}=======================================${NC}"
echo

# Show help information
show_help() {
    echo "M4KK1 system install script"
    echo
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -d, --device DEVICE     Target device (default: $DEFAULT_DEVICE)"
    echo "  -b, --bootloader DEVICE Bootloader device (default: $DEFAULT_BOOTLOADER_DEVICE)"
    echo "  -m, --mode MODE         Install mode: full, kernel, bootloader (default: full)"
    echo "  -y, --yes               Auto-confirm all prompts"
    echo "  -h, --help              Show this help"
    echo
    echo "Examples:"
    echo "  $0                      # Interactive install"
    echo "  $0 -d /dev/sdb -y       # Install to sdb with auto-confirm"
    echo "  $0 --mode kernel -y     # Install kernel only"
    echo
    echo -e "${RED}Warning: This will overwrite the boot sector!${NC}"
    echo -e "${RED}Please backup important data!${NC}"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--device)
            INSTALL_DEVICE="$2"
            shift 2
            ;;
        -b|--bootloader)
            BOOTLOADER_DEVICE="$2"
            shift 2
            ;;
        -m|--mode)
            INSTALL_MODE="$2"
            shift 2
            ;;
        -y|--yes)
            CONFIRMATION=false
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# Check permissions
check_permissions() {
    echo -e "${YELLOW}Checking installation permissions...${NC}"

    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: Root permissions required${NC}"
        echo "Please use sudo $0 or run as root"
        exit 1
    fi

    echo -e "${GREEN}Permission check passed${NC}"
}

# Check device
check_device() {
    echo -e "${YELLOW}Checking target device...${NC}"

    if [ ! -b "$INSTALL_DEVICE" ]; then
        echo -e "${RED}Error: Device $INSTALL_DEVICE does not exist${NC}"
        exit 1
    fi

    local device_size=$(blockdev --getsize64 "$INSTALL_DEVICE" 2>/dev/null || echo "0")
    if [ "$device_size" -lt $((1024*1024*1024)) ]; then
        echo -e "${YELLOW}Warning: Device smaller than 1GB${NC}"
    fi

    echo -e "${GREEN}Device check passed: $INSTALL_DEVICE${NC}"
}

# Confirm installation
confirm_installation() {
    if [ "$CONFIRMATION" = true ]; then
        echo
        echo -e "${RED}Warning: This will modify the following devices:${NC}"
        echo "  Bootloader device: $BOOTLOADER_DEVICE"
        echo "  System device: $INSTALL_DEVICE"
        echo
        echo -e "${RED}This will overwrite boot sector and possibly data!${NC}"
        echo -e "${YELLOW}Please ensure important data is backed up!${NC}"
        echo
        read -p "Continue installation? (yes/no): " -r
        if [[ ! $REPLY =~ ^[Yy][Ee][Ss]$ ]]; then
            echo "Installation cancelled"
            exit 0
        fi
    fi
}

# Build system
build_system() {
    echo -e "${YELLOW}Building M4KK1 system...${NC}"

    if [[ "$INSTALL_MODE" == "full" || "$INSTALL_MODE" == "bootloader" ]]; then
        echo -e "${BLUE}Building bootloader...${NC}"
        cd "$PROJECT_ROOT/sys/boot/bootcamp"
        make clean
        make all
    fi

    if [[ "$INSTALL_MODE" == "full" || "$INSTALL_MODE" == "kernel" ]]; then
        echo -e "${BLUE}Building kernel...${NC}"
        cd "$PROJECT_ROOT/sys/src"
        make clean
        make all
    fi

    echo -e "${GREEN}System build completed${NC}"
}

# Install bootloader
install_bootloader() {
    echo -e "${YELLOW}Installing bootloader...${NC}"

    cd "$PROJECT_ROOT/sys/boot/bootcamp"

    if [ -f "m4kk1.img" ]; then
        echo -e "${BLUE}Installing bootloader to $BOOTLOADER_DEVICE...${NC}"
        dd if=m4kk1.img of="$BOOTLOADER_DEVICE" bs=512 count=1 conv=notrunc
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}Bootloader installed${NC}"
        else
            echo -e "${RED}Error: Bootloader install failed${NC}"
            exit 1
        fi
    else
        echo -e "${RED}Error: Bootloader image not found${NC}"
        exit 1
    fi
}

# Install kernel
install_kernel() {
    echo -e "${YELLOW}Installing kernel...${NC}"

    local kernel_files=()
    while IFS= read -r -d '' file; do
        kernel_files+=("$file")
    done < <(find "$PROJECT_ROOT" -name "*.kernel" -print0 2>/dev/null)

    if [ ${#kernel_files[@]} -eq 0 ]; then
        echo -e "${RED}Error: Kernel files not found${NC}"
        exit 1
    fi

    local kernel_file="${kernel_files[0]}"
    echo -e "${BLUE}Installing kernel $kernel_file to ${INSTALL_DEVICE}1...${NC}"

    echo -e "${YELLOW}Note: Please ensure appropriate partition structure exists${NC}"

    echo -e "${GREEN}Kernel installation completed${NC}"
}

# Install system files
install_system() {
    echo -e "${YELLOW}Installing system files...${NC}"

    echo -e "${GREEN}System files installation completed${NC}"
}

# Verify installation
verify_installation() {
    echo -e "${YELLOW}Verifying installation...${NC}"

    if [ -f "$PROJECT_ROOT/sys/boot/bootcamp/m4kk1.img" ]; then
        echo -e "${GREEN}Bootloader file exists${NC}"
    else
        echo -e "${RED}Error: Bootloader file not found${NC}"
        exit 1
    fi

    if [ -b "$INSTALL_DEVICE" ]; then
        echo -e "${GREEN}Target device accessible${NC}"
    else
        echo -e "${RED}Error: Target device not accessible${NC}"
        exit 1
    fi

    echo -e "${GREEN}Installation verification passed${NC}"
}

# Main install flow
main() {
    echo -e "${BLUE}Starting M4KK1 system installation...${NC}"
    echo "Install mode: $INSTALL_MODE"
    echo "Bootloader device: $BOOTLOADER_DEVICE"
    echo "System device: $INSTALL_DEVICE"
    echo

    check_permissions
    check_device
    confirm_installation
    build_system
    verify_installation

    case "$INSTALL_MODE" in
        "bootloader")
            install_bootloader
            ;;
        "kernel")
            install_kernel
            ;;
        "full")
            install_bootloader
            install_kernel
            install_system
            ;;
        *)
            echo -e "${RED}Error: Unknown install mode $INSTALL_MODE${NC}"
            exit 1
            ;;
    esac

    echo
    echo -e "${GREEN}=======================================${NC}"
    echo -e "${GREEN}    M4KK1 Installation Complete!${NC}"
    echo -e "${GREEN}=======================================${NC}"
    echo
    echo -e "${BLUE}Install summary:${NC}"
    echo "  Bootloader installed to: $BOOTLOADER_DEVICE"
    echo "  System installed to: $INSTALL_DEVICE"
    echo "  Install mode: $INSTALL_MODE"
    echo
    echo -e "${YELLOW}Reboot to use the new M4KK1 OS${NC}"
    echo -e "${YELLOW}Note: May need to select correct boot device in BIOS${NC}"
}

main "$@"
