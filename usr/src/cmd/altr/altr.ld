/* M4KK1 4P1 - altr.ld
 * ALTR2 (VSCode-style editor) loads at 0x3800000 in the GUI-app
 * window (0x3000000..0x4000000, buddy zone starts above).  The old
 * 0xF00000 slot cannot hold ALTR2: its .bss (pixel buffer 1.18MB +
 * 6-doc line arena 1.44MB) would overrun logview's 0xF50000 base. */

ENTRY(_start)

SECTIONS
{
    . = 0x3800000;

    .text : {
        *(.text*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata*)
    }

    .data ALIGN(4096) : {
        *(.data*)
    }

    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}
