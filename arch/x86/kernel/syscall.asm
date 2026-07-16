; M4KK1 x86 Architecture - System Call Handling
; 独特的系统调用处理实现 (32位)

BITS 32

section .text
extern syscall_handler

; M4KK1独特系统调用入口点
global m4k_syscall_entry
m4k_syscall_entry:
    ; 保存所有寄存器
    pusha

    ; 调用C系统调用处理函数
    call syscall_handler

    ; 恢复所有寄存器
    popa

    ; 返回
    iret

; M4KK1独特系统调用处理函数
global m4k_syscall_handler
m4k_syscall_handler:
    ; 参数：系统调用号在EAX中
    push ebp
    mov ebp, esp

    ; 调用C系统调用处理函数
    call syscall_handler

    pop ebp
    ret

; M4KK1独特系统调用注册函数
global m4k_register_syscall
m4k_register_syscall:
    ; 参数：栈中传递系统调用号和处理函数
    mov eax, 0              ; 成功
    ret