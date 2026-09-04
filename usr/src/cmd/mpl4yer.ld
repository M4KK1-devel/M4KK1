/* M4KK1 4P1 - mpl4yer.ld
 * mpl4yer loads at 0x3300000 in the new GUI-app window
 * (0x3000000..0x4000000, buddy zone starts above). */

ENTRY(_start)

SECTIONS
{
    . = 0x3300000;

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
