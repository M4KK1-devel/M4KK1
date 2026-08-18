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
#include "../../include/video.h"
#include <stdint.h>
#include <stdbool.h>
#include "../../include/string.h"

extern void pic_unmask_irq(uint32_t irq_num);

/* ── Mouse event ring buffer ── */
#define MOUSE_EVENT_BUF_SIZE 128

static struct m4k_mouse_event mouse_event_buf[MOUSE_EVENT_BUF_SIZE];
static volatile uint32_t mouse_event_head = 0;
static volatile uint32_t mouse_event_tail = 0;

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
    int32_t  x_movement;
    int32_t  y_movement;
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
                     : "a"(u8Value), "d"((uint16_t)u16Port));
}

static inline uint8_t
inb(uint16_t u16Port)
{
    uint8_t u8Value;
    __asm__ volatile("inb %1, %0"
                     : "=a"(u8Value)
                     : "d"((uint16_t)u16Port));
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

    /* Overflow packets carry bogus deltas: QEMU's input-send-event with
     * |delta| > 255 sets the overflow bits.  Clamp instead of trusting
     * the wrapped magnitude so large jumps still land (direction
     * preserved, magnitude capped at 255 per packet). */
    int32_t xm, ym;
    if (pPacket[0] & MOUSE_X_OVERFLOW)
        xm = (pPacket[0] & MOUSE_X_SIGN) ? -255 : 255;
    else
        xm = (int8_t)pPacket[1];
    if (pPacket[0] & MOUSE_Y_OVERFLOW)
        ym = (pPacket[0] & MOUSE_Y_SIGN) ? -255 : 255;
    else
        ym = (int8_t)pPacket[2];

    /* 9-bit two's-complement decode for the non-overflow case: the
     * byte-0 sign bit adds -256 for values in [-256, -129]. */
    if (!(pPacket[0] & MOUSE_X_OVERFLOW) &&
        (pPacket[0] & MOUSE_X_SIGN) && !(pPacket[1] & 0x80))
        xm -= 256;
    if (!(pPacket[0] & MOUSE_Y_OVERFLOW) &&
        (pPacket[0] & MOUSE_Y_SIGN) && !(pPacket[2] & 0x80))
        ym -= 256;

    /* PS/2 device Y grows UPWARD (button toward user = +y); screen
     * coordinates grow downward — invert the axis. */
    ym = -ym;

    /* Raw deltas straight through (no ballistics multiplier): the
     * QMP/QEMU pointer injects absolute-accurate relative deltas and
     * automated tests expect a 1:1 move.  Human users can move the
     * hardware mouse faster instead.  Keep the full 9-bit range:
     * re-casting to int8_t would wrap |delta| > 127 packets and lose
     * most of a large QMP jump. */
    mouse_state.x_movement = xm;
    mouse_state.y_movement = ym;

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
    mouse_state.y_position +=
        mouse_state.y_movement;

    if (mouse_state.x_position < 0)
        mouse_state.x_position = 0;
    if (mouse_state.x_position > 799)
        mouse_state.x_position = 799;
    if (mouse_state.y_position < 0)
        mouse_state.y_position = 0;
    if (mouse_state.y_position > 599)
        mouse_state.y_position = 599;

    uint32_t next_tail = (mouse_event_tail + 1) % MOUSE_EVENT_BUF_SIZE;
    if (next_tail != mouse_event_head) {
        mouse_event_buf[mouse_event_tail].dx = mouse_state.x_movement;
        mouse_event_buf[mouse_event_tail].dy = mouse_state.y_movement;
        mouse_event_buf[mouse_event_tail].buttons = mouse_state.buttons;
        mouse_event_buf[mouse_event_tail].reserved = 0;
        mouse_event_tail = next_tail;
    }
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
    M4K_LOG_INFO("Initializing PS/2 mouse...\n");
    mkrn_memset(&mouse_state, 0, sizeof(mouse_state));
    mouse_state.x_position = 400;
    mouse_state.y_position = 300;

    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xA8), "d"((uint16_t)0x64));

    for (int _i = 0; _i < 1000 && (inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_OBF); _i++)
        inb(MOUSE_DATA_PORT);

    /* PS/2 init must write cmd byte in two steps due to QEMU 11 quirk.
       Step 1: disable mouse, configure device, then enable. */
    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x60), "d"((uint16_t)0x64));
    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x45), "d"((uint16_t)0x60));

    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xD4), "d"((uint16_t)0x64));
    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xF4), "d"((uint16_t)0x60));
    mouse_wait_response();

    mouse_wait_ready();
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x60), "d"((uint16_t)0x64));
    mouse_wait_ready();
    /* Command byte: 0x47 = enable IRQ1 (keyboard) AND IRQ12 (mouse),
     * system flag, and — critical — bit6 PS/2 port-1 TRANSLATION.
     * Without bit6 (old value 0x07) the controller forwards raw
     * Set-2 scancodes while the driver keymaps are indexed by Set-1
     * codes, so every key lookup misses and the keyboard buffer stays
     * empty (timer IRQ0 unaffected — only keys/mouse die). */
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x47), "d"((uint16_t)0x60));

    mkrn_idt_register_handler(0x2C, (mkrn_int_handler_t)mkrn_mouse_handler);
    pic_unmask_irq(12);

    mouse_state.initialized = true;
    M4K_LOG_INFO("PS/2 mouse driver initialized\n");
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

bool
mkrn_mouse_has_event(void)
{
    return mouse_event_head != mouse_event_tail;
}

bool
mkrn_mouse_get_event(struct m4k_mouse_event *ev)
{
    if (mouse_event_head == mouse_event_tail)
        return false;

    *ev = mouse_event_buf[mouse_event_head];
    mouse_event_head = (mouse_event_head + 1) % MOUSE_EVENT_BUF_SIZE;
    return true;
}
