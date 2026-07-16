; M4KK1 4P1 - entry.asm
; Description: Boot entry point for x86.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

BITS 32

; M4KK1独特的引导标识
M4K_BOOT_MAGIC equ 0x4D344B42  ; "M4KB"

section .multiboot_header
align 4
multiboot_header:
    ; M4KK1独特的魔数
    dd M4K_BOOT_MAGIC
    dd 0                    ; 标志
    dd -(M4K_BOOT_MAGIC + 0); 校验和

    ; M4KK1独特的信息标签
    dw 1                    ; 类型
    dw 0                    ; 标志
    dd 12                   ; 大小
    dd multiboot_header     ; 头地址

section .text
global _start
extern mkrn_main
extern m4k_stack_top

_start:
    ; M4KK1独特的32位初始化
    cli

    ; 设置M4KK1独特的栈
    mov esp, m4k_stack_top

    ; 传递M4KK1独特的多引导信息
    push ebx                ; 多引导信息
    push M4K_BOOT_MAGIC     ; 魔数

    ; 调用M4KK1内核主函数
    call mkrn_main

    ; M4KK1独特的死循环
.halt:
    cli
    hlt
    jmp .halt

section .bss
align 16
m4k_stack_bottom:
    resb 16384              ; 16KB栈
m4k_stack_top: