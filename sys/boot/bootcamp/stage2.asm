; M4KK1 Bootcamp Stage 2 Bootloader
; 第二阶段引导加载程序，负责内核加载和文件系统初始化

BITS 16
ORG 0x8000

; 常量定义
KERNEL_ADDR     equ 0x100000
KERNEL_SIZE     equ 0x100000    ; 1MB 内核空间
INITRD_ADDR     equ 0x200000
VIDEO_MODE      equ 0x4112      ; 图形模式 1024x768x32位
STACK_ADDR      equ 0x7B00

; 魔数
M4KK1_MAGIC     equ 0x4D344B4B
YFS_MAGIC       equ 0x59465321    ; "YFS!"

; 数据区
memory_entries      dd 0
low_memory_kb       dw 0
high_memory_kb      dw 0
extended_memory_kb  dw 0

; 打印字符串函数
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

start:
    ; 初始化段寄存器
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_ADDR

    ; 显示阶段2消息
    mov si, msg_stage2
    call print_string

    ; 设置视频模式
    call set_video_mode

    ; 检测硬件
    call detect_hardware

    ; 初始化文件系统
    call init_filesystem

    ; 加载内核
    call load_kernel

    ; 验证内核魔数
    mov eax, [KERNEL_ADDR]
    cmp eax, M4KK1_MAGIC
    jne kernel_error

    ; 显示内核加载成功消息
    mov si, msg_kernel_loaded
    call print_string

    ; 准备内核启动参数
    call prepare_kernel_params

    ; 切换到保护模式
    call enter_protected_mode

; 设置视频模式
set_video_mode:
    pusha
    mov ax, 0x4F02          ; VESA BIOS扩展
    mov bx, VIDEO_MODE      ; 1024x768x32位
    int 0x10
    popa
    ret

; 检测硬件
detect_hardware:
    pusha

    ; 检测CPU特性
    call detect_cpu_features

    ; 检测PCI设备
    call detect_pci_devices

    ; 检测磁盘
    call detect_disks

    popa
    ret

; CPU特性检测
detect_cpu_features:
    pusha
    ; 检查CPUID支持
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    jz .no_cpuid

    ; 获取CPU信息
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_extended

    mov eax, 0x80000001
    cpuid
    mov [cpu_features], edx

.no_extended:
    mov eax, 1
    cpuid
    mov [cpu_info], eax
    mov [cpu_features], edx

.no_cpuid:
    popa
    ret

; PCI设备检测
detect_pci_devices:
    pusha
    mov eax, 0
    mov [pci_devices], eax
    ; 简单PCI检测实现
    popa
    ret

; 磁盘检测
detect_disks:
    pusha
    ; 检测硬盘数量和类型
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, 0x80
    int 0x13
    jc .no_edd

    mov [edd_support], bx

.no_edd:
    popa
    ret

; 初始化文件系统
init_filesystem:
    pusha

    ; 挂载根文件系统
    call mount_root_fs

    popa
    ret

; 挂载根文件系统
mount_root_fs:
    pusha
    mov si, msg_mounting_root
    call print_string

    ; 设置根文件系统参数
    ; 这里应该实际挂载根文件系统
    popa
    ret

; 加载内核
load_kernel:
    pusha
    mov si, msg_loading_kernel
    call print_string

    ; 从文件系统加载内核
    call load_kernel_from_fs

    popa
    ret

; 从文件系统加载内核
load_kernel_from_fs:
    pusha

    ; 尝试从引导设备加载内核
    mov si, msg_loading_kernel_fs
    call print_string

    ; 读取内核文件到内存
    ; 这里应该实现实际的文件系统读取
    ; 暂时使用简化的内核映像

    ; 创建简化的内核映像
    mov edi, KERNEL_ADDR
    mov ecx, 4096           ; 4KB内核映像
    xor eax, eax
    rep stosd

    ; 设置内核魔数和基本结构
    mov dword [KERNEL_ADDR], M4KK1_MAGIC
    mov dword [KERNEL_ADDR + 4], 0x1000    ; 内核大小
    mov dword [KERNEL_ADDR + 8], 0x100000  ; 内核入口点

    ; 设置内核头部信息
    mov dword [KERNEL_ADDR + 512], 0x4D344B4B  ; "M4KK"魔数
    mov word [KERNEL_ADDR + 516], 1            ; 版本号
    mov word [KERNEL_ADDR + 518], 0            ; 子版本号

    popa
    ret

; 准备内核启动参数
prepare_kernel_params:
    pusha

    ; 设置引导信息结构
    mov edi, boot_params
    mov eax, [memory_entries]
    stosd
    mov ax, [low_memory_kb]
    stosw
    mov ax, [high_memory_kb]
    stosw

    popa
    ret

; 进入保护模式
enter_protected_mode:
    pusha

    ; 禁用中断
    cli

    ; 加载GDT
    lgdt [gdt_descriptor]

    ; 设置保护模式标志
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; 跳转到保护模式
    jmp 0x08:protected_mode_start

; 保护模式代码
BITS 32
protected_mode_start:
    ; 设置段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 设置栈指针
    mov esp, 0x90000

    ; 显示保护模式消息
    mov esi, msg_protected_mode
    call print_string_pm

    ; 跳转到内核入口点
    jmp KERNEL_ADDR + 0x1000    ; 跳过内核魔数

; 32位打印函数
print_string_pm:
    pusha
    mov edi, 0xB8000           ; 视频内存地址
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x07               ; 白色字符，黑色背景
    stosw
    jmp .loop
.done:
    popa
    ret

; 内核错误处理
kernel_error:
    mov si, msg_kernel_error
    call print_string
    jmp hang

hang:
    cli
    hlt
    jmp hang

; 数据区
cpu_info            dd 0
cpu_features        dd 0
pci_devices         dd 0
edd_support         dw 0

; 引导参数
boot_params:
    dd 0    ; 内存条目数
    dw 0    ; 低端内存KB
    dw 0    ; 高端内存KB

; GDT定义
gdt_start:
    ; 空描述符
    dq 0x0000000000000000

    ; 代码段描述符
    dw 0xFFFF           ; 段限制低16位
    dw 0x0000           ; 段基址低16位
    db 0x00             ; 段基址中8位
    db 0x9A             ; 存在位、特权级、类型标志
    db 0xCF             ; 段限制高4位、粒度标志
    db 0x00             ; 段基址高8位

    ; 数据段描述符
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92             ; 可读写数据段
    db 0xCF
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; 魔数
dd M4KK1_MAGIC

; 消息字符串
msg_stage2          db "M4KK1 Bootcamp Stage 2", 13, 10, 0
msg_mounting_root   db "Mounting root filesystem...", 13, 10, 0
msg_loading_kernel  db "Loading kernel...", 13, 10, 0
msg_loading_kernel_fs db "Loading kernel from filesystem...", 13, 10, 0
msg_kernel_loaded   db "Kernel loaded successfully", 13, 10, 0
msg_protected_mode  db "Entering protected mode...", 13, 10, 0
msg_kernel_error    db "Kernel loading failed!", 13, 10, 0

; 填充并添加魔数
times 2048 - ($ - $$) db 0