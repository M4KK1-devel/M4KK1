/*
 * M4KK1 4P1 - stubs.c
 * Description: Stub implementations for unimplemented
 *              kernel functions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/stdint.h"
#include "../include/stdarg.h"
#include "../include/stddef.h"

extern void mkrn_console_write(const char *pStr);

void
__stack_chk_fail(void)
{
    mkrn_console_write(
        "Stack overflow detected\n");
    while (1) { }
}

void
__stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

void
process_return(void)
{
    mkrn_console_write(
        "Process returned unexpectedly\n");
    while (1) { }
}

#ifdef USE_STUB_STRING

size_t
strlen(const char *pS)
{
    const char *p = pS;
    while (*p)
        p++;
    return (size_t)(p - pS);
}

char *
strdup(const char *pS)
{
    (void)pS;
    return (char *)0x100000;
}

#endif /* USE_STUB_STRING */

#ifdef USE_STUB_MEMORY

void *
kmalloc(size_t size)
{
    (void)size;
    return (void *)0x100000;
}

void
kfree(void *pPtr)
{
    (void)pPtr;
}

#endif /* USE_STUB_MEMORY */

#ifdef USE_STUB_CONSOLE

void
mkrn_console_init(void) { }

void
mkrn_console_write(const char *pStr)
{
    (void)pStr;
}

void
mkrn_console_write_hex(uint32_t u32Value)
{
    (void)u32Value;
}

void
mkrn_console_write_dec(uint32_t u32Value)
{
    (void)u32Value;
}

void
mkrn_console_put_char(char c)
{
    (void)c;
}

#endif /* USE_STUB_CONSOLE */

#ifdef USE_STUB_MEMORY_INFO

void *
mkrn_mem_init(void)
{
    return (void *)0x100000;
}

uint32_t
mkrn_mem_get_total(void)
{
    return 128 * 1024 * 1024;
}

uint32_t
mkrn_mem_get_free(void)
{
    return 64 * 1024 * 1024;
}

#endif /* USE_STUB_MEMORY_INFO */

#ifdef USE_STUB_GDT

void
mkrn_gdt_init(void) { }

#endif /* USE_STUB_GDT */

#ifdef USE_STUB_IDT

void
mkrn_idt_init(void) { }

void
mkrn_idt_register_handler(uint8_t num, void *pHandler)
{
    (void)num;
    (void)pHandler;
}

#endif /* USE_STUB_IDT */

#ifdef USE_STUB_TIMER

void
mkrn_timer_init(uint32_t u32Frequency)
{
    (void)u32Frequency;
}

uint32_t
mkrn_timer_get_frequency(void)
{
    return 1000;
}

#endif /* USE_STUB_TIMER */
