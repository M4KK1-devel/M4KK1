/*
 * M4KK1 4P1 - idt.h
 * Description: Interrupt Descriptor Table definitions and operations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

typedef struct {
    u16 base_low;
    u16 selector;
    u8 always0;
    u8 flags;
    u16 base_high;
} __attribute__((packed)) mkrn_idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) mkrn_idt_ptr_t;

typedef void (*mkrn_int_handler_t)(void);

/* IDT flag bits */
#define M4K_IDT_PRESENT         0x80
#define M4K_IDT_DPL_0           0x00
#define M4K_IDT_DPL_1           0x20
#define M4K_IDT_DPL_2           0x40
#define M4K_IDT_DPL_3           0x60
#define M4K_IDT_STORAGE_SEGMENT 0x10
#define M4K_IDT_GATE_16BIT      0x00
#define M4K_IDT_GATE_32BIT      0x08
#define M4K_IDT_TRAP_GATE       0x07
#define M4K_IDT_INTERRUPT_GATE  0x06
#define M4K_IDT_INTERRUPT_GATE_32 0x0E
#define M4K_IDT_TASK_GATE       0x05

/* Interrupt vector numbers */
#define M4K_IDT_DIVIDE_BY_ZERO          0x00
#define M4K_IDT_DEBUG                   0x01
#define M4K_IDT_NMI                     0x02
#define M4K_IDT_BREAKPOINT              0x03
#define M4K_IDT_OVERFLOW                0x04
#define M4K_IDT_BOUND_RANGE             0x05
#define M4K_IDT_INVALID_OPCODE          0x06
#define M4K_IDT_DEVICE_NOT_AVAILABLE    0x07
#define M4K_IDT_DOUBLE_FAULT            0x08
#define M4K_IDT_COPROCESSOR_SEGMENT     0x09
#define M4K_IDT_INVALID_TSS             0x0A
#define M4K_IDT_SEGMENT_NOT_PRESENT     0x0B
#define M4K_IDT_STACK_SEGMENT_FAULT     0x0C
#define M4K_IDT_GENERAL_PROTECTION      0x0D
#define M4K_IDT_PAGE_FAULT              0x0E
#define M4K_IDT_RESERVED                0x0F
#define M4K_IDT_FPU_ERROR               0x10
#define M4K_IDT_ALIGNMENT_CHECK         0x11
#define M4K_IDT_MACHINE_CHECK           0x12
#define M4K_IDT_SIMD_ERROR              0x13
#define M4K_IDT_VIRT_EXCEPTION          0x14
#define M4K_IDT_CONTROL_PROTECTION      0x15

#define M4K_IDT_TIMER                   0x20
#define M4K_IDT_KEYBOARD                0x21
#define M4K_IDT_CASCADE                 0x22
#define M4K_IDT_COM2                    0x23
#define M4K_IDT_COM1                    0x24
#define M4K_IDT_LPT2                    0x25
#define M4K_IDT_FLOPPY                  0x26
#define M4K_IDT_LPT1                    0x27
#define M4K_IDT_RTC                     0x28
#define M4K_IDT_FREE1                   0x29
#define M4K_IDT_FREE2                   0x2A
#define M4K_IDT_FREE3                   0x2B
#define M4K_IDT_MOUSE                   0x2C
#define M4K_IDT_FPU                     0x2D
#define M4K_IDT_PRIMARY_ATA             0x2E
#define M4K_IDT_SECONDARY_ATA           0x2F

/**
 * mkrn_idt_init - Initialize the IDT
 *
 * Return: void
 */
void mkrn_idt_init(void);
void mkrn_idt_init_c(void);

/**
 * mkrn_idt_set_gate - Set an IDT entry
 * @num: Interrupt vector number
 * @base: Handler function address
 * @selector: Code segment selector
 * @flags: Gate flags
 *
 * Return: void
 */
void mkrn_idt_set_gate(u8 num, u32 base, u16 selector, u8 flags);
void mkrn_idt_set_gate_c(u8 num, u32 base, u16 selector, u8 flags);

/**
 * mkrn_idt_register_handler - Register an interrupt handler
 * @num: Interrupt vector number
 * @handler: Handler function pointer
 *
 * Return: void
 */
void mkrn_idt_register_handler(u8 num, mkrn_int_handler_t handler);

/**
 * mkrn_idt_enable_interrupts - Enable interrupts (STI)
 *
 * Return: void
 */
void mkrn_idt_enable_interrupts(void);

/**
 * mkrn_idt_disable_interrupts - Disable interrupts (CLI)
 *
 * Return: void
 */
void mkrn_idt_disable_interrupts(void);

/* External interrupt handler declarations */
extern void mkrn_idt_handler_00(void);
extern void mkrn_idt_handler_01(void);
extern void mkrn_idt_handler_02(void);
extern void mkrn_idt_handler_03(void);
extern void mkrn_idt_handler_04(void);
extern void mkrn_idt_handler_05(void);
extern void mkrn_idt_handler_06(void);
extern void mkrn_idt_handler_07(void);
extern void mkrn_idt_handler_08(void);
extern void mkrn_idt_handler_09(void);
extern void mkrn_idt_handler_0A(void);
extern void mkrn_idt_handler_0B(void);
extern void mkrn_idt_handler_0C(void);
extern void mkrn_idt_handler_0D(void);
extern void mkrn_idt_handler_0E(void);
extern void mkrn_idt_handler_0F(void);
extern void mkrn_idt_handler_10(void);
extern void mkrn_idt_handler_11(void);
extern void mkrn_idt_handler_12(void);
extern void mkrn_idt_handler_13(void);
extern void mkrn_idt_handler_14(void);
extern void mkrn_idt_handler_15(void);
extern void mkrn_idt_handler_20(void);
extern void mkrn_idt_handler_21(void);
extern void mkrn_idt_handler_22(void);
extern void mkrn_idt_handler_23(void);
extern void mkrn_idt_handler_24(void);
extern void mkrn_idt_handler_25(void);
extern void mkrn_idt_handler_26(void);
extern void mkrn_idt_handler_27(void);
extern void mkrn_idt_handler_28(void);
extern void mkrn_idt_handler_29(void);
extern void mkrn_idt_handler_2A(void);
extern void mkrn_idt_handler_2B(void);
extern void mkrn_idt_handler_2C(void);
extern void mkrn_idt_handler_2D(void);
extern void mkrn_idt_handler_2E(void);
extern void mkrn_idt_handler_2F(void);
