/*
 * m4sht.ld — serial test shell for autologin (full-test) builds.
 *
 * MDM itself runs at 0x800000 (m4sh.ld), so the parallel serial
 * shell spawned beside the desktop session needs its own window.
 * Windows in use inside the flat user window (0x400000..0x2000000):
 * copland 0x600000, MDM/m4sh 0x800000, terminal 0xC00000, filemgr
 * 0xD00000, cptest 0xE00000, sprach 0x1100000 (BSS to ~0x1AF7020).
 * 0xF00000 is free.
 */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
    . = 0xF00000;

    .text ALIGN(4096) : {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
