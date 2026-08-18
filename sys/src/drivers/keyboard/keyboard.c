/*
 * M4KK1 4P1 - keyboard.c
 * Description: PS/2 keyboard driver — scancode
 *              processing, keymap lookup, LED control.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "keyboard.h"
#include "../../include/console.h"
#include "../../include/kernel.h"
#include "../../include/idt.h"
#include <stdint.h>
#include <stdbool.h>
#include "../../include/string.h"

/* PIC 中断屏蔽控制 */
extern void pic_unmask_irq(uint32_t irq_num);

#define KEYBOARD_DATA_PORT      0x60
#define KEYBOARD_STATUS_PORT    0x64
#define KEYBOARD_COMMAND_PORT   0x64

#define KEYBOARD_CMD_DISABLE    0xAD
#define KEYBOARD_CMD_ENABLE     0xAE
#define KEYBOARD_CMD_READ       0xD0
#define KEYBOARD_CMD_WRITE      0xD1
#define KEYBOARD_CMD_SELF_TEST  0xAA
#define KEYBOARD_CMD_INTERFACE_TEST 0xAB

#define KEYBOARD_ACK            0xFA
#define KEYBOARD_RESEND         0xFE
#define KEYBOARD_TEST_OK        0x55

#define KEYBOARD_STATUS_OBF     0x01
#define KEYBOARD_STATUS_IBF     0x02
#define KEYBOARD_STATUS_SYS     0x04
#define KEYBOARD_STATUS_CMD     0x08
#define KEYBOARD_STATUS_LOCK    0x10
#define KEYBOARD_STATUS_MIN     0x20
#define KEYBOARD_STATUS_TO      0x40
#define KEYBOARD_STATUS_PAR     0x80

#define KEYBOARD_LED_SCROLL     0x01
#define KEYBOARD_LED_NUM        0x02
#define KEYBOARD_LED_CAPS       0x04

#define KEYBOARD_MOD_SHIFT      0x0001
#define KEYBOARD_MOD_CTRL       0x0002
#define KEYBOARD_MOD_ALT        0x0004
#define KEYBOARD_MOD_CAPS       0x0100
#define KEYBOARD_MOD_NUM        0x0200
#define KEYBOARD_MOD_SCROLL     0x0400

#define SCANCODE_SET_1          1
#define SCANCODE_SET_2          2
#define SCANCODE_SET_3          3
#define KEYBOARD_BUFFER_SIZE    256

typedef struct {
    bool     initialized;
    uint8_t  scancode_set;
    uint8_t  led_status;
    bool     extended_mode;
    uint8_t  buffer[KEYBOARD_BUFFER_SIZE];
    uint32_t buffer_head;
    uint32_t buffer_tail;
    bool     shift_pressed;
    bool     ctrl_pressed;
    bool     alt_pressed;
    bool     caps_lock;
    bool     num_lock;
    bool     scroll_lock;
} keyboard_state_t;

static keyboard_state_t keyboard_state;

static const char keymap_lower[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e',
    'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k',
    'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char keymap_upper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E',
    'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K',
    'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static inline void
outb(uint16_t u16Port, uint8_t u8Value)
{
    __asm__ volatile("outb %0, %1"
                     :
                     : "a"(u8Value), "Nd"(u16Port));
}

static inline uint8_t
inb(uint16_t u16Port)
{
    uint8_t u8Value;
    __asm__ volatile("inb %1, %0"
                     : "=a"(u8Value)
                     : "Nd"(u16Port));
    return u8Value;
}

static bool
keyboard_wait_ready(void)
{
    uint32_t u32Timeout = 100000;
    while (u32Timeout--) {
        uint8_t u8Status =
            inb(KEYBOARD_STATUS_PORT);
        if (!(u8Status & KEYBOARD_STATUS_IBF))
            return true;
    }
    return false;
}

static bool
keyboard_send_command(uint8_t u8Command)
{
    if (!keyboard_wait_ready())
        return false;
    outb(KEYBOARD_COMMAND_PORT, u8Command);
    return true;
}

static bool
keyboard_send_data(uint8_t u8Data)
{
    if (!keyboard_wait_ready())
        return false;
    outb(KEYBOARD_DATA_PORT, u8Data);
    return true;
}

static uint8_t
keyboard_read_data(void)
{
    uint32_t timeout = 100000;
    while ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OBF) == 0) {
        if (--timeout == 0) return 0;  // 超时返回0
    }
    return inb(KEYBOARD_DATA_PORT);
}

static uint8_t
keyboard_wait_response(void)
{
    uint32_t u32Timeout = 100000;
    while (u32Timeout--) {
        if (inb(KEYBOARD_STATUS_PORT)
            & KEYBOARD_STATUS_OBF)
            return inb(KEYBOARD_DATA_PORT);
    }
    return 0;
}

static void
keyboard_set_leds(uint8_t u8Leds)
{
    if (keyboard_send_data(0xED)) {
        uint8_t u8Resp = keyboard_wait_response();
        if (u8Resp == KEYBOARD_ACK)
            keyboard_send_data(u8Leds);
    }
}

void
mkrn_keyboard_handler(void)
{
    uint8_t u8Scancode = keyboard_read_data();
    uint8_t u8KeyCode = 0;
    bool bPressed = true;

    if (u8Scancode == 0xE0) {
        keyboard_state.extended_mode = true;
        return;
    }

    if (u8Scancode & 0x80) {
        bPressed = false;
        u8Scancode &= 0x7F;
    }

    if (keyboard_state.extended_mode) {
        u8KeyCode = u8Scancode + 128;
        keyboard_state.extended_mode = false;
    } else {
        u8KeyCode = u8Scancode;
    }

    switch (u8KeyCode) {
    case 0x2A:
    case 0x36:
        keyboard_state.shift_pressed = bPressed;
        break;
    case 0x1D:
        keyboard_state.ctrl_pressed = bPressed;
        break;
    case 0x38:   /* set-1 left Alt */
    case 0x64:   /* set-2 right Alt (extended) */
        keyboard_state.alt_pressed = bPressed;
        break;
    case 0x3A:
        if (!bPressed) {
            keyboard_state.caps_lock =
                !keyboard_state.caps_lock;
            keyboard_set_leds(
                (keyboard_state.caps_lock
                     ? KEYBOARD_LED_CAPS
                     : 0)
                | (keyboard_state.num_lock
                       ? KEYBOARD_LED_NUM
                       : 0)
                | (keyboard_state.scroll_lock
                       ? KEYBOARD_LED_SCROLL
                       : 0));
        }
        break;
    case 0x45:
        if (!bPressed) {
            keyboard_state.num_lock =
                !keyboard_state.num_lock;
            keyboard_set_leds(
                (keyboard_state.caps_lock
                     ? KEYBOARD_LED_CAPS
                     : 0)
                | (keyboard_state.num_lock
                       ? KEYBOARD_LED_NUM
                       : 0)
                | (keyboard_state.scroll_lock
                       ? KEYBOARD_LED_SCROLL
                       : 0));
        }
        break;
    case 0x46:
        if (!bPressed) {
            keyboard_state.scroll_lock =
                !keyboard_state.scroll_lock;
            keyboard_set_leds(
                (keyboard_state.caps_lock
                     ? KEYBOARD_LED_CAPS
                     : 0)
                | (keyboard_state.num_lock
                       ? KEYBOARD_LED_NUM
                       : 0)
                | (keyboard_state.scroll_lock
                       ? KEYBOARD_LED_SCROLL
                       : 0));
        }
        break;
    }

    if (u8KeyCode >= 128) {
        /* E0-extended keys that the desktop needs: PageUp / PageDown
         * (terminal scrollback).  Push private control codes into the
         * ASCII buffer so m4k_get_keyboard_event() delivers them;
         * 0x01/0x02 never collide with printable ASCII or Ctrl+C. */
        if (bPressed) {
            uint8_t ext = u8Scancode;   /* base code, no make/break bit */
            char ch = 0;
            if (ext == 0x49)      ch = 0x01;   /* PageUp   */
            else if (ext == 0x51) ch = 0x02;   /* PageDown */
            if (ch) {
                uint32_t u32NextTail =
                    (keyboard_state.buffer_tail + 1)
                    % KEYBOARD_BUFFER_SIZE;
                if (u32NextTail
                    != keyboard_state.buffer_head)
                {
                    keyboard_state.buffer
                        [keyboard_state.buffer_tail] = ch;
                    keyboard_state.buffer_tail =
                        u32NextTail;
                }
            }
        }
        return;
    }

    char ch = 0;
    if (keyboard_state.shift_pressed
        ^ keyboard_state.caps_lock)
        ch = keymap_upper[u8KeyCode];
    else
        ch = keymap_lower[u8KeyCode];

    /* Push ONLY on make (press).  Key release used to push the same
     * char again, doubling every keystroke ("root" became "rrooott")
     * and toggling two-state handlers (Tab switched fields twice and
     * landed back where it started). */
    if (ch != 0 && bPressed) {
        uint32_t u32NextTail =
            (keyboard_state.buffer_tail + 1)
            % KEYBOARD_BUFFER_SIZE;
        if (u32NextTail
            != keyboard_state.buffer_head)
        {
            keyboard_state.buffer
                [keyboard_state.buffer_tail] = ch;
            keyboard_state.buffer_tail =
                u32NextTail;
        }
    }
}

char
mkrn_keyboard_get_char(void)
{
    if (keyboard_state.buffer_head
        == keyboard_state.buffer_tail)
        return 0;

    char ch =
        keyboard_state.buffer
            [keyboard_state.buffer_head];
    keyboard_state.buffer_head =
        (keyboard_state.buffer_head + 1)
        % KEYBOARD_BUFFER_SIZE;
    return ch;
}

bool
mkrn_keyboard_has_char(void)
{
    return keyboard_state.buffer_head
           != keyboard_state.buffer_tail;
}

void
mkrn_kbd_init(void)
{
    M4K_LOG_INFO("Initializing keyboard driver...");

    mkrn_memset(&keyboard_state, 0, sizeof(keyboard_state));
    keyboard_state.scancode_set = SCANCODE_SET_1;
    keyboard_state.num_lock = true;
    keyboard_state.buffer_head = 0;
    keyboard_state.buffer_tail = 0;

    /* 清空输出缓冲区 */
    uint32_t timeout = 100;
    while ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OBF) && timeout--)
        inb(KEYBOARD_DATA_PORT);

    /* 注册中断处理程序 */
    mkrn_idt_register_handler(0x21, (mkrn_int_handler_t)mkrn_keyboard_handler);

    /* 取消屏蔽 IRQ1 */
    pic_unmask_irq(1);

    keyboard_state.initialized = true;
    M4K_LOG_INFO("Keyboard driver initialized");
}

bool
mkrn_keyboard_is_initialized(void)
{
    return keyboard_state.initialized;
}

uint32_t
mkrn_keyboard_get_modifiers(void)
{
    uint32_t u32Mods = 0;
    if (keyboard_state.shift_pressed)
        u32Mods |= KEYBOARD_MOD_SHIFT;
    if (keyboard_state.ctrl_pressed)
        u32Mods |= KEYBOARD_MOD_CTRL;
    if (keyboard_state.alt_pressed)
        u32Mods |= KEYBOARD_MOD_ALT;
    if (keyboard_state.caps_lock)
        u32Mods |= KEYBOARD_MOD_CAPS;
    if (keyboard_state.num_lock)
        u32Mods |= KEYBOARD_MOD_NUM;
    if (keyboard_state.scroll_lock)
        u32Mods |= KEYBOARD_MOD_SCROLL;
    return u32Mods;
}
