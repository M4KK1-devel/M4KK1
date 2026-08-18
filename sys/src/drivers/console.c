/*
 * M4KK1 4P1 - console.c
 * Description: VGA text-mode console driver with
 *              serial (COM1) support.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../include/console.h"

#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_SIZE        (VGA_WIDTH * VGA_HEIGHT)
#define VGA_MEMORY      0xB8000
#define VGA_COMMAND_PORT 0x3D4
#define VGA_DATA_PORT   0x3D5

typedef struct {
    uint16_t *buffer;
    uint8_t  cursor_x;
    uint8_t  cursor_y;
    uint8_t  text_color;
    uint8_t  background_color;
    bool     initialized;
} console_state_t;

static console_state_t console_state;

static inline void
vga_write_register(uint16_t u16Reg, uint8_t u8Value)
{
    __asm__ volatile(
        "outb %%al, %%dx"
        :
        : "a"(u8Value), "d"(u16Reg));
}

static inline uint8_t
vga_read_register(uint16_t u16Reg)
{
    uint8_t u8Value;
    __asm__ volatile(
        "inb %%dx, %%al"
        : "=a"(u8Value)
        : "d"(u16Reg));
    return u8Value;
}

static void
vga_set_cursor_position(uint16_t u16Pos)
{
    vga_write_register(VGA_COMMAND_PORT, 0x0E);
    vga_write_register(VGA_DATA_PORT,
                       (uint8_t)(u16Pos >> 8));
    vga_write_register(VGA_COMMAND_PORT, 0x0F);
    vga_write_register(VGA_DATA_PORT,
                       (uint8_t)u16Pos);
}

void
mkrn_console_clear(void)
{
    if (!console_state.initialized)
        return;

    uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;
    uint16_t u16Color =
        (console_state.background_color << 4)
        | console_state.text_color;
    uint16_t u16Blank = 0x20 | (u16Color << 8);

    for (int i = 0; i < VGA_SIZE; i++)
        pBuffer[i] = u16Blank;

    console_state.cursor_x = 0;
    console_state.cursor_y = 0;
    vga_set_cursor_position(0);
}

void
mkrn_console_scroll(void)
{
    if (!console_state.initialized)
        return;

    uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;

    /* Single overlapping up-copy + one-row clear: one VGA pass
     * instead of per-word C loop (rep movsl, 2 words per dword). */
    uint32_t u32Words =
        (uint32_t)VGA_WIDTH * (VGA_HEIGHT - 1);
    int bOddTail = (int)(u32Words & 1u);
    uint32_t u32Dwords = u32Words >> 1;
    uint16_t const *pu16Src = pBuffer + VGA_WIDTH;
    uint16_t *pu16Dst = pBuffer;
    __asm__ volatile("cld; rep movsl"
        : "+c"(u32Dwords), "+D"(pu16Dst), "+S"(pu16Src)
        :
        : "memory");
    if (bOddTail)
        *pu16Dst = *pu16Src;

    uint16_t u16Color =
        (console_state.background_color << 4)
        | console_state.text_color;
    uint16_t u16Blank = 0x20 | (u16Color << 8);
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1);
         i < VGA_SIZE; i++)
        pBuffer[i] = u16Blank;

    if (console_state.cursor_y > 0)
        console_state.cursor_y--;
}

void
mkrn_console_write_at(char c, uint8_t u8X,
                      uint8_t u8Y, uint8_t u8Color)
{
    if (!console_state.initialized)
        return;

    if (u8X >= VGA_WIDTH || u8Y >= VGA_HEIGHT)
        return;

    uint32_t u32Pos = u8Y * VGA_WIDTH + u8X;
    if (u32Pos >= VGA_SIZE)
        return;

    uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;
    uint16_t u16VgaColor =
        (u8Color & 0xF0) | ((u8Color & 0x0F) >> 4);
    pBuffer[u32Pos] = c | (u16VgaColor << 8);
}

void
mkrn_console_write_string_at(const char *pStr,
                             uint8_t u8X, uint8_t u8Y,
                             uint8_t u8Color)
{
    if (!console_state.initialized || pStr == NULL)
        return;

    uint8_t u8OrigX = u8X;
    while (*pStr) {
        if (u8X >= VGA_WIDTH) {
            u8X = u8OrigX;
            u8Y++;
            if (u8Y >= VGA_HEIGHT)
                break;
        }
        mkrn_console_write_at(*pStr++, u8X++, u8Y,
                              u8Color);
    }
}

uint8_t
mkrn_console_get_color(void)
{
    if (!console_state.initialized)
        return 0;
    return (console_state.background_color << 4)
           | console_state.text_color;
}

void
mkrn_console_backspace(void)
{
    if (!console_state.initialized)
        return;

    if (console_state.cursor_x > 0) {
        console_state.cursor_x--;
        uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;
        uint32_t u32Pos =
            console_state.cursor_y * VGA_WIDTH
            + console_state.cursor_x;
        uint16_t u16Color =
            (console_state.background_color << 4)
            | console_state.text_color;
        pBuffer[u32Pos] = 0x20 | (u16Color << 8);
        vga_set_cursor_position(
            console_state.cursor_y * VGA_WIDTH
            + console_state.cursor_x);
    }
}

void
mkrn_console_tab(void)
{
    if (!console_state.initialized)
        return;

    uint8_t u8Spaces =
        4 - (console_state.cursor_x % 4);
    for (uint8_t i = 0; i < u8Spaces; i++)
        mkrn_console_put_char(' ');
}

static __attribute__((noinline)) void
serial_init(void)
{
    __asm__ volatile(
        "mov $0x00, %%al\n"
        "mov $0x3F9, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0x80, %%al\n"
        "mov $0x3FB, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0x01, %%al\n"
        "mov $0x3F8, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0x00, %%al\n"
        "mov $0x3F9, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0x03, %%al\n"
        "mov $0x3FB, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0xC7, %%al\n"
        "mov $0x3FA, %%dx\n"
        "out %%al, %%dx\n"
        "mov $0x0B, %%al\n"
        "mov $0x3FC, %%dx\n"
        "out %%al, %%dx\n"
        :
        :
        : "eax", "edx", "memory");
}

static __attribute__((noinline)) void
serial_putchar(char c)
{
    unsigned int timeout = 10000;
    __asm__ volatile(
        "1:\n"
        "mov $0x3FD, %%dx\n"
        "in %%dx, %%al\n"
        "test $0x20, %%al\n"
        "jnz 2f\n"
        "decl %0\n"
        "jz 2f\n"
        "jmp 1b\n"
        "2:\n"
        "movzbl %1, %%eax\n"
        "mov $0x3F8, %%dx\n"
        "out %%al, %%dx\n"
        : "+r"(timeout)
        : "rm"((unsigned char)c)
        : "eax", "edx", "memory");
}

void
mkrn_console_init(void)
{
    serial_init();
    serial_putchar('\n');
    serial_putchar('\n');

    console_state.buffer = (uint16_t *)VGA_MEMORY;
    console_state.cursor_x = 0;
    console_state.cursor_y = 0;
    console_state.text_color = M4K_VGA_COLOR_WHITE;
    console_state.background_color = M4K_VGA_COLOR_BLACK;
    console_state.initialized = true;

    mkrn_console_clear();
}

void
mkrn_console_set_cursor(uint8_t u8X, uint8_t u8Y)
{
    if (!console_state.initialized)
        return;

    if (u8X < VGA_WIDTH && u8Y < VGA_HEIGHT) {
        console_state.cursor_x = u8X;
        console_state.cursor_y = u8Y;
        vga_set_cursor_position(
            u8Y * VGA_WIDTH + u8X);
    }
}

void
mkrn_console_get_cursor(uint8_t *pX, uint8_t *pY)
{
    if (!console_state.initialized)
        return;

    if (pX)
        *pX = console_state.cursor_x;
    if (pY)
        *pY = console_state.cursor_y;
}

void
mkrn_console_put_char(char c)
{
    serial_putchar(c);

    if (!console_state.initialized)
        return;

    switch (c) {
    case '\n':
        mkrn_console_newline();
        return;
    case '\r':
        console_state.cursor_x = 0;
        vga_set_cursor_position(
            console_state.cursor_y * VGA_WIDTH
            + console_state.cursor_x);
        return;
    case '\t':
        mkrn_console_tab();
        return;
    case '\b':
        mkrn_console_backspace();
        return;
    }

    if (console_state.cursor_x >= VGA_WIDTH)
        mkrn_console_newline();

    if (console_state.cursor_y >= VGA_HEIGHT) {
        mkrn_console_scroll();
        console_state.cursor_y = VGA_HEIGHT - 1;
    }

    uint32_t u32Pos =
        console_state.cursor_y * VGA_WIDTH
        + console_state.cursor_x;
    if (u32Pos < VGA_SIZE) {
        uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;
        uint16_t u16Color =
            (console_state.background_color << 4)
            | console_state.text_color;
        pBuffer[u32Pos] = c | (u16Color << 8);
    }

    console_state.cursor_x++;
    vga_set_cursor_position(
        console_state.cursor_y * VGA_WIDTH
        + console_state.cursor_x);
}

void
mkrn_console_newline(void)
{
    if (!console_state.initialized)
        return;

    console_state.cursor_x = 0;
    console_state.cursor_y++;

    if (console_state.cursor_y >= VGA_HEIGHT) {
        mkrn_console_scroll();
        console_state.cursor_y = VGA_HEIGHT - 1;
    }

    vga_set_cursor_position(
        console_state.cursor_y * VGA_WIDTH
        + console_state.cursor_x);
}

void
mkrn_console_write(const char *pStr)
{
    if (!console_state.initialized || pStr == NULL)
        return;

    while (*pStr)
        mkrn_console_put_char(*pStr++);
}

void
mkrn_console_write_hex(uint32_t u32Value)
{
    if (!console_state.initialized)
        return;


    char buffer[8];
    int i = 0;
    bool bLeadingZero = true;

    for (int shift = 28; shift >= 0; shift -= 4) {
        uint8_t u8Digit =
            (u32Value >> shift) & 0xF;
        if (u8Digit != 0 || !bLeadingZero
            || shift == 0)
        {
            buffer[i++] =
                (u8Digit < 10)
                    ? ('0' + u8Digit)
                    : ('A' + u8Digit - 10);
            bLeadingZero = false;
        }
    }

    if (i == 0)
        mkrn_console_put_char('0');
    else
        for (int j = 0; j < i; j++)
            mkrn_console_put_char(buffer[j]);
}

void
mkrn_console_write_dec(uint32_t u32Value)
{
    if (!console_state.initialized)
        return;

    if (u32Value == 0) {
        mkrn_console_put_char('0');
        return;
    }

    char buffer[10];
    int i = 0;

    while (u32Value > 0 && i < 9) {
        buffer[i++] = '0' + (u32Value % 10);
        u32Value /= 10;
    }

    for (int j = i - 1; j >= 0; j--)
        mkrn_console_put_char(buffer[j]);
}

void
mkrn_console_write_bin(uint32_t u32Value)
{
    if (!console_state.initialized)
        return;

    mkrn_console_write("0b");

    if (u32Value == 0) {
        mkrn_console_put_char('0');
        return;
    }

    char buffer[32];
    int i = 0;

    for (int shift = 31; shift >= 0; shift--) {
        if (u32Value & (1 << shift))
            buffer[i++] = '1';
        else if (i > 0 || shift == 0)
            buffer[i++] = '0';
    }

    for (int j = 0; j < i; j++)
        mkrn_console_put_char(buffer[j]);
}

void
mkrn_console_set_color(uint8_t u8Color)
{
    if (!console_state.initialized)
        return;

    console_state.text_color = u8Color & 0x0F;
    console_state.background_color =
        (u8Color >> 4) & 0x0F;
}
