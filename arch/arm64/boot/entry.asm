; M4KK1 4P1 - entry.asm
; Description: Boot entry point for ARM64.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

.section ".text.boot"

.global _start
.extern mkrn_main
.extern m4k_stack_top

; M4KK1独特的ARM64引导标识
.equ M4K_BOOT_MAGIC, 0x4D344B42

_start:
    ; M4KK1独特的ARM64初始化
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbz x0, master_core

    ; 其他核心等待
.wait_forever:
    wfe
    b .wait_forever

master_core:
    ; 设置M4KK1独特的栈
    ldr x0, =m4k_stack_top
    mov sp, x0

    ; 清空BSS段
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x1, x1, x0
    cbz x1, .skip_bss

.clear_bss:
    str xzr, [x0], #8
    subs x1, x1, #8
    b.gt .clear_bss

.skip_bss:
    ; 准备M4KK1独特的多引导信息
    mov x0, #0              ; 无多引导信息
    mov x1, M4K_BOOT_MAGIC  ; M4KK1魔数

    ; 调用M4KK1内核主函数
    bl mkrn_main

    ; M4KK1独特的死循环
.halt:
    wfi
    b .halt

.section .data
.align 3
m4k_stack_bottom:
    .space 16384            ; 16KB栈
m4k_stack_top: