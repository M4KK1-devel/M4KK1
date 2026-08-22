/* M4KK1 4P1 - calc_gui.ld
 * Calculator loads at 0xE80000, clear of:
 *   0xD00000  file manager
 *   0xE00000  cptest
 *   0xF00000  altr (text editor)
 *   0x1100000 m4shg (graphical shell) */

ENTRY(_start)

SECTIONS
{
    . = 0xE80000;

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
