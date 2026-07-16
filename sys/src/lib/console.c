/*
 * M4KK1 4P1 - console.c
 * Description: Console output — printf, panic, error
 *              screens for VGA text mode.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../include/console.h"

#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_SIZE        (VGA_WIDTH * VGA_HEIGHT)
#define VGA_MEMORY      0xB8000

typedef struct {
    uint16_t *buffer;
    uint8_t  cursor_x;
    uint8_t  cursor_y;
    uint8_t  text_color;
    uint8_t  background_color;
    bool     initialized;
} console_state_t;

static console_state_t console_state;

extern void mkrn_console_write(const char *pStr);
extern void mkrn_console_write_hex(
    uint32_t u32Value);
extern void mkrn_console_write_dec(
    uint32_t u32Value);
extern void mkrn_console_put_char(char c);

void
mkrn_console_printf(const char *pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);

    while (*pFormat) {
        if (*pFormat == '%') {
            pFormat++;
            switch (*pFormat) {
            case 's': {
                const char *pS =
                    va_arg(args, const char *);
                if (pS)
                    mkrn_console_write(pS);
                else
                    mkrn_console_write("(null)");
                break;
            }
            case 'd':
            case 'u': {
                uint32_t u32Value =
                    va_arg(args, uint32_t);
                mkrn_console_write_dec(u32Value);
                break;
            }
            case 'x':
            case 'X': {
                uint32_t u32Value =
                    va_arg(args, uint32_t);
                mkrn_console_write_hex(u32Value);
                break;
            }
            case 'c': {
                char c =
                    (char)va_arg(args, int);
                mkrn_console_put_char(c);
                break;
            }
            case '%':
                mkrn_console_put_char('%');
                break;
            default:
                mkrn_console_put_char('%');
                mkrn_console_put_char(*pFormat);
                break;
            }
        } else {
            mkrn_console_put_char(*pFormat);
        }
        pFormat++;
    }

    va_end(args);
}

void
mkrn_console_set_screen_color(uint8_t u8Background,
                              uint8_t u8Foreground)
{
    if (!console_state.initialized)
        return;

    console_state.text_color = u8Foreground;
    console_state.background_color = u8Background;

    uint16_t *pBuffer = (uint16_t *)VGA_MEMORY;
    uint16_t u16Color =
        (u8Background << 4) | u8Foreground;
    uint16_t u16Blank = 0x20 | (u16Color << 8);

    for (int i = 0; i < VGA_SIZE; i++)
        pBuffer[i] = u16Blank;
}

void
mkrn_console_panic(const char *pMessage)
{
    mkrn_console_set_screen_color(VGA_COLOR_BLUE,
                                  VGA_COLOR_WHITE);

    mkrn_console_clear();
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "           KERNEL PANIC :( \n");
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "A critical system error has occurred.\n\n");

    if (pMessage) {
        mkrn_console_write("Error: ");
        mkrn_console_write(pMessage);
        mkrn_console_write("\n");
    }

    mkrn_console_write("\nSystem halted.\n");
    mkrn_console_write(
        "=====================================\n");
}

void
mkrn_console_memory_error(const char *pMessage)
{
    mkrn_console_set_screen_color(VGA_COLOR_RED,
                                  VGA_COLOR_WHITE);

    mkrn_console_clear();
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "         MEMORY ERROR :/ \n");
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "A memory management error occurred.\n\n");

    if (pMessage) {
        mkrn_console_write("Error: ");
        mkrn_console_write(pMessage);
        mkrn_console_write("\n");
    }

    mkrn_console_write("\nSystem halted.\n");
    mkrn_console_write(
        "=====================================\n");
}

void
mkrn_console_system_error(const char *pMessage)
{
    mkrn_console_set_screen_color(
        VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    mkrn_console_clear();
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "         SYSTEM ERROR :3 \n");
    mkrn_console_write(
        "=====================================\n");
    mkrn_console_write(
        "A system error has occurred.\n\n");

    if (pMessage) {
        mkrn_console_write("Error: ");
        mkrn_console_write(pMessage);
        mkrn_console_write("\n");
    }

    mkrn_console_write(
        "\nPlease restart the system.\n");
    mkrn_console_write(
        "=====================================\n");
}
