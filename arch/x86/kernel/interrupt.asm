; M4KK1 x86 Architecture - Interrupt Handling
; 独特的中断处理实现 (32位)

BITS 32

section .text
extern m4k_interrupt_handler

; M4KK1独特的中断入口点
global m4k_interrupt_entry
m4k_interrupt_entry:
    ; 保存所有寄存器
    pusha

    ; 调用C中断处理函数
    call m4k_interrupt_handler

    ; 恢复所有寄存器
    popa

    ; 返回
    iret

; M4KK1独特的中断处理函数
global m4k_interrupt_handler_impl
m4k_interrupt_handler_impl:
    push ebp
    mov ebp, esp

    ; 获取中断号（从栈中）
    mov eax, [ebp + 12]     ; 中断号在栈中

    ; 根据中断号调用相应处理函数
    cmp eax, 0x4D           ; M4KK1独特系统调用
    je .syscall_handler

    ; 其他中断处理
    ; TODO: 实现其他中断处理

.syscall_handler:
    ; 调用系统调用处理
    extern m4k_syscall_handler
    call m4k_syscall_handler

    pop ebp
    ret

; M4KK1独特的中断注册函数
global m4k_register_interrupt
m4k_register_interrupt:
    ; 参数：栈中传递中断号和处理函数
    mov eax, 0              ; 成功
    ret