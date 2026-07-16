/*
 * M4KK1 4P1 - mouse.c
 * Description: PS/2 mouse driver — packet processing,
 *              interrupt handling, position tracking.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "mouse.h"
#include "../../include/console.h"
#include "../../include/kernel.h"
#include "../../include/idt.h"
#include <stdint.h>
#include <stdbool.h>
#include "../../include/string.h"

#define MOUSE_DATA_PORT         0x60
#define MOUSE_STATUS_PORT       0x64
#define MOUSE_COMMAND_PORT      0x64

#define MOUSE_CMD_DISABLE       0xA7
#define MOUSE_CMD_ENABLE        0xA8
#define MOUSE_CMD_TEST_MOUSE    0xA9
#define MOUSE_CMD_SELF_TEST     0xAA
#define MOUSE_CMD_INTERFACE_TEST 0xAB

#define MOUSE_ACK               0xFA
#define MOUSE_RESEND            0xFE
#define MOUSE_TEST_OK           0x00

#define MOUSE_STATUS_OBF        0x01
#define MOUSE_STATUS_IBF        0x02

#define MOUSE_PACKET_SIZE       3

#define MOUSE_LEFT_BUTTON       0x01
#define MOUSE_RIGHT_BUTTON      0x02
#define MOUSE_MIDDLE_BUTTON     0x04
#define MOUSE_X_SIGN            0x10
#define MOUSE_Y_SIGN            0x20
#define MOUSE_X_OVERFLOW        0x40
#define MOUSE_Y_OVERFLOW        0x80

typedef struct {
    bool     initialized;
    bool     has_wheel;
    int8_t   x_movement;
    int8_t   y_movement;
    int8_t   z_movement;
    uint8_t  buttons;
    uint8_t  packet[MOUSE_PACKET_SIZE];
    uint8_t  packet_index;
    int32_t  x_position;
    int32_t  y_position;
    uint32_t sample_rate;
} mouse_state_t;

static mouse_state_t mouse_state;

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
mouse_wait_ready(void)
{
    uint32_t u32Timeout = 100000;
    while (u32Timeout--) {
        uint8_t u8Status = inb(MOUSE_STATUS_PORT);
        if (!(u8Status & MOUSE_STATUS_IBF))
            return true;
    }
    return false;
}

static bool
mouse_send_command(uint8_t u8Command)
{
    if (!mouse_wait_ready())
        return false;
    outb(MOUSE_COMMAND_PORT, u8Command);
    return true;
}

static bool
mouse_send_data(uint8_t u8Data)
{
    if (!mouse_wait_ready())
        return false;
    outb(MOUSE_DATA_PORT, u8Data);
    return true;
}

static uint8_t
mouse_read_data(void)
{
    while (!(inb(MOUSE_STATUS_PORT)
             & MOUSE_STATUS_OBF)) { }
    return inb(MOUSE_DATA_PORT);
}

static uint8_t
mouse_wait_response(void)
{
    uint32_t u32Timeout = 100000;
    while (u32Timeout--) {
        if (inb(MOUSE_STATUS_PORT)
            & MOUSE_STATUS_OBF)
            return inb(MOUSE_DATA_PORT);
    }
    return 0;
}

static void
mouse_process_packet(uint8_t *pPacket)
{
    mouse_state.buttons = pPacket[0] & 0x07;
    mouse_state.x_movement = (int8_t)pPacket[1];
    mouse_state.y_movement = (int8_t)pPacket[2];

    if (pPacket[0] & MOUSE_X_SIGN)
        mouse_state.x_movement -= 256;
    if (pPacket[0] & MOUSE_Y_SIGN)
        mouse_state.y_movement -= 256;

    if (mouse_state.has_wheel
        && mouse_state.packet_index > 3)
    {
        mouse_state.z_movement =
            (int8_t)pPacket[3];
        if (pPacket[0] & MOUSE_X_OVERFLOW)
            mouse_state.z_movement -= 256;
    }

    mouse_state.x_position +=
        mouse_state.x_movement;
    mouse_state.y_position -=
        mouse_state.y_movement;

    if (mouse_state.x_position < 0)
        mouse_state.x_position = 0;
    if (mouse_state.y_position < 0)
        mouse_state.y_position = 0;
}

void
mkrn_mouse_handler(void)
{
    if (!(inb(MOUSE_STATUS_PORT)
          & MOUSE_STATUS_OBF))
        return;

    uint8_t u8Data = inb(MOUSE_DATA_PORT);
    mouse_state.packet[mouse_state.packet_index++] =
        u8Data;

    if (mouse_state.packet_index
        >= MOUSE_PACKET_SIZE)
    {
        mouse_process_packet(mouse_state.packet);
        mouse_state.packet_index = 0;
    }
}

void
mkrn_mouse_init(void)
{
    M4K_LOG_INFO("Initializing mouse driver...");

    mkrn_memset(&mouse_state, 0, sizeof(mouse_state));
    mouse_state.packet_index = 0;
    mouse_state.sample_rate = 100;

    mouse_send_command(MOUSE_CMD_DISABLE);

    while (inb(MOUSE_STATUS_PORT)
           & MOUSE_STATUS_OBF)
        inb(MOUSE_DATA_PORT);

    if (mouse_send_command(MOUSE_CMD_SELF_TEST)) {
        uint8_t u8Resp = mouse_wait_response();
        if (u8Resp != MOUSE_TEST_OK) {
            M4K_LOG_WARN(
                "Mouse self-test failed");
            return;
        }
    }

    if (mouse_send_command(MOUSE_CMD_ENABLE)) {
        uint8_t u8Resp = mouse_wait_response();
        if (u8Resp != MOUSE_ACK) {
            M4K_LOG_WARN("Mouse enable failed");
            return;
        }
    }

    if (mouse_send_data(0xF3)) {
        mouse_wait_response();
        mouse_send_data(100);
    }

    if (mouse_send_data(0xF4)) {
        uint8_t u8Resp = mouse_wait_response();
        if (u8Resp == MOUSE_ACK)
            mouse_state.has_wheel = true;
    }

    if (mouse_send_data(0xE8)) {
        mouse_wait_response();
        mouse_send_data(0x03);
    }

    mkrn_idt_register_handler(
        0x2C,
        (interrupt_handler_t)
            mkrn_mouse_handler);

    mouse_state.initialized = true;
    M4K_LOG_INFO("Mouse driver initialized");
}

bool
mkrn_mouse_is_initialized(void)
{
    return mouse_state.initialized;
}

void
mkrn_mouse_get_position(int32_t *pX, int32_t *pY)
{
    if (pX)
        *pX = mouse_state.x_position;
    if (pY)
        *pY = mouse_state.y_position;
}

void
mkrn_mouse_get_movement(int8_t *pX, int8_t *pY,
                        int8_t *pZ)
{
    if (pX)
        *pX = mouse_state.x_movement;
    if (pY)
        *pY = mouse_state.y_movement;
    if (pZ)
        *pZ = mouse_state.z_movement;
}

uint8_t
mkrn_mouse_get_buttons(void)
{
    return mouse_state.buttons;
}

void
mkrn_mouse_set_sample_rate(uint32_t u32Rate)
{
    if (u32Rate > 200)
        u32Rate = 200;
    if (u32Rate < 10)
        u32Rate = 10;

    mouse_state.sample_rate = u32Rate;

    if (mouse_send_data(0xF3)) {
        mouse_wait_response();
        mouse_send_data((uint8_t)u32Rate);
    }
}

uint32_t
mkrn_mouse_get_sample_rate(void)
{
    return mouse_state.sample_rate;
}

bool
mkrn_mouse_has_wheel(void)
{
    return mouse_state.has_wheel;
}
