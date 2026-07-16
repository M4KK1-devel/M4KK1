; M4KK1 x86_64 Architecture - Interrupt Handling
; 独特的中断处理实现

BITS 64

; M4KK1独特的中断栈结构
struc m4k_interrupt_frame
    .rip            resq 1      ; 指令指针
    .cs             resq 1      ; 代码段
    .rflags         resq 1      ; 标志寄存器
    .rsp            resq 1      ; 栈指针
    .ss             resq 1      ; 栈段
endstruc

section .text
extern m4k_interrupt_handler

; M4KK1独特的中断入口点
global m4k_interrupt_entry
m4k_interrupt_entry:
    ; 保存所有寄存器
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 调用C中断处理函数
    mov rdi, rsp            ; 传递栈指针
    call m4k_interrupt_handler

    ; 恢复所有寄存器
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 返回
    iretq

; M4KK1独特的中断处理函数
global m4k_interrupt_handler_impl
m4k_interrupt_handler_impl:
    ; 参数：rdi = 栈指针
    push rbp
    mov rbp, rsp

    ; 获取中断号
    mov rsi, [rdi + 8]      ; CS段
    and rsi, 0xFF           ; 低8位是中断号

    ; 根据中断号调用相应处理函数
    cmp rsi, 0x4D           ; M4KK1独特系统调用
    je .syscall_handler

    ; 其他中断处理
    ; TODO: 实现其他中断处理

.syscall_handler:
    ; 调用系统调用处理
    extern m4k_syscall_handler
    call m4k_syscall_handler

    pop rbp
    ret

; M4KK1独特的中断注册函数
global m4k_register_interrupt
m4k_register_interrupt:
    ; 参数：rdi = 中断号, rsi = 处理函数
    mov rax, 0              ; 成功
    ret