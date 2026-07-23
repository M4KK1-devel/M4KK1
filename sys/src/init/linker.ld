/**
 * M4KK1 Y4KU Kernel - 链接脚本
 * 定义内核的内存布局和段组织
 */

ENTRY(_start)

SECTIONS {
    /* 内核起始地址为1MB */
    . = 0x100000;

    /* 引导代码段 (Multiboot头必须在前8192字节内) */
    .text ALIGN(4096) : {
        *(.multiboot)
        LONG(0x4D344B4B)  /* M4KK魔数 */
        *(.text.boot)
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* 数据段 */
    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
    }

    /* BSS段（未初始化数据） */
    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* 内核结束标记 */
    __kernel_end = .;

    /* 内核栈空间（由entry.asm定义） */
    /* .bootstrap_stack ALIGN(4096) (NOLOAD) : */
    /* { */
    /*     __stack_bottom = .; */
    /*     . += 16384; */  /* 16KB栈 */
    /*     __stack_top = .; */
    /* } */

    /* 内核堆空间 */
    .kernel_heap ALIGN(4096) : {
        __heap_start = .;
        . += 0x400000;  /* 4MB堆 */
        __heap_end = .;
    }

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}

/* 内核符号 */
KERNEL_MAGIC = 0x4D344B4B;
KERNEL_BASE = 0x100000;
KERNEL_HEAP_SIZE = 0x100000;
KERNEL_STACK_SIZE = 32768;