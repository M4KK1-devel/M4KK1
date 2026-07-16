; M4KK1 x86_64 Architecture - Memory Paging
; 独特的内存分页实现

BITS 64

; M4KK1独特的分页标志
%define M4K_PAGE_PRESENT    (1 << 0)
%define M4K_PAGE_WRITE      (1 << 1)
%define M4K_PAGE_USER       (1 << 2)
%define M4K_PAGE_PWT        (1 << 3)
%define M4K_PAGE_PCD        (1 << 4)
%define M4K_PAGE_ACCESSED   (1 << 5)
%define M4K_PAGE_DIRTY      (1 << 6)
%define M4K_PAGE_HUGE       (1 << 7)  ; M4KK1独特的大页面标志
%define M4K_PAGE_GLOBAL     (1 << 8)
%define M4K_PAGE_NX         (1 << 63) ; M4KK1独特的NX位

section .data
align 4096
; M4KK1独特的PML4表
m4k_pml4:
    times 512 dq 0

; M4KK1独特的PDP表
m4k_pdp:
    times 512 dq 0

; M4KK1独特的页目录
m4k_page_dir:
    times 512 dq 0

section .text
global m4k_paging_init
global m4k_map_page
global m4k_unmap_page

; M4KK1独特的分页初始化
m4k_paging_init:
    ; 设置M4KK1独特的PML4
    mov rax, m4k_pdp
    or rax, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_USER
    mov [m4k_pml4], rax

    ; 设置M4KK1独特的PDP
    mov rax, m4k_page_dir
    or rax, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_USER
    mov [m4k_pdp], rax

    ; 设置M4KK1独特的页目录（映射前2MB）
    mov rcx, 0
.map_loop:
    mov rax, rcx
    shl rax, 21             ; 2MB * 条目号
    or rax, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_HUGE | M4K_PAGE_GLOBAL
    mov [m4k_page_dir + rcx * 8], rax

    inc rcx
    cmp rcx, 512
    jl .map_loop

    ; 加载M4KK1独特的页表
    mov rax, m4k_pml4
    mov cr3, rax

    ret

; M4KK1独特的页映射函数
m4k_map_page:
    ; 参数：rdi = 虚拟地址, rsi = 物理地址, rdx = 标志
    push rbx
    push rcx
    push rdx

    ; 计算页表索引
    mov rax, rdi
    shr rax, 39             ; PML4索引
    and rax, 0x1FF
    mov rbx, m4k_pml4
    mov rbx, [rbx + rax * 8]

    ; 检查PML4条目是否存在
    test rbx, M4K_PAGE_PRESENT
    jz .create_pdp

    ; 获取PDP地址
    and rbx, 0xFFFFFFFFFF000
    jmp .check_pdp

.create_pdp:
    ; 创建新的PDP表
    call m4k_alloc_page
    or rbx, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_USER
    mov [m4k_pml4 + rax * 8], rbx

    mov rbx, rax
    shl rbx, 12

.check_pdp:
    ; PDP索引
    mov rax, rdi
    shr rax, 30
    and rax, 0x1FF
    mov rcx, rbx
    mov rbx, [rcx + rax * 8]

    ; 检查PDP条目是否存在
    test rbx, M4K_PAGE_PRESENT
    jz .create_page_dir

    ; 获取页目录地址
    and rbx, 0xFFFFFFFFFF000
    jmp .check_page_dir

.create_page_dir:
    ; 创建新的页目录
    call m4k_alloc_page
    or rbx, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_USER
    mov [rcx + rax * 8], rbx

    mov rbx, rax
    shl rbx, 12

.check_page_dir:
    ; 页目录索引
    mov rax, rdi
    shr rax, 21
    and rax, 0x1FF
    mov rcx, rbx
    mov rbx, [rcx + rax * 8]

    ; 检查页目录条目是否存在
    test rbx, M4K_PAGE_PRESENT
    jz .create_page_table

    ; 获取页表地址
    and rbx, 0xFFFFFFFFFF000
    jmp .map_page_entry

.create_page_table:
    ; 创建新的页表
    call m4k_alloc_page
    or rbx, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_USER
    mov [rcx + rax * 8], rbx

    mov rbx, rax
    shl rbx, 12

.map_page_entry:
    ; 页表索引
    mov rax, rdi
    shr rax, 12
    and rax, 0x1FF

    ; 设置页表条目
    mov rcx, rsi
    or rcx, M4K_PAGE_PRESENT | M4K_PAGE_WRITE | M4K_PAGE_GLOBAL
    and rcx, ~0xFFF         ; 清除低12位
    or rcx, rdx             ; 应用标志

    mov [rbx + rax * 8], rcx

    pop rdx
    pop rcx
    pop rbx
    ret

; M4KK1独特的页取消映射函数
m4k_unmap_page:
    ; 参数：rdi = 虚拟地址
    push rbx
    push rcx

    ; 页表索引
    mov rax, rdi
    shr rax, 12
    and rax, 0x1FF

    ; 清零页表条目
    mov qword [rbx + rax * 8], 0

    ; 刷新TLB
    invlpg [rdi]

    pop rcx
    pop rbx
    ret

; M4KK1独特的页分配函数
m4k_alloc_page:
    ; 返回分配的页地址在rax中
    ; 这里应该实现实际的页分配逻辑
    mov rax, 0x100000       ; 临时返回固定地址
    ret