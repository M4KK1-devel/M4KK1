/* M4KK1 4P1 - altr.ld
 * Text editor loads at 0xF00000, clear of:
 *   0x600000  Copland (display server)
 *   0x700000  Copland shared region (COPLAND_SHM_BASE)
 *   0x800000  init / login / m4sh / mdm (shell zone)
 *   0x900000  Sprach (window manager)
 *   0xC00000  terminal emulator
 *   0xD00000  file manager
 *   0xE00000  cptest
 *   0xE80000  calculator
 *   0x1100000 m4shg (graphical shell)
 * The flat address space has no paging, so a fork+exec'd child must
 * never overlap the parent's live code/data. */

ENTRY(_start)

SECTIONS
{
    . = 0xF00000;

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
