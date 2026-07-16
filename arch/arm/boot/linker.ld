/* M4KK1 ARM Architecture - Linker Script */
/* 独特的M4KK1 ARM链接器脚本 */

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)

ENTRY(_start)

SECTIONS
{
    . = 0xC0000000;         /* M4KK1独特的高半内核地址 */

    .text : AT(ADDR(.text) - 0xC0000000)
    {
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