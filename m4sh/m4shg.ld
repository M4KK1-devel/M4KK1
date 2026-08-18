/* M4KK1 4P1 - m4shg.ld
 * Graphical-terminal shell (m4shg) loads at 0x1000000 (16 MB), clear of:
 *   0x100000..~0x500000  kernel image + 4MB kernel heap + boot stack
 *   0x600000             Copland (display server)
 *   0x700000             Copland shared region (COPLAND_SHM_BASE)
 *   0x800000             init / login / m4sh / mdm (shell zone, LIVE)
 *   0x900000..~0xBB0000  Sprach (window manager code + BSS)
 *   0xC00000             /bin/terminal (terminal emulator)
 * The flat address space has no paging, so the exec'd shell image must
 * never overlap any live process.  The kernel page allocator is
 * first-fit from low memory; 16 MB is far above anything it hands out
 * (fork/exec stacks total a few hundred KB), so the region stays free.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x1000000;

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

    __end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
