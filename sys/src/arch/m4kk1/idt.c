/*
 * M4KK1 4P1 - idt.c
 * Description: IDT management — gate setup, handler
 *              registration, exception dispatch, PIC
 *              EOI, and interrupt-state helpers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../include/idt.h"
#include "../../include/stdint.h"
#include "../../include/string.h"
#include "../../include/console.h"
#include "../../include/kernel.h"

extern void idt_init(void);
extern void idt_set_gate(uint8_t num, uint32_t base,
                         uint16_t selector,
                         uint8_t flags);
extern void pic_init(void);
extern void enable_interrupts(void);
extern void disable_interrupts(void);
extern uint32_t interrupts_enabled(void);
extern void pic_send_eoi(uint32_t irq_num);

static mkrn_int_handler_t
    interrupt_handlers[256];

static const char *exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "FPU floating point error",
    "Alignment check",
    "Machine check",
    "SIMD floating point error",
    "Virtualization error",
    "Control protection error",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

/**
 * @brief  Initialize the IDT system.
 */
void
mkrn_idt_init(void)
{
    mkrn_memset(interrupt_handlers, 0,
           sizeof(interrupt_handlers));

    pic_init();

    idt_init();

    M4K_LOG_INFO("IDT initialized successfully");
}

/**
 * @brief  Set an IDT gate entry.
 */
void
mkrn_idt_set_gate(uint8_t u8Num, uint32_t u32Base,
                  uint16_t u16Selector,
                  uint8_t u8Flags)
{
    idt_set_gate(u8Num, u32Base, u16Selector,
                 u8Flags);
}

/**
 * @brief  Register an interrupt handler.
 */
void
mkrn_idt_register_handler(
    uint8_t u8Num,
    mkrn_int_handler_t handler)
{
    interrupt_handlers[u8Num] = handler;
    M4K_LOG_INFO(
        "Interrupt handler registered for "
        "vector 0x");
    mkrn_console_write_hex(u8Num);
    mkrn_console_write("\n");
}

/**
 * @brief  Unregister an interrupt handler.
 */
void
mkrn_idt_unregister_handler(uint8_t u8Num)
{
    interrupt_handlers[u8Num] = NULL;
    M4K_LOG_INFO(
        "Interrupt handler unregistered for "
        "vector 0x");
    mkrn_console_write_hex(u8Num);
    mkrn_console_write("\n");
}

/**
 * @brief  Get the handler registered for a vector.
 */
mkrn_int_handler_t
mkrn_idt_get_handler(uint8_t u8Num)
{
    return interrupt_handlers[u8Num];
}

/**
 * @brief  Enable interrupts.
 */
void
mkrn_idt_enable_interrupts(void)
{
    enable_interrupts();
    M4K_LOG_INFO("Interrupts enabled");
}

/**
 * @brief  Disable interrupts.
 */
void
mkrn_idt_disable_interrupts(void)
{
    disable_interrupts();
    M4K_LOG_INFO("Interrupts disabled");
}

/**
 * @brief  Check whether interrupts are enabled.
 */
uint32_t
mkrn_idt_interrupts_enabled(void)
{
    return interrupts_enabled();
}

/**
 * @brief  Handle an exception (fault / trap).
 */
void
mkrn_idt_handle_exception(uint32_t u32Vector)
{
    const char *pMessage = NULL;

    if (u32Vector < 32
        && exception_messages[u32Vector])
        pMessage = exception_messages[u32Vector];
    else
        pMessage = "Unknown exception";

    M4K_LOG_ERROR("*** EXCEPTION OCCURRED ***");
    M4K_LOG_ERROR("Vector: 0x");
    mkrn_console_write_hex(u32Vector);
    mkrn_console_write("\n");
    M4K_LOG_ERROR("Error: ");
    mkrn_console_write(pMessage);
    mkrn_console_write("\n");

    if (interrupt_handlers[u32Vector]) {
        M4K_LOG_INFO(
            "Calling registered exception "
            "handler...");
        interrupt_handlers[u32Vector]();
    } else {
        M4K_LOG_ERROR(
            "No handler registered for this "
            "exception.");
        M4K_LOG_ERROR("System halted.");

        mkrn_idt_disable_interrupts();
        while (1) {}
    }
}

/**
 * @brief  Handle an IRQ interrupt.
 */
void
mkrn_idt_handle_irq(uint32_t u32IrqNum)
{
    uint32_t u32Vector = u32IrqNum + 0x20;

    pic_send_eoi(u32IrqNum);

    if (interrupt_handlers[u32Vector])
        interrupt_handlers[u32Vector]();
    else {
        M4K_LOG_WARN("Unhandled IRQ ");
        mkrn_console_write_dec(u32IrqNum);
        mkrn_console_write(" (vector 0x");
        mkrn_console_write_hex(u32Vector);
        mkrn_console_write(")\n");
    }
}

/**
 * @brief  Get the description string for an
 *         exception vector.
 */
const char *
mkrn_idt_get_exception_message(uint32_t u32Vector)
{
    if (u32Vector < 32
        && exception_messages[u32Vector])
        return exception_messages[u32Vector];
    return "Unknown exception";
}

/**
 * @brief  Print the current IDT status.
 */
void
mkrn_idt_print_status(void)
{
    uint32_t u32Count = 0;

    M4K_LOG_INFO("IDT Status:");
    M4K_LOG_INFO("Registered handlers:");

    for (uint32_t i = 0; i < 256; i++) {
        if (interrupt_handlers[i]) {
            if (i < 32) {
                M4K_LOG_INFO("  Vector 0x");
                mkrn_console_write_hex(i);
                mkrn_console_write(
                    " (Exception): ");
                mkrn_console_write(
                    mkrn_idt_get_exception_message(
                        i));
                mkrn_console_write("\n");
            } else if (i >= 0x20 && i < 0x30) {
                M4K_LOG_INFO("  Vector 0x");
                mkrn_console_write_hex(i);
                mkrn_console_write(" (IRQ ");
                mkrn_console_write_dec(i - 0x20);
                mkrn_console_write(
                    "): Registered\n");
            } else {
                M4K_LOG_INFO("  Vector 0x");
                mkrn_console_write_hex(i);
                mkrn_console_write(
                    ": Registered\n");
            }
            u32Count++;
        }
    }

    M4K_LOG_INFO("Total registered handlers: ");
    mkrn_console_write_dec(u32Count);
    mkrn_console_write("\n");
    M4K_LOG_INFO("Interrupts are ");
    mkrn_console_write(
        mkrn_idt_interrupts_enabled()
            ? "enabled" : "disabled");
    mkrn_console_write("\n");
}
