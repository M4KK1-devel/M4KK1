/* M4KK1 4P1 - altr.ld
 * Text editor loads at 0x1E10000, clear of:
 *   0x600000  Copland (display server)
 *   0x700000  Copland shared region (COPLAND_SHM_BASE)
 *   0x800000  init / login / m4sh / mdm (shell zone, mutually exclusive)
 *   0xB08000  pcc (console-only compiler, legacy overlaps tolerated)
 *   0xC00000  terminal emulator (memsz 0x1c18a0 -> ends 0xDC18A0)
 *   0xE00000  cptest (smoke client, full-test ISO)
 *   0xE80000  calcg (calculator, ends 0xED8C54)
 *   0x1000000 m4shg (graphical shell, ends 0x10A2D24)
 *   0x1100000 sprach WM — huge .bss footprint to 0x1CB22D4
 *   0x1D00000 fm (file manager, ends ~0x1E03000)
 * History: altr sat at 0xF00000 (collided with m4sht in the
 * full-test ISO and its .bss tail pressed on m4shg), then briefly
 * at 0x1100000 — which turned out to be sprach's live window
 * [0x1100000, 0x1CB22D4).  Final slot: [0x1E10000, 0x1F8D568),
 * leaving 0x72AA8 headroom below the reserved ramdisk at 0x2000000. */
ENTRY(_start)

SECTIONS
{
    . = 0x1E10000;

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
