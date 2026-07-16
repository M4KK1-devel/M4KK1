; M4KK1 Architecture - Global Descriptor Table
; 全局描述符表汇编实现

BITS 32

; GDT描述符结构
struc gdt_entry
    .limit_low      resw 1      ; 段限制低16位
    .base_low       resw 1      ; 段基址低16位
    .base_mid       resb 1      ; 段基址中8位
    .access         resb 1      ; 访问权限字节
    .limit_high     resb 1      ; 段限制高4位 + 标志
    .base_high      resb 1      ; 段基址高8位
endstruc

; GDT指针结构
struc gdt_ptr
    .limit          resw 1      ; GDT限制
    .base           resd 1      ; GDT基址
endstruc

; 段选择子常量
%define KERNEL_CODE_SEG 0x08
%define KERNEL_DATA_SEG 0x10
%define USER_CODE_SEG   0x18
%define USER_DATA_SEG   0x20
%define TSS_SEG         0x28

; GDT访问权限标志
%define GDT_PRESENT     (1 << 7)
%define GDT_DPL_0       (0 << 5)    ; 特权级0
%define GDT_DPL_1       (1 << 5)    ; 特权级1
%define GDT_DPL_2       (2 << 5)    ; 特权级2
%define GDT_DPL_3       (3 << 5)    ; 特权级3
%define GDT_SEGMENT     (1 << 4)    ; 代码/数据段
%define GDT_CODE        (1 << 3)    ; 可执行代码段
%define GDT_DATA        (0 << 3)    ; 数据段
%define GDT_READWRITE   (1 << 1)    ; 可读写
%define GDT_GROW_DOWN   (1 << 2)    ; 向下扩展

; GDT粒度标志
%define GDT_GRANULARITY (1 << 7)    ; 4KB粒度
%define GDT_32BIT       (1 << 6)    ; 32位保护模式
%define GDT_64BIT       (1 << 5)    ; 64位代码段

SECTION .data

; 全局描述符表
gdt:
    ; 空描述符 (必须存在)
    dd 0x00000000
    dd 0x00000000

    ; 内核代码段
    dw 0xFFFF           ; 限制低16位
    dw 0x0000           ; 基址低16位
    db 0x00             ; 基址中8位
    db GDT_PRESENT | GDT_DPL_0 | GDT_SEGMENT | GDT_CODE | GDT_READWRITE
    db GDT_GRANULARITY | GDT_32BIT | 0x0F
    db 0x00             ; 基址高8位

    ; 内核数据段
    dw 0xFFFF           ; 限制低16位
    dw 0x0000           ; 基址低16位
    db 0x00             ; 基址中8位
    db GDT_PRESENT | GDT_DPL_0 | GDT_SEGMENT | GDT_DATA | GDT_READWRITE
    db GDT_GRANULARITY | GDT_32BIT | 0x0F
    db 0x00             ; 基址高8位

    ; 用户代码段
    dw 0xFFFF           ; 限制低16位
    dw 0x0000           ; 基址低16位
    db 0x00             ; 基址中8位
    db GDT_PRESENT | GDT_DPL_3 | GDT_SEGMENT | GDT_CODE | GDT_READWRITE
    db GDT_GRANULARITY | GDT_32BIT | 0x0F
    db 0x00             ; 基址高8位

    ; 用户数据段
    dw 0xFFFF           ; 限制低16位
    dw 0x0000           ; 基址低16位
    db 0x00             ; 基址中8位
    db GDT_PRESENT | GDT_DPL_3 | GDT_SEGMENT | GDT_DATA | GDT_READWRITE
    db GDT_GRANULARITY | GDT_32BIT | 0x0F
    db 0x00             ; 基址高8位

    ; TSS段 (任务状态段)
    dw 0x0067           ; 限制低16位 (103字节)
    dw 0x0000           ; 基址低16位
    db 0x00             ; 基址中8位
    db GDT_PRESENT | GDT_DPL_0 | 0x09  ; TSS描述符
    db 0x00             ; 粒度和限制高4位
    db 0x00             ; 基址高8位

gdt_end:

; GDT指针
gdt_ptr_data:
    dw gdt_end - gdt - 1    ; GDT限制
    dd gdt                  ; GDT基址

SECTION .text

; 加载GDT (参数: GDT指针地址)
GLOBAL gdt_load
gdt_load:
    mov eax, [esp + 4]      ; 获取GDT指针参数
    lgdt [eax]              ; 加载GDT指针

    ; 重新加载段寄存器
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 远跳转刷新CS
    jmp KERNEL_CODE_SEG:.flush_cs

.flush_cs:
    ret

; 获取GDT信息
GLOBAL gdt_get_info
gdt_get_info:
    mov eax, gdt_ptr_data   ; 返回GDT指针地址
    ret

; 设置TSS段基址
GLOBAL gdt_set_tss_base
gdt_set_tss_base:
    mov eax, [esp + 4]      ; 获取TSS基址参数

    ; 更新TSS段基址 (基址低16位)
    mov word [gdt + TSS_SEG + 2], ax

    ; 更新TSS段基址 (基址中8位)
    mov byte [gdt + TSS_SEG + 4], ah

    ; 更新TSS段基址 (基址高8位)
    shr eax, 16
    mov byte [gdt + TSS_SEG + 7], al
    mov byte [gdt + TSS_SEG + 8], ah

    ret

; 设置TSS段限制
GLOBAL gdt_set_tss_limit
gdt_set_tss_limit:
    mov eax, [esp + 4]      ; 获取TSS限制参数

    ; 更新TSS段限制
    mov word [gdt + TSS_SEG], ax

    ret

; 转储GDT信息（调试用）
GLOBAL gdt_dump
gdt_dump:
    pusha

    mov esi, gdt_msg
    call print_string

    ; 显示GDT基址和限制
    mov esi, gdt_base_msg
    call print_string
    mov eax, gdt
    call print_hex

    mov esi, gdt_limit_msg
    call print_string
    mov eax, gdt_end - gdt - 1
    call print_hex

    mov esi, newline
    call print_string

    popa
    ret

SECTION .rodata

gdt_msg         db "M4KK1 GDT Information:", 13, 10, 0
gdt_base_msg    db "  Base: 0x", 0
gdt_limit_msg   db "  Limit: 0x", 0
newline         db 13, 10, 0

; 刷新TSS
GLOBAL gdt_flush
gdt_flush:
    mov eax, [esp + 4]      ; 获取TSS基址参数
    ltr ax                  ; 加载任务寄存器
    ret

; 获取当前段寄存器值
GLOBAL get_cs
get_cs:
    mov eax, cs
    ret

GLOBAL get_ds
get_ds:
    mov eax, ds
    ret

GLOBAL get_es
get_es:
    mov eax, es
    ret

GLOBAL get_fs
get_fs:
    mov eax, fs
    ret

GLOBAL get_gs
get_gs:
    mov eax, gs
    ret

GLOBAL get_ss
get_ss:
    mov eax, ss
    ret

; 设置段寄存器
GLOBAL set_ds
set_ds:
    mov eax, [esp + 4]
    mov ds, ax
    ret

GLOBAL set_es
set_es:
    mov eax, [esp + 4]
    mov es, ax
    ret

GLOBAL set_fs
set_fs:
    mov eax, [esp + 4]
    mov fs, ax
    ret

GLOBAL set_gs
set_gs:
    mov eax, [esp + 4]
    mov gs, ax
    ret

; 外部函数声明
extern print_string
extern print_hex