/* M4KK1 4P1 - heaptest.ld
 * heaptest loads at 0x900000: clear of the m4sh family (0x800000),
 * pcc (0xB00000), terminal (0xC00000) and m4sht (0xF00000). */

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
