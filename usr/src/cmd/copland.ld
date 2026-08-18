/*
 * M4KK1 4P1 - copland.ld
 * Description: Linker script for the Copland display server.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Copland loads at 0x600000.  The flat address space has no paging,
 * so each live image needs its own zone:
 *   0x600000  Copland (display server)
 *   0x700000  Copland shared region (COPLAND_SHM_BASE)
 *   0x800000  init / login / m4sh / mdm (shell zone)
 *   0x900000  Sprach (window manager)
 * Copland forks and execs Sprach; init forks Copland and stays in
 * the 0x800000 zone for the serial login chain.
 */

ENTRY(_start)

SECTIONS {
    . = 0x600000;

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
