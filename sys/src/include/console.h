/*
 * M4KK1 4P1 - console.h
 * Description: Console input/output function declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#define M4K_VGA_COLOR_BLACK         0x0
#define M4K_VGA_COLOR_BLUE          0x1
#define M4K_VGA_COLOR_GREEN         0x2
#define M4K_VGA_COLOR_CYAN          0x3
#define M4K_VGA_COLOR_RED           0x4
#define M4K_VGA_COLOR_MAGENTA       0x5
#define M4K_VGA_COLOR_BROWN         0x6
#define M4K_VGA_COLOR_LIGHT_GRAY    0x7
#define M4K_VGA_COLOR_DARK_GRAY     0x8
#define M4K_VGA_COLOR_LIGHT_BLUE    0x9
#define M4K_VGA_COLOR_LIGHT_GREEN   0xA
#define M4K_VGA_COLOR_LIGHT_CYAN    0xB
#define M4K_VGA_COLOR_LIGHT_RED     0xC
#define M4K_VGA_COLOR_LIGHT_MAGENTA 0xD
#define M4K_VGA_COLOR_YELLOW        0xE
#define M4K_VGA_COLOR_WHITE         0xF

#define M4K_VGA_TEXT_BUFFER 0xB8000
#define M4K_VGA_WIDTH       80
#define M4K_VGA_HEIGHT      25

#define M4K_CONSOLE_COLOR_DEFAULT (M4K_VGA_COLOR_LIGHT_GRAY | (M4K_VGA_COLOR_BLACK << 4))

void mkrn_console_init(void);
void mkrn_console_clear(void);
void mkrn_console_set_cursor(u8 x, u8 y);
void mkrn_console_get_cursor(u8 *x, u8 *y);
void mkrn_console_put_char(char c);
void mkrn_console_write(const char *str);
void mkrn_console_write_at(char c, u8 x, u8 y, u8 color);
void mkrn_console_write_string_at(const char *str, u8 x, u8 y, u8 color);
void mkrn_console_write_hex(u32 value);
void mkrn_console_write_dec(u32 value);
void mkrn_console_write_bin(u32 value);
void mkrn_console_set_color(u8 color);
u8 mkrn_console_get_color(void);
void mkrn_console_scroll(void);
void mkrn_console_backspace(void);
void mkrn_console_newline(void);
void mkrn_console_tab(void);
void mkrn_console_printf(const char *format, ...);
void mkrn_console_set_screen_color(u8 background, u8 foreground);
void mkrn_console_panic(const char *message);
void mkrn_console_memory_error(const char *message);
void mkrn_console_system_error(const char *message);
