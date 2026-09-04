/* M4KK1 4P1 - cal_gui.ld
 * cal loads at 0x3400000 in the new GUI-app window
 * (0x3000000..0x4000000, buddy zone starts above). */

ENTRY(_start)

SECTIONS
{
    . = 0x3400000;

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
