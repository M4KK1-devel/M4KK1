/*
 * M4KK1 4P1 - gdt.h
 * Description: Global Descriptor Table definitions and operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

typedef struct {
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attribute__((packed)) mkrn_gdt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) mkrn_gdt_ptr_t;

#define M4K_GDT_KERNEL_CODE 0x08
#define M4K_GDT_KERNEL_CODE2 0x10
#define M4K_GDT_KERNEL_DATA 0x18
#define M4K_GDT_USER_CODE   0x20
#define M4K_GDT_USER_DATA   0x28
#define M4K_GDT_TSS         0x30

/* GDT access byte flags */
#define M4K_GDT_ACCESS_PRESENT      0x80
#define M4K_GDT_ACCESS_PRIVILEGE_0  0x00
#define M4K_GDT_ACCESS_PRIVILEGE_1  0x20
#define M4K_GDT_ACCESS_PRIVILEGE_2  0x40
#define M4K_GDT_ACCESS_PRIVILEGE_3  0x60
#define M4K_GDT_ACCESS_CODE_DATA    0x10
#define M4K_GDT_ACCESS_EXECUTABLE   0x08
#define M4K_GDT_ACCESS_DIRECTION    0x04
#define M4K_GDT_ACCESS_READWRITE    0x02
#define M4K_GDT_ACCESS_ACCESSED     0x01

/* GDT granularity flags */
#define M4K_GDT_GRANULARITY_1B      0x00
#define M4K_GDT_GRANULARITY_4K      0x80
#define M4K_GDT_GRANULARITY_16BIT   0x00
#define M4K_GDT_GRANULARITY_32BIT   0x40

typedef struct {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed)) mkrn_tss_entry_t;

/**
 * mkrn_gdt_init - Initialize the GDT
 *
 * Return: void
 */
void mkrn_gdt_init(void);

/**
 * mkrn_gdt_set_gate - Set a GDT entry
 * @num: Entry index
 * @base: Segment base address
 * @limit: Segment limit
 * @access: Access flags
 * @gran: Granularity flags
 *
 * Return: void
 */
void mkrn_gdt_set_gate(s32 num, u32 base, u32 limit, u8 access, u8 gran);

/**
 * mkrn_gdt_flush - Flush and reload the GDT
 *
 * Return: void
 */
void mkrn_gdt_flush(void);

/**
 * mkrn_tss_flush - Load the TSS
 *
 * Return: void
 */
void mkrn_tss_flush(void);

/**
 * mkrn_set_kernel_stack - Set kernel stack for TSS
 * @stack: Stack pointer value
 *
 * Return: void
 */
void mkrn_set_kernel_stack(u32 stack);
