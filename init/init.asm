; M4KK1 4P1 - init.asm
; Description: Minimal init process ELF binary for M4KK1.
;
; Copyright (c) 2026 Yaku Makki
; SPDX-License-Identifier: 4P1-Custom

BITS 32
GLOBAL _start

section .text
_start:
    mov esi, hello_msg
.lp:
    lodsb
    test al, al
    jz .done
    mov dx, 0x3F8
    out dx, al
    jmp .lp
.done:
.idle:
    hlt
    jmp .idle

section .data
hello_msg: db "Hello from init!", 10, 0
