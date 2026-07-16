; M4KK1 x86_64 Architecture - System Call Handling
; 独特的系统调用处理实现

BITS 64

section .text
extern syscall_handler

; M4KK1独特系统调用入口点
global m4k_syscall_entry
m4k_syscall_entry:
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

    ; 调用C系统调用处理函数
    call syscall_handler

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

; M4KK1独特系统调用处理函数
global m4k_syscall_handler
m4k_syscall_handler:
    ; 参数：系统调用号在RAX中
    push rbp
    mov rbp, rsp

    ; 调用C系统调用处理函数
    call syscall_handler

    pop rbp
    ret

; M4KK1独特系统调用注册函数
global m4k_register_syscall
m4k_register_syscall:
    ; 参数：rdi = 系统调用号, rsi = 处理函数
    mov rax, 0              ; 成功
    ret