/*
 * M4KK1 4P1 - memory.c
 * Description: Kernel memory allocator stubs.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifdef USE_STUB_MEMORY

#include <stdint.h>
#include <stddef.h>

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
