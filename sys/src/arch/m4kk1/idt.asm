; M4KK1 Architecture - Interrupt Descriptor Table
; 中断描述符表汇编实现

BITS 32

; IDT描述符结构
struc idt_entry
    .offset_low     resw 1      ; 中断处理函数低16位地址
    .selector       resw 1      ; 段选择子
    .zero           resb 1      ; 保留，必须为0
    .flags          resb 1      ; 标志位
    .offset_high    resw 1      ; 中断处理函数高16位地址
endstruc

; IDT指针结构
struc idt_ptr_struct
    .limit          resw 1      ; IDT限制
    .base           resd 1      ; IDT基址
endstruc

; 中断标志
%define IDT_PRESENT     (1 << 7)    ; 存在位
%define IDT_DPL_0       (0 << 5)    ; 特权级0
%define IDT_DPL_1       (1 << 5)    ; 特权级1
%define IDT_DPL_2       (2 << 5)    ; 特权级2
%define IDT_DPL_3       (3 << 5)    ; 特权级3
%define IDT_GATE_386    0x0E        ; 32位中断门 (S=0, type=0xE)
%define IDT_TRAP_386    0x0F        ; 32位陷阱门 (S=0, type=0xF)

; 段选择子常量
%define KERNEL_CODE_SEG 0x08
%define KERNEL_DATA_SEG 0x10

; 中断向量号
%define IRQ_BASE        0x20        ; IRQ基础向量号
%define IRQ0_TIMER      0x20        ; 定时器中断
%define IRQ1_KEYBOARD   0x21        ; 键盘中断
%define IRQ2_CASCADE    0x22        ; 级联中断
%define IRQ3_COM2       0x23        ; 串口2
%define IRQ4_COM1       0x24        ; 串口1
%define IRQ5_LPT2       0x25        ; 并口2
%define IRQ6_FLOPPY     0x26        ; 软驱
%define IRQ7_LPT1       0x27        ; 并口1
%define IRQ8_RTC        0x28        ; 实时时钟
%define IRQ9_ACPI       0x29        ; ACPI
%define IRQ10_SCI       0x2A        ; SCI
%define IRQ11_USB       0x2B        ; USB
%define IRQ12_PS2       0x2C        ; PS/2鼠标
%define IRQ13_FPU       0x2D        ; FPU协处理器
%define IRQ14_IDE1      0x2E        ; IDE主通道
%define IRQ15_IDE2      0x2F        ; IDE从通道

SECTION .data

; 中断描述符表 (256个条目)
idt:
    times 256 dd 0, 0          ; 初始化为0

; IDT指针
idt_ptr:
    dw idt_end - idt - 1        ; IDT限制
    dd idt                      ; IDT基址

idt_end:

SECTION .text

; 加载IDT
GLOBAL idt_load
idt_load:
    lidt [idt_ptr]              ; 加载IDT指针
    ret

; 设置IDT条目
GLOBAL idt_set_gate
idt_set_gate:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; 获取向量号
    mov ebx, [ebp + 12]         ; 获取处理函数地址
    mov ecx, [ebp + 16]         ; 获取段选择子
    mov edx, [ebp + 20]         ; 获取标志

    ; 计算IDT条目地址
    imul eax, 8                 ; 每个条目8字节
    add eax, idt

    ; 设置偏移地址低16位
    mov word [eax], bx

    ; 设置段选择子
    mov word [eax + 2], cx

    ; 设置标志和保留字节
    mov byte [eax + 4], 0x00
    mov byte [eax + 5], dl

    ; 设置偏移地址高16位
    shr ebx, 16
    mov word [eax + 6], bx

    pop ebp
    ret

; 获取中断向量号（从栈中）
GLOBAL get_interrupt_vector
get_interrupt_vector:
    mov eax, [esp + 4]          ; 从栈中获取向量号
    ret

; 设置PIC主从片初始化
GLOBAL pic_init
pic_init:
    pusha

    ; 保存PIC掩码
    in al, 0x21
    mov byte [pic_master_mask], al
    in al, 0xA1
    mov byte [pic_slave_mask], al

    ; 初始化主PIC (ICW1)
    mov al, 0x11                ; ICW1: 级联、需要ICW4
    out 0x20, al

    ; 初始化从PIC (ICW1)
    mov al, 0x11                ; ICW1: 级联、需要ICW4
    out 0xA0, al

    ; 主PIC ICW2: 中断向量基址
    mov al, IRQ_BASE
    out 0x21, al

    ; 从PIC ICW2: 中断向量基址
    mov al, IRQ_BASE + 8
    out 0xA1, al

    ; 主PIC ICW3: 连接到从PIC的IRQ2
    mov al, 0x04
    out 0x21, al

    ; 从PIC ICW3: 连接到主PIC的IRQ2
    mov al, 0x02
    out 0xA1, al

    ; 主PIC ICW4: 8086模式、正常EOI
    mov al, 0x01
    out 0x21, al

    ; 从PIC ICW4: 8086模式、正常EOI
    mov al, 0x01
    out 0xA1, al

    ; 恢复PIC掩码
    mov al, [pic_master_mask]
    out 0x21, al
    mov al, [pic_slave_mask]
    out 0xA1, al

    popa
    ret

; 发送PIC EOI
GLOBAL pic_send_eoi
pic_send_eoi:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; 获取IRQ号

    ; 如果IRQ >= 8，发送EOI到从PIC
    cmp eax, 8
    jl .master_only

    ; 发送EOI到从PIC
    mov al, 0x20
    out 0xA0, al

.master_only:
    ; 发送EOI到主PIC
    mov al, 0x20
    out 0x20, al

    pop ebp
    ret


; 启用中断
GLOBAL enable_interrupts
enable_interrupts:
    sti
    ret

; 禁用中断
GLOBAL disable_interrupts
disable_interrupts:
    cli
    ret

; 检查中断状态
GLOBAL interrupts_enabled
interrupts_enabled:
    pushf                       ; 压入标志寄存器
    pop eax                     ; 弹出到EAX
    and eax, 0x200              ; 检查IF位
    shr eax, 9                  ; 右移9位
    ret

; 初始化IDT
GLOBAL idt_init
idt_init:
    pusha

    ; 设置默认中断处理函数
    mov eax, isr_default
    mov ebx, KERNEL_CODE_SEG
    mov ecx, IDT_PRESENT | IDT_DPL_0 | IDT_GATE_386

    ; 设置前32个异常处理函数
    mov edx, 0                  ; 从向量0开始
.loop_exceptions:
    push ecx
    push ebx
    push eax
    push edx
    call idt_set_gate
    add esp, 16

    inc edx
    cmp edx, 32
    jl .loop_exceptions

    ; 设置IRQ处理函数
    mov eax, irq_default
    mov ecx, IDT_PRESENT | IDT_DPL_0 | IDT_GATE_386

    ; 设置IRQ 0-15
    mov edx, IRQ_BASE           ; 从IRQ_BASE开始
.loop_irqs:
    ; 为定时器中断（IRQ0）使用专用处理函数
    cmp edx, IRQ0_TIMER
    jne .use_default

    mov eax, irq_timer
    jmp .set_gate

.use_default:
    mov eax, irq_default

.set_gate:
    push ecx
    push ebx
    push eax
    push edx
    call idt_set_gate
    add esp, 16

    inc edx
    cmp edx, IRQ_BASE + 16
    jl .loop_irqs

    ; 加载IDT
    call idt_load

    ; 重新启用中断
    sti

    popa
    ret

; 默认ISR处理函数
GLOBAL isr_default
isr_default:
    pusha

    ; 显示异常信息
    mov esi, exception_msg
    call print_string

    ; 获取中断向量号
    mov eax, [esp + 32]         ; 从栈中获取向量号

    ; 显示向量号
    mov esi, vector_msg
    call print_string
    call print_hex

    mov esi, newline
    call print_string

    ; 停止系统
    cli
    hlt
    jmp $

; 默认IRQ处理函数
GLOBAL irq_default
irq_default:
    pusha

    ; 发送EOI到主PIC
    mov al, 0x20
    out 0x20, al

    ; 检查是否为从PIC的中断
    mov al, 0x0B
    out 0xA0, al
    in al, 0xA0

    ; 如果是从PIC的中断，发送EOI到从PIC
    test al, 0x80
    jz .no_slave_eoi

    mov al, 0x20
    out 0xA0, al

.no_slave_eoi:
    popa
    iret

; 定时器IRQ处理函数
GLOBAL irq_timer
irq_timer:
    pusha


    call mkrn_timer_handler

    ; 发送EOI到主PIC
    mov al, 0x20
    out 0x20, al

    popa
    iret

; 系统调用中断处理函数 (int 0x80)
GLOBAL isr_syscall
isr_syscall:
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    call mkrn_syscall_handler

    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    iret

; M4KK1 系统调用中断处理函数 (int 0x4D)
; Push order: reversed so saved_regs[0..4] = {EBX, ECX, EDX, ESI, EDI}
; Stack layout (low→high): EAX, EBX, ECX, EDX, ESI, EDI, EBP
GLOBAL isr_m4k_syscall
isr_m4k_syscall:
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax                ; top of saved regs = EAX (syscall number)

    lea eax, [esp+4]        ; pointer to EBX (2nd from top)
    push eax                ; 2nd arg: saved_regs points to EBX
    push dword [esp+4]      ; 1st arg: syscall_num = EAX (top of saved regs)

    call m4k_syscall_handler
    add esp, 8              ; pop two args

    add esp, 4              ; skip saved EAX (return value is already in EAX)
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp
    iret

; 屏蔽IRQ
GLOBAL pic_mask_irq
pic_mask_irq:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; 获取IRQ号

    cmp eax, 8
    jl .mask_master

    ; 屏蔽从PIC的IRQ
    sub eax, 8
    mov ecx, eax
    in al, 0xA1
    bts eax, ecx                ; 设置屏蔽位
    out 0xA1, al
    jmp .done

.mask_master:
    ; 屏蔽主PIC的IRQ
    mov ecx, eax
    in al, 0x21
    bts eax, ecx                ; 设置屏蔽位
    out 0x21, al

.done:
    pop ebp
    ret

; 取消屏蔽IRQ
GLOBAL pic_unmask_irq
pic_unmask_irq:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; 获取IRQ号

    cmp eax, 8
    jl .unmask_master

    ; 取消屏蔽从PIC的IRQ
    sub eax, 8
    mov ecx, eax
    in al, 0xA1
    btc eax, ecx                ; 清除屏蔽位
    out 0xA1, al
    jmp .done

.unmask_master:
    ; 取消屏蔽主PIC的IRQ
    mov ecx, eax
    in al, 0x21
    btc eax, ecx                ; 清除屏蔽位
    out 0x21, al

.done:
    pop ebp
    ret

SECTION .data

pic_master_mask db 0
pic_slave_mask  db 0

SECTION .rodata

exception_msg   db "Exception occurred! Vector: 0x", 0
vector_msg      db "0x", 0
newline         db 13, 10, 0
idt_msg         db "M4KK1 IDT Information:", 13, 10, 0
idt_base_msg    db "  Base: 0x", 0
idt_limit_msg   db "  Limit: 0x", 0

; 外部函数声明
extern print_string
extern print_hex
extern mkrn_timer_handler
extern mkrn_syscall_handler
extern m4k_syscall_handler


; 获取IDT信息
GLOBAL idt_get_info
idt_get_info:
    mov eax, idt_ptr        ; 返回IDT指针地址
    ret

; 转储IDT信息（调试用）
GLOBAL idt_dump
idt_dump:
    pusha

    mov esi, idt_msg
    call print_string

    ; 显示IDT基址和限制
    mov esi, idt_base_msg
    call print_string
    mov eax, idt
    call print_hex

    mov esi, idt_limit_msg
    call print_string
    mov eax, idt_end - idt - 1
    call print_hex

    mov esi, newline
    call print_string

    popa
    ret

; 获取当前IRQ状态
GLOBAL get_current_irq
get_current_irq:
    ; 从栈中获取IRQ号（如果在IRQ处理函数中）
    mov eax, [esp + 12]     ; 中断向量号 - 0x20 = IRQ号
    sub eax, IRQ_BASE
    ret

; 设置中断标志
GLOBAL set_interrupt_flag
set_interrupt_flag:
    mov eax, [esp + 4]      ; 获取标志值
    test eax, eax
    jz .disable
    sti                     ; 启用中断
    ret
.disable:
    cli                     ; 禁用中断
    ret

; 原子操作：测试并设置
GLOBAL atomic_test_and_set
atomic_test_and_set:
    mov eax, [esp + 8]      ; 获取内存地址
    mov ebx, [esp + 4]      ; 获取期望值
    mov ecx, [esp + 12]     ; 获取新值
    lock cmpxchg [eax], ecx ; 原子比较和交换
    ret

; 原子操作：加法
GLOBAL atomic_add
atomic_add:
    mov eax, [esp + 8]      ; 获取内存地址
    mov ebx, [esp + 4]      ; 获取增量值
    lock add [eax], ebx     ; 原子加法
    ret

; 原子操作：减法
GLOBAL atomic_sub
atomic_sub:
    mov eax, [esp + 8]      ; 获取内存地址
    mov ebx, [esp + 4]      ; 获取减量值
    lock sub [eax], ebx     ; 原子减法
    ret

; 外部函数声明
extern print_string
extern print_hex
extern mkrn_timer_handler
extern mkrn_syscall_handler