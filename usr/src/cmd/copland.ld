/*
 * M4KK1 4P1 - copland.ld
 * Description: Linker script for the Copland display server.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Copland loads at 0x800000 (same zone as m4sh).  Sprach will link at
 * 0x900000 so the two images can coexist in the flat address space
 * after Copland forks and execs Sprach.
 */

ENTRY(_start)

SECTIONS {
    . = 0x800000;

    .text ALIGN(4096) : {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
