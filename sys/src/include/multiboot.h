/*
 * M4KK1 4P1 - multiboot.h
 * Description: Multiboot specification structures and constants.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define M4K_MULTIBOOT_INFO_MEMORY       0x00000001
#define M4K_MULTIBOOT_INFO_BOOTDEV      0x00000002
#define M4K_MULTIBOOT_INFO_CMDLINE      0x00000004
#define M4K_MULTIBOOT_INFO_MODS         0x00000008
#define M4K_MULTIBOOT_INFO_AOUT_SYMS    0x00000010
#define M4K_MULTIBOOT_INFO_ELF_SHDR     0x00000020
#define M4K_MULTIBOOT_INFO_MEM_MAP       0x00000040
#define M4K_MULTIBOOT_INFO_DRIVE_INFO   0x00000080
#define M4K_MULTIBOOT_INFO_CONFIG_TABLE 0x00000100
#define M4K_MULTIBOOT_INFO_BOOT_LOADER_NAME 0x00000200
#define M4K_MULTIBOOT_INFO_APM_TABLE     0x00000400
#define M4K_MULTIBOOT_INFO_VBE_INFO     0x00000800
#define M4K_MULTIBOOT_INFO_FRAMEBUFFER_INFO 0x00001000

typedef struct mkrn_multiboot_info {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u32 vbe_mode;
    u32 vbe_interface_seg;
    u32 vbe_interface_off;
    u32 vbe_interface_len;
} multiboot_info_t;

typedef struct mkrn_multiboot_mmap_entry {
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#define M4K_MULTIBOOT_MEMORY_AVAILABLE  1
#define M4K_MULTIBOOT_MEMORY_RESERVED   2
#define M4K_MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define M4K_MULTIBOOT_MEMORY_NVS        4
#define M4K_MULTIBOOT_MEMORY_BADRAM     5

typedef struct mkrn_multiboot_mod_list {
    u32 mod_start;
    u32 mod_end;
    u32 cmdline;
    u32 pad;
} multiboot_mod_list_t;

typedef struct mkrn_multiboot_elf_section_header_table {
    u32 num;
    u32 size;
    u32 addr;
    u32 shndx;
} multiboot_elf_section_header_table_t;

typedef struct mkrn_multiboot_aout_symbol_table {
    u32 tabsize;
    u32 strsize;
    u32 addr;
    u32 reserved;
} multiboot_aout_symbol_table_t;
