; M4KK1 4P1 - entry.asm
; Description: Boot entry point for PowerPC.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

.section ".text"

.global _start
.extern mkrn_main
.extern m4k_stack_top

# M4KK1独特的PowerPC引导标识
.equ M4K_BOOT_MAGIC, 0x4D344B42

_start:
    # M4KK1独特的PowerPC初始化
    mfmsr r3
    ori r3, r3, 0x2000      # 启用机器检查
    mtmsr r3

    # 设置M4KK1独特的栈
    lis r1, m4k_stack_top@h
    ori r1, r1, m4k_stack_top@l
    subi r1, r1, 256        # 栈指针预留空间

    # 清空BSS段
    lis r14, __bss_start@h
    ori r14, r14, __bss_start@l
    lis r15, __bss_end@h
    ori r15, r15, __bss_end@l
    sub r16, r15, r14
    beq .skip_bss

.clear_bss:
    stw r0, 0(r14)
    stw r0, 4(r14)
    stw r0, 8(r14)
    stw r0, 12(r14)
    addi r14, r14, 16
    sub r16, r16, 16
    bgt .clear_bss

.skip_bss:
    # 准备M4KK1独特的多引导信息
    li r3, 0                # 无多引导信息
    li r4, M4K_BOOT_MAGIC   # M4KK1魔数

    # 调用M4KK1内核主函数
    bl mkrn_main

    # M4KK1独特的死循环
.halt:
    li r3, 0
    mtmsr r3
    isync
    b .halt

.section .data
.align 3
m4k_stack_bottom:
    .space 16384            # 16KB栈
m4k_stack_top: