/* M4KK1 x86_64 Architecture - Linker Script */
/* 独特的M4KK1 x86_64链接器脚本 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

SECTIONS
{
    . = 0xC0000000;         /* M4KK1独特的高半内核地址 */

    .text : AT(ADDR(.text) - 0xC0000000)
    {
        *(.multiboot_header)
        *(.text.boot)
        *(.text)
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}