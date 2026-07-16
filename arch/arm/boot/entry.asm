; M4KK1 4P1 - entry.asm
; Description: Boot entry point for ARM.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

.section ".text.boot"

.global _start
.extern mkrn_main
.extern m4k_stack_top

; M4KK1独特的ARM引导标识
.equ M4K_BOOT_MAGIC, 0x4D344B42

_start:
    ; M4KK1独特的ARM初始化
    mrs r0, cpsr
    bic r0, r0, #0x1F       ; 进入SVC模式
    orr r0, r0, #0x13       ; SVC模式
    msr cpsr_c, r0

    ; 设置M4KK1独特的栈
    ldr sp, =m4k_stack_top

    ; 清空BSS段
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
    b .check_bss

.clear_bss:
    str r2, [r0], #4
    add r0, r0, #4

.check_bss:
    cmp r0, r1
    blt .clear_bss

    ; 准备M4KK1独特的多引导信息
    mov r0, #0              ; 无多引导信息
    ldr r1, =M4K_BOOT_MAGIC ; M4KK1魔数

    ; 调用M4KK1内核主函数
    bl mkrn_main

    ; M4KK1独特的死循环
.halt:
    wfi
    b .halt

.section .data
.align 4
m4k_stack_bottom:
    .space 16384            ; 16KB栈
m4k_stack_top: