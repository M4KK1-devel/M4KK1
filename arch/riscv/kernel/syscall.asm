# M4KK1 RISC-V Architecture - System Call Implementation
# 独特的M4KK1 RISC-V系统调用实现

.section ".text"

.global m4k_syscall_handler
.extern syscall_table

# M4KK1独特的RISC-V系统调用处理函数
m4k_syscall_handler:
    # 保存寄存器
    addi sp, sp, -128
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd t0, 24(sp)
    sd t1, 32(sp)
    sd t2, 40(sp)
    sd s0, 48(sp)
    sd s1, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)

    # 获取系统调用号（使用M4KK1独特的方式）
    ld a7, 120(sp)          # 从栈中获取系统调用号

    # 验证系统调用号
    li t0, 0
    blt a7, t0, invalid_syscall
    li t0, 256
    bge a7, t0, invalid_syscall

    # 调用系统调用处理函数
    la t0, syscall_table
    slli a7, a7, 3
    add t0, t0, a7
    ld t0, 0(t0)
    beqz t0, invalid_syscall

    # 调用处理函数
    jalr t0

    # 恢复寄存器
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld t0, 24(sp)
    ld t1, 32(sp)
    ld t2, 40(sp)
    ld s0, 48(sp)
    ld s1, 56(sp)
    ld a0, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)
    addi sp, sp, 128

    # 返回
    sret

invalid_syscall:
    # M4KK1独特的无效系统调用处理
    li a0, -1
    j m4k_syscall_handler

# M4KK1独特的系统调用入口点
.global m4k_syscall
m4k_syscall:
    # 使用M4KK1独特的方式进行系统调用
    ecall                   # RISC-V环境调用，但使用M4KK1独特的环境
    ret

# M4KK1独特的快速系统调用
.global m4k_fast_syscall
m4k_fast_syscall:
    # RISC-V特定的快速系统调用
    li a7, 0x4D             # M4KK1独特的环境调用号
    ecall
    ret