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
    mov eax, isr_default
    mov ebx, KERNEL_CODE_SEG
    mov ecx, IDT_PRESENT | IDT_DPL_0 | IDT_GATE_386
    push ecx
    push ebx
    push eax
    push edx
    call idt_set_gate
    mov edx, [esp]              ; 恢复循环计数器 (idt_set_gate 会修改 EDX)
    add esp, 16

    inc edx
    cmp edx, 32
    jl .loop_exceptions

    ; 设置IRQ处理函数
    mov edx, IRQ_BASE           ; 从IRQ_BASE开始
.loop_irqs:
    ; 每轮重新加载参数 (idt_set_gate 会修改 EAX/EBX/ECX/EDX)
    mov eax, irq_default
    mov ebx, KERNEL_CODE_SEG
    mov ecx, IDT_PRESENT | IDT_DPL_0 | IDT_GATE_386

    ; 为定时器中断（IRQ0）使用专用处理函数
    cmp edx, IRQ0_TIMER
    jne .use_default

    mov eax, irq_timer

.use_default:
    push ecx
    push ebx
    push eax
    push edx
    call idt_set_gate
    mov edx, [esp]              ; 恢复循环计数器 (idt_set_gate 会修改 EDX)
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

    ; ── Minimal exception dump (keep tiny: a broken dump here loops) ──
    mov eax, dbg_msg_x
    push eax
    call mkrn_console_write
    add esp, 4
    mov eax, [esp + 36]        ; EIP (no-err layout) or SS (priv-change)
    push eax
    call mkrn_console_write_hex
    add esp, 4
    mov eax, dbg_msg_cr2
    push eax
    call mkrn_console_write
    add esp, 4
    mov eax, cr2
    push eax
    call mkrn_console_write_hex
    add esp, 4
    mov eax, dbg_msg_y
    push eax
    call mkrn_console_write
    add esp, 4

    popa

    ; 停止系统
    cli
    hlt
    jmp $

; 默认IRQ处理函数
; 现在调用C语言处理程序 mkrn_idt_handle_irq()
GLOBAL irq_default
irq_default:
    pusha

    ; 确定IRQ号 - 先检查从PIC
    mov al, 0x0B        ; OCW3: 读取ISR
    out 0xA0, al
    in al, 0xA0
    test al, al
    jnz .slave_irq

    ; 检查主PIC
    mov al, 0x0B        ; OCW3: 读取ISR
    out 0x20, al
    in al, 0x20

    ; 找到触发的IRQ位 - 先零扩展al到eax
    movzx eax, al       ; 将 al 零扩展到 eax
    bsf eax, eax
    jz .no_irq          ; 没有IRQ，可能是伪中断

    ; IRQ号在eax中 (0-7 对应主PIC)
    push eax
    call mkrn_idt_handle_irq
    add esp, 4
    jmp .done

.slave_irq:
    ; 从PIC中断 - 先零扩展al到eax
    movzx eax, al       ; 将 al 零扩展到 eax
    bsf eax, eax
    jz .no_irq
    add eax, 8          ; 从PIC IRQ是 8-15
    push eax
    call mkrn_idt_handle_irq
    add esp, 4

.done:
    popa
    iret

.no_irq:
    ; 伪中断或无IRQ，直接返回
    popa
    iret

; 定时器IRQ处理函数
GLOBAL irq_timer
irq_timer:
    pusha

    ; 传递中断帧指针（pusha 保存的 edi 槽位）给 C 处理函数，
    ; 帧内 [0..7]=edi,esi,ebp,esp,ebx,edx,ecx,eax [8]=eip [9]=cs [10]=eflags
    mov eax, esp
    push eax
    call mkrn_timer_handler
    add esp, 4

    ; 发送EOI到主PIC
    mov al, 0x20
    out 0x20, al

    popa
    iret

; 系统调用中断处理函数 (int 0x80)
GLOBAL isr_syscall
isr_syscall:
    push eax                ; preserve syscall number across frame capture
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    ; Save user frame for fork(): g_syscall_user_frame = {eip, cs, eflags, esp, ss, ebp}
    ; Stack now: [esp]=ebp, +4=edi, +8=esi, +12=edx, +16=ecx, +20=ebx, +24=eax,
    ;            +28=user_eip, +32=cs, +36=eflags, +40=esp, +44=ss
    ; NOTE: use the symbol as an immediate (array address), not
    ; dword [sym] which would load the array's first element instead.
    mov eax, g_syscall_user_frame
    mov ecx, [esp+28]
    mov [eax], ecx
    mov ecx, [esp+32]
    mov [eax+4], ecx
    mov ecx, [esp+36]
    mov [eax+8], ecx
    mov ecx, [esp+40]
    mov [eax+12], ecx
    mov ecx, [esp+44]
    mov [eax+16], ecx
    mov ecx, [esp+0]          ; user EBP (pushed first, bottom of block)
    mov [eax+20], ecx

    ; Restore the syscall number into EAX: the frame capture above
    ; clobbered the register, and mkrn_syscall_handler reads the number
    ; from EAX at entry.
    mov eax, [esp+24]

    ; Restore user argument registers: the frame capture above used
    ; ECX as scratch, so the user's ECX (syscall arg2) was lost and
    ; mkrn_syscall_handler would read the user's EBP as arg2.  Restore
    ; all argument registers from the saved block before calling the
    ; handler (which reads args from EBX/ECX/EDX/ESI/EDI).
    mov ebx, [esp+20]
    mov ecx, [esp+16]
    mov edx, [esp+12]
    mov esi, [esp+8]
    mov edi, [esp+4]

    call mkrn_syscall_handler

    ; Cooperative scheduling: give other ready processes (e.g. a forked
    ; child) a chance to run before returning to user mode.  EAX holds
    ; the syscall return value, so save it across the switch.
    push eax
    call mkrn_process_yield
    pop eax

    ; NOTE: this ISR pushes EAX first (bottom of the saved block, [esp+24]),
    ; unlike isr_m4k_syscall which pushes it last.  Restore in the order the
    ; regs were pushed, then skip EAX.
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    add esp, 4              ; skip saved EAX (syscall number, bottom of block)
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

    ; Save user frame for fork(): g_syscall_user_frame = {eip, cs, eflags, esp, ss, ebp}
    ; Stack now: [esp]=eax, +4=ebx, +8=ecx, +12=edx, +16=esi, +20=edi, +24=ebp,
    ;            +28=user_eip, +32=cs, +36=eflags, +40=esp, +44=ss
    ; NOTE: use the symbol as an immediate (array address), not
    ; dword [sym] which would load the array's first element instead.
    mov eax, g_syscall_user_frame
    mov ecx, [esp+28]
    mov [eax], ecx
    mov ecx, [esp+32]
    mov [eax+4], ecx
    mov ecx, [esp+36]
    mov [eax+8], ecx
    mov ecx, [esp+40]
    mov [eax+12], ecx
    mov ecx, [esp+44]
    mov [eax+16], ecx
    mov ecx, [esp+24]         ; user EBP (pushed last, at +24)
    mov [eax+20], ecx

    lea eax, [esp+4]        ; pointer to EBX (2nd from top)
    push eax                ; 2nd arg: saved_regs points to EBX
    push dword [esp+4]      ; 1st arg: syscall_num = EAX (top of saved regs)

    call m4k_syscall_handler
    add esp, 8              ; pop two args

    ; Cooperative scheduling: give other ready processes (e.g. a forked
    ; child) a chance to run before returning to user mode.  EAX holds
    ; the syscall return value, so save it across the switch.
    push eax
    call mkrn_process_yield
    pop eax

    add esp, 4              ; skip saved EAX (syscall number at entry)
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
    btr eax, ecx                ; 清除屏蔽位 (bit RESET, not toggle!)
    out 0xA1, al
    jmp .done

.unmask_master:
    ; 取消屏蔽主PIC的IRQ
    mov ecx, eax
    in al, 0x21
    btr eax, ecx                ; 清除屏蔽位 (bit RESET, not toggle!)
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
dbg_msg_x       db "[EXC] vec=", 0
dbg_exc         db "[EXC] ", 0
dbg_msg_eip     db " eip=", 0
dbg_msg_eip2    db " eip2=", 0
dbg_msg_sp      db " ", 0
dbg_msg_eip3    db " eip3=", 0
dbg_msg_cr2     db " cr2=", 0
dbg_msg_cs      db " cs=", 0
dbg_msg_y       db "\r\n", 0
idt_msg         db "M4KK1 IDT Information:", 13, 10, 0
idt_base_msg    db "  Base: 0x", 0
idt_limit_msg   db "  Limit: 0x", 0

; 外部函数声明
extern print_string
extern print_hex
extern mkrn_timer_handler
extern mkrn_syscall_handler
extern m4k_syscall_handler
extern mkrn_idt_handle_irq
extern g_syscall_user_frame
extern mkrn_process_yield
extern mkrn_console_write
extern mkrn_console_write_hex


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