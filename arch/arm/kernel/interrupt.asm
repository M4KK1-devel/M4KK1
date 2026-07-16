; M4KK1 ARM Architecture - Interrupt Handling
; 独特的中断处理实现 (32位)

.section ".text"

.global m4k_interrupt_handler
.extern m4k_process_interrupt

; M4KK1独特的中断处理函数
m4k_interrupt_handler:
    ; 保存所有寄存器
    stmdb sp!, {r0-r12, lr}

    ; 调用C中断处理函数
    bl m4k_process_interrupt

    ; 恢复所有寄存器
    ldmia sp!, {r0-r12, lr}

    ; 返回
    subs pc, lr, #4

; M4KK1独特的中断注册函数
.global m4k_register_interrupt
m4k_register_interrupt:
    ; 参数：r0 = 中断号, r1 = 处理函数
    mov r0, #0              ; 成功
    mov pc, lr