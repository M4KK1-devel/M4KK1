/* M4KK1 4P1 - clock_gui.ld
 * clock loads at 0xEC00000, clear of the other GUI apps and m4sh variants. */

ENTRY(_start)

SECTIONS
{
    . = 0xFD8000;

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
