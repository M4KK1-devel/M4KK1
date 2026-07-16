; M4KK1 x86_64 Architecture - Global Descriptor Table
; 独特的M4KK1 GDT实现

BITS 64

; M4KK1独特的GDT条目结构
struc m4k_gdt_entry
    .limit_low      resw 1      ; 段限制低16位
    .base_low       resw 1      ; 基地址低16位
    .base_mid       resb 1      ; 基地址中8位
    .access         resb 1      ; 访问权限
    .limit_high     resb 1      ; 限制高4位和标志
    .base_high      resb 1      ; 基地址高8位
endstruc

; M4KK1独特的GDT指针结构
struc m4k_gdt_ptr_struct
    .limit          resw 1      ; GDT限制
    .base           resq 1      ; GDT基地址（64位）
endstruc

; M4KK1独特的访问权限标志
%define M4K_GDT_PRESENT         (1 << 7)
%define M4K_GDT_DPL_0           (0 << 5)
%define M4K_GDT_DPL_1           (1 << 5)
%define M4K_GDT_DPL_2           (2 << 5)
%define M4K_GDT_DPL_3           (3 << 5)
%define M4K_GDT_CODE            (1 << 4) | (1 << 3) | (1 << 1)
%define M4K_GDT_DATA            (1 << 4) | (1 << 1)
%define M4K_GDT_LONG_MODE       (1 << 5)  ; M4KK1独特的长模式标志

; M4KK1独特的段选择子
%define M4K_KERNEL_CODE         0x08
%define M4K_KERNEL_DATA         0x10
%define M4K_USER_CODE           0x18
%define M4K_USER_DATA           0x20

section .data
align 16
; M4KK1独特的GDT表
m4k_gdt:
    ; 空描述符
    dq 0x0000000000000000

    ; M4KK1独特的内核代码段（64位长模式）
    dw 0xFFFF               ; 限制低16位
    dw 0x0000               ; 基地址低16位
    db 0x00                 ; 基地址中8位
    db M4K_GDT_PRESENT | M4K_GDT_DPL_0 | M4K_GDT_CODE | M4K_GDT_LONG_MODE
    db 0xAF                 ; 粒度4K，64位模式
    db 0x00                 ; 基地址高8位

    ; M4KK1独特的数据段
    dw 0xFFFF               ; 限制低16位
    dw 0x0000               ; 基地址低16位
    db 0x00                 ; 基地址中8位
    db M4K_GDT_PRESENT | M4K_GDT_DPL_0 | M4K_GDT_DATA
    db 0xCF                 ; 粒度4K，32位模式
    db 0x00                 ; 基地址高8位

    ; M4KK1独特的用户代码段
    dw 0xFFFF               ; 限制低16位
    dw 0x0000               ; 基地址低16位
    db 0x00                 ; 基地址中8位
    db M4K_GDT_PRESENT | M4K_GDT_DPL_3 | M4K_GDT_CODE | M4K_GDT_LONG_MODE
    db 0xAF                 ; 粒度4K，64位模式
    db 0x00                 ; 基地址高8位

    ; M4KK1独特的数据段
    dw 0xFFFF               ; 限制低16位
    dw 0x0000               ; 基地址低16位
    db 0x00                 ; 基地址中8位
    db M4K_GDT_PRESENT | M4K_GDT_DPL_3 | M4K_GDT_DATA
    db 0xCF                 ; 粒度4K，32位模式
    db 0x00                 ; 基地址高8位

m4k_gdt_end:

; M4KK1独特的GDT指针
m4k_gdt_ptr:
    dw m4k_gdt_end - m4k_gdt - 1
    dq m4k_gdt

section .text
global m4k_gdt_load
global m4k_gdt_set_gate

; M4KK1独特的GDT加载函数
m4k_gdt_load:
    lgdt [m4k_gdt_ptr]

    ; M4KK1独特的数据段重载
    mov ax, M4K_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; M4KK1独特的远跳转到64位代码段
    push M4K_KERNEL_CODE
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ret

; M4KK1独特的数据段获取函数
global m4k_get_cs
m4k_get_cs:
    mov ax, cs
    ret

global m4k_get_ds
m4k_get_ds:
    mov ax, ds
    ret

global m4k_get_es
m4k_get_es:
    mov ax, es
    ret

global m4k_get_fs
m4k_get_fs:
    mov ax, fs
    ret

global m4k_get_gs
m4k_get_gs:
    mov ax, gs
    ret

global m4k_get_ss
m4k_get_ss:
    mov ax, ss
    ret

; M4KK1独特的数据段设置函数
global m4k_set_ds
m4k_set_ds:
    mov ax, di
    mov ds, ax
    ret

global m4k_set_es
m4k_set_es:
    mov ax, di
    mov es, ax
    ret

global m4k_set_fs
m4k_set_fs:
    mov ax, di
    mov fs, ax
    ret

global m4k_set_gs
m4k_set_gs:
    mov ax, di
    mov gs, ax
    ret