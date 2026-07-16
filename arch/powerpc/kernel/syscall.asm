# M4KK1 PowerPC Architecture - System Call Implementation
# 独特的M4KK1 PowerPC系统调用实现

.section ".text"

.global m4k_syscall_handler
.extern syscall_table

# M4KK1独特的PowerPC系统调用处理函数
m4k_syscall_handler:
    # 保存寄存器
    stwu r1, -256(r1)       # 栈帧
    stmw r0, 8(r1)          # 保存所有寄存器

    # 获取系统调用号（使用M4KK1独特的方式）
    lwz r0, 256(r1)         # 从栈中获取系统调用号

    # 验证系统调用号
    cmpwi r0, 0
    blt invalid_syscall
    cmpwi r0, 256
    bge invalid_syscall

    # 调用系统调用处理函数
    lis r3, syscall_table@h
    ori r3, r3, syscall_table@l
    rlwinm r0, r0, 2, 0, 29  # 乘以4
    lwzx r3, r3, r0
    cmpwi r3, 0
    beq invalid_syscall

    # 调用处理函数
    mtlr r3
    blrl

    # 恢复寄存器
    lmw r0, 8(r1)
    addi r1, r1, 256

    # 返回
    rfi

invalid_syscall:
    # M4KK1独特的无效系统调用处理
    li r3, -1
    b m4k_syscall_handler

# M4KK1独特的系统调用入口点
.global m4k_syscall
m4k_syscall:
    # 使用M4KK1独特的方式进行系统调用
    sc                      # 系统调用，但使用M4KK1独特的环境
    blr

# M4KK1独特的快速系统调用
.global m4k_fast_syscall
m4k_fast_syscall:
    # PowerPC特定的快速系统调用
    li r0, 0x4D             # M4KK1独特的环境调用号
    sc
    blr