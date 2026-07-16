; M4KK1 Bootcamp Stage 1 Bootloader
; 这是引导加载程序的第一阶段，负责硬件检测和内核加载准备

BITS 16
ORG 0x7C00

; 常量定义
STAGE2_ADDR     equ 0x8000
STAGE2_SIZE     equ 0x1000
KERNEL_ADDR     equ 0x100000
STACK_ADDR      equ 0x7B00

; 魔数和标志
M4KK1_MAGIC     equ 0x4D344B4B    ; "M4KK" in hex
BOOT_SIGNATURE  equ 0xAA55

start:
    ; 初始化段寄存器
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_ADDR

    ; 保存驱动器号
    mov [drive_number], dl

    ; 检查是否为M4KK1引导扇区
    mov eax, M4KK1_MAGIC
    cmp eax, M4KK1_MAGIC
    jne boot_error

    ; 显示加载消息
    mov si, msg_loading
    call print_string

    ; 加载第二阶段引导加载程序
    call load_stage2

    ; 跳转到第二阶段
    jmp STAGE2_ADDR

; 简化的内存检测
detect_memory:
    pusha
    ; 使用INT 0x12获取低端内存大小
    int 0x12
    jc .error
    mov [low_memory_kb], ax
    popa
    ret
.error:
    mov si, msg_disk_error
    call print_string
    popa
    ret

; 加载第二阶段引导加载程序
load_stage2:
    pusha
    ; 重置磁盘控制器
    mov ah, 0x00
    mov dl, [drive_number]
    int 0x13
    jc .error

    ; 读取第二阶段到内存
    mov ah, 0x02           ; 读取扇区
    mov al, STAGE2_SIZE / 512 + 1  ; 读取扇区数
    mov ch, 0              ; 柱面号
    mov cl, 2              ; 扇区号（从第2扇区开始）
    mov dh, 0              ; 磁头号
    mov dl, [drive_number] ; 驱动器号
    mov bx, STAGE2_ADDR    ; 目标地址
    int 0x13
    jc .error

    ; 验证第二阶段魔数
    mov eax, [STAGE2_ADDR]
    cmp eax, M4KK1_MAGIC
    jne .error

    popa
    ret

.error:
    mov si, msg_disk_error
    call print_string
    jmp hang

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

; 错误处理
boot_error:
    mov si, msg_boot_error
    call print_string
    jmp hang

hang:
    cli
    hlt
    jmp hang

; 数据区
drive_number        db 0
low_memory_kb       dw 0

; 消息字符串
msg_loading         db "M4KK1 Loading...", 13, 10, 0
msg_boot_error      db "Boot error!", 13, 10, 0
msg_disk_error      db "Disk error!", 13, 10, 0

; 魔数和引导标志
dd M4KK1_MAGIC

; 填充到512字节并添加引导标志
times 510 - ($ - $$) db 0
dw BOOT_SIGNATURE