# M4KK1 PowerPC Architecture - Interrupt Handling
# 独特的中断处理实现

.section ".text"

.global m4k_interrupt_handler
.extern m4k_process_interrupt

# M4KK1独特的中断处理函数
m4k_interrupt_handler:
    # 保存所有寄存器
    stwu r1, -512(r1)       # 栈帧
    stmw r0, 8(r1)          # 保存所有寄存器

    # 调用C中断处理函数
    bl m4k_process_interrupt

    # 恢复所有寄存器
    lmw r0, 8(r1)
    addi r1, r1, 512

    # 返回
    rfi

# M4KK1独特的中断注册函数
.global m4k_register_interrupt
m4k_register_interrupt:
    # 参数：r3 = 中断号, r4 = 处理函数
    li r3, 0                # 成功
    blr