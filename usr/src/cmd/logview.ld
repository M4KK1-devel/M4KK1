/* M4KK1 4P1 - logview.ld
 * logview loads at 0xEC80000, clear of the other GUI apps and m4sh variants. */

ENTRY(_start)

SECTIONS
{
    . = 0xF50000;

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
