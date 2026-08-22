/*
 * M4KK1 4P1 - cptest.ld
 * Description: Linker script for the Copland protocol smoke client.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Address map (flat, no paging):
 *   0x600000  Copland daemon
 *   0x700000  Copland shared region + conn table (0x720000)
 *   0x800000  init / login / m4sh / mdm shell zone
 *   0xC00000  terminal
 *   0xD00000  fm
 *   0xE00000  cptest (this image, incl. 64KB pixel pool in BSS)
 *   0x1100000 Sprach WM
 *   0x2000000 ramdisk (kernel-reserved)
 */

ENTRY(_start)

SECTIONS {
    . = 0xE00000;

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
