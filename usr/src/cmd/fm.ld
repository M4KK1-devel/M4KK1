/* M4KK1 4P1 - fm.ld
 * File manager loads at 0x1D00000, clear of:
 *   (see altr.ld for the full map)
 *   0x1100000 sprach WM .bss footprint ends 0x1CB22D4
 *   0x1E10000 altr (editor)
 * History: fm sat at 0xD00000 with memsz ~0x102xxx (940KB pixel
 * buffer .bss), so its top ran to ~0xE022F4 and the execve memsz
 * memset smeared the first pages of cptest's .text at 0xE00000 —
 * the "fm spawn freezes the whole desktop" bug.  New window:
 * [0x1D00000, ~0x1E03000), clear of sprach (ends 0x1CB22D4) with
 * 0x2DD2C headroom and of altr at 0x1E10000. */
ENTRY(_start)

SECTIONS
{
    . = 0x1D00000;

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
