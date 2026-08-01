/* M4KK1 4P1 - sprach.ld
 * Sprach window manager loads at 0x900000 (option A: copland 0x800000,
 * sprach 0x900000), one ELF per layout mode. */

ENTRY(_start)

SECTIONS
{
    . = 0x900000;

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
