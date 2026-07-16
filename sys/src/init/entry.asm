; M4KK1 4P1 - entry.asm
; Description: Boot entry point for Kernel init.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

BITS 32

; Multiboot header - must be within first 8192 bytes
SECTION .multiboot
align 4
    dd 0x1BADB002          ; magic
    dd 0x00000003          ; flags: page align + memory info
    dd -(0x1BADB002 + 0x00000003) ; checksum
    dd 0x00000000          ; header_addr
    dd 0x00000000          ; load_addr
    dd 0x00000000          ; load_end_addr
    dd 0x00000000          ; bss_end_addr
    dd 0x00000000          ; entry_addr

SECTION .text

; External symbol declarations
extern mkrn_main
extern __bss_start
extern __bss_end
extern __stack_top

; Kernel magic number
KERNEL_MAGIC equ 0x4D344B4B

; Multiboot protocol info structure
struc multiboot_info
    .flags          resd 1
    .mem_lower      resd 1
    .mem_upper      resd 1
    .boot_device    resd 1
    .cmdline        resd 1
    .mods_count     resd 1
    .mods_addr      resd 1
    .syms           resd 4
    .mmap_length    resd 1
    .mmap_addr      resd 1
    .drives_length  resd 1
    .drives_addr    resd 1
    .config_table   resd 1
    .boot_loader_name resd 1
    .apm_table      resd 1
    .vbe_control_info resd 1
    .vbe_mode_info  resd 1
    .vbe_mode       resw 1
    .vbe_interface_seg resw 1
    .vbe_interface_off resw 1
    .vbe_interface_len resw 1
endstruc

; Kernel entry point
GLOBAL _start
_start:
    ; Save multiboot info pointer
    mov edi, ebx

    ; Save magic verification
    cmp eax, 0x2BADB002
    jne .invalid_magic

    ; Set stack pointer
    mov esp, __stack_top

    ; Clear BSS section
    call clear_bss

    ; Save multiboot info (kmain params: magic first, mb_info second)
    push eax            ; magic
    push edi            ; mb_info
    call mkrn_main

    ; If kmain returns, enter infinite loop
.halt:
    cli
    hlt
    jmp .halt

.invalid_magic:
    ; Invalid magic number, infinite loop
    jmp .halt

; Clear BSS section
clear_bss:
    push eax
    push ecx
    push edi
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    pop edi
    pop ecx
    pop eax
    ret

; Kernel magic number
SECTION .rodata
    dd KERNEL_MAGIC

; Stack space
SECTION .bootstrap_stack, nobits
    align 4096
    resb 32768          ; 32KB stack space
__stack_top:
