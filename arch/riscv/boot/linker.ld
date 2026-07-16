/* M4KK1 RISC-V Architecture - Linker Script */
/* 独特的M4KK1 RISC-V链接器脚本 */

OUTPUT_FORMAT(elf64-littleriscv)
OUTPUT_ARCH(riscv)

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