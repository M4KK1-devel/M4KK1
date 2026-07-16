/*
 * M4KK1 4P1 - elf.h
 * Description: ELF binary format structure definitions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_ELF_NIDENT       16
#define M4K_ELFMAG0         0x7F
#define M4K_ELFMAG1         'E'
#define M4K_ELFMAG2         'L'
#define M4K_ELFMAG3         'F'

typedef struct {
    u8  e_ident[M4K_ELF_NIDENT];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u32 e_entry;
    u32 e_phoff;
    u32 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __attribute__((packed)) mkrn_elf32_ehdr_t;

typedef struct {
    u32 p_type;
    u32 p_offset;
    u32 p_vaddr;
    u32 p_paddr;
    u32 p_filesz;
    u32 p_memsz;
    u32 p_flags;
    u32 p_align;
} __attribute__((packed)) mkrn_elf32_phdr_t;

#define M4K_EI_CLASS        4
#define M4K_EI_DATA         5
#define M4K_EI_VERSION      6
#define M4K_EI_OSABI        7

#define M4K_ELFCLASS32      1
#define M4K_ELFCLASS64      2
#define M4K_ELFDATA2LSB     1
#define M4K_ET_EXEC         2
#define M4K_EM_386          3
#define M4K_EV_CURRENT      1

#define M4K_PT_NULL         0
#define M4K_PT_LOAD         1

#define M4K_PF_X            1
#define M4K_PF_W            2
#define M4K_PF_R            4
