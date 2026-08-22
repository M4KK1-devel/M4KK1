/* M4KK1 4P1 - sprach.ld
 * Sprach window manager loads at 0x1100000.  Its BSS is huge
 * (~10MB: launchpad + terminal composite buffers), reaching
 * ~0x1ADBE40.  The old 0x900000 base put those buffers straight on
 * top of terminal (0xC00000) and fm (0xD00000) — the "crash exactly
 * when a terminal is alive" bug.  Keep the zone clear of m4shg
 * (ends ~0x105A000) and the ramdisk window (0x2000000+). */

ENTRY(_start)

SECTIONS
{
    . = 0x1100000;

    .text : {
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.rel*)
        *(.symtab)
        *(.strtab)
    }
}
