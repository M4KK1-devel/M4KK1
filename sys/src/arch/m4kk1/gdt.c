/*
 * M4KK1 4P1 - gdt.c
 * Description: GDT management — gate setup, TSS
 *              installation, and kernel-stack update.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../include/gdt.h"
#include "../../include/stdint.h"
#include "../../include/string.h"
#include "../../include/console.h"

extern void gdt_set_tss_base(uint32_t base);
extern void gdt_set_tss_limit(uint32_t limit);

mkrn_gdt_entry_t gdt_entries[7];
mkrn_gdt_ptr_t gdt_ptr;

static mkrn_tss_entry_t tss_entry;
static uint32_t u32KernelStack = 0;

/**
 * @brief  Configure a single GDT entry.
 */
void
mkrn_gdt_set_gate(int32_t s32Num, uint32_t u32Base,
                  uint32_t u32Limit,
                  uint8_t u8Access,
                  uint8_t u8Gran)
{
    mkrn_gdt_entry_t *pEntry = &gdt_entries[s32Num];

    pEntry->base_low = (uint16_t)(u32Base & 0xFFFF);
    pEntry->base_middle =
        (uint8_t)((u32Base >> 16) & 0xFF);
    pEntry->base_high =
        (uint8_t)((u32Base >> 24) & 0xFF);

    pEntry->limit_low =
        (uint16_t)(u32Limit & 0xFFFF);
    pEntry->granularity =
        (uint8_t)((u32Limit >> 16) & 0x0F);

    pEntry->granularity |= (uint8_t)(u8Gran & 0xF0);
    pEntry->access = u8Access;
}

/**
 * @brief  Initialize the GDT.
 */
void
mkrn_gdt_init(void)
{
    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entries)
                               - 1);
    gdt_ptr.base = (uint32_t)&gdt_entries;

    mkrn_memset(gdt_entries, 0, sizeof(gdt_entries));

    mkrn_gdt_set_gate(
        1, 0x00000000, 0xFFFFFFFF,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_0
            | M4K_GDT_ACCESS_CODE_DATA
            | M4K_GDT_ACCESS_EXECUTABLE
            | M4K_GDT_ACCESS_READWRITE,
        M4K_GDT_GRANULARITY_4K
            | M4K_GDT_GRANULARITY_32BIT);

    mkrn_gdt_set_gate(
        2, 0x00000000, 0xFFFFFFFF,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_0
            | M4K_GDT_ACCESS_CODE_DATA
            | M4K_GDT_ACCESS_EXECUTABLE
            | M4K_GDT_ACCESS_READWRITE,
        M4K_GDT_GRANULARITY_4K
            | M4K_GDT_GRANULARITY_32BIT);

    mkrn_gdt_set_gate(
        3, 0x00000000, 0xFFFFFFFF,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_0
            | M4K_GDT_ACCESS_CODE_DATA
            | M4K_GDT_ACCESS_READWRITE,
        M4K_GDT_GRANULARITY_4K
            | M4K_GDT_GRANULARITY_32BIT);

    mkrn_gdt_set_gate(
        4, 0x00000000, 0xFFFFFFFF,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_3
            | M4K_GDT_ACCESS_CODE_DATA
            | M4K_GDT_ACCESS_EXECUTABLE
            | M4K_GDT_ACCESS_READWRITE,
        M4K_GDT_GRANULARITY_4K
            | M4K_GDT_GRANULARITY_32BIT);

    mkrn_gdt_set_gate(
        5, 0x00000000, 0xFFFFFFFF,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_3
            | M4K_GDT_ACCESS_CODE_DATA
            | M4K_GDT_ACCESS_READWRITE,
        M4K_GDT_GRANULARITY_4K
            | M4K_GDT_GRANULARITY_32BIT);

    mkrn_memset(&tss_entry, 0, sizeof(mkrn_tss_entry_t));
    mkrn_gdt_set_gate(
        6, (uint32_t)&tss_entry,
        sizeof(mkrn_tss_entry_t) - 1,
        M4K_GDT_ACCESS_PRESENT
            | M4K_GDT_ACCESS_PRIVILEGE_0 | 0x09,
        0x00);

    if (u32KernelStack != 0) {
        tss_entry.ss0 = M4K_GDT_KERNEL_DATA;
        tss_entry.esp0 = u32KernelStack;
    }

    __asm__ volatile(
        "lgdt %0" : : "m"(gdt_ptr) : "memory");
}

/**
 * @brief  Load the TSS via the LTR instruction.
 */
void
mkrn_tss_flush(void)
{
    gdt_set_tss_base((uint32_t)&tss_entry);
    gdt_set_tss_limit(
        (uint32_t)(sizeof(mkrn_tss_entry_t) - 1));

    __asm__ volatile(
        "ltr %%ax\n" : : "a"(M4K_GDT_TSS));
}

/**
 * @brief  Set the kernel stack pointer in the TSS.
 */
void
mkrn_set_kernel_stack(uint32_t u32Stack)
{
    u32KernelStack = u32Stack;

    tss_entry.ss0 = M4K_GDT_KERNEL_DATA;
    tss_entry.esp0 = u32Stack;
}
