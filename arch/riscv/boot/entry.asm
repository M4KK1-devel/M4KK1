; M4KK1 4P1 - entry.asm
; Description: Boot entry point for RISC-V.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

.section ".text.boot"

.global _start
.extern mkrn_main
.extern m4k_stack_top

# M4KK1独特的RISC-V引导标识
.equ M4K_BOOT_MAGIC, 0x4D344B42

_start:
    # M4KK1独特的RISC-V初始化
    csrr t0, mhartid
    bnez t0, .wait_forever

    # 设置M4KK1独特的栈
    la sp, m4k_stack_top

    # 清空BSS段
    la t0, __bss_start
    la t1, __bss_end
    bge t0, t1, .skip_bss

.clear_bss:
    sd zero, (t0)
    addi t0, t0, 8
    blt t0, t1, .clear_bss

.skip_bss:
    # 准备M4KK1独特的多引导信息
    li a0, 0                # 无多引导信息
    li a1, M4K_BOOT_MAGIC   # M4KK1魔数

    # 调用M4KK1内核主函数
    call mkrn_main

    # M4KK1独特的死循环
.halt:
    wfi
    j .halt

.wait_forever:
    wfi
    j .wait_forever

.section .data
.align 4
m4k_stack_bottom:
    .space 16384            # 16KB栈
m4k_stack_top: