/* M4KK1 4P1 - calc_gui.ld
 * Calculator moved to the new GUI-app window at 0x3700000
 * (0x3000000..0x4000000, buddy zone starts above) — its old
 * 0xE80000 slot overlapped the pcc mutex zone. */

ENTRY(_start)

SECTIONS
{
    . = 0x3700000;

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
