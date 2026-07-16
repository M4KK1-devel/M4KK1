/*
 * M4KK1 4P1 - keyboard.h
 * Description: Keyboard driver interface for PS/2 and
 *              USB keyboard devices.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    KEYBOARD_EVENT_KEY_PRESS = 0,
    KEYBOARD_EVENT_KEY_RELEASE = 1,
    KEYBOARD_EVENT_KEY_REPEAT = 2
} keyboard_event_type_t;

typedef enum {
    KEYBOARD_KEY_RELEASED = 0,
    KEYBOARD_KEY_PRESSED = 1
} keyboard_key_state_t;

#define KEYBOARD_MOD_NONE   0x0000
#define KEYBOARD_MOD_LSHIFT 0x0001
#define KEYBOARD_MOD_RSHIFT 0x0002
#define KEYBOARD_MOD_LCTRL  0x0004
#define KEYBOARD_MOD_RCTRL  0x0008
#define KEYBOARD_MOD_LALT   0x0010
#define KEYBOARD_MOD_RALT   0x0020
#define KEYBOARD_MOD_LMETA  0x0040
#define KEYBOARD_MOD_RMETA  0x0080
#define KEYBOARD_MOD_CAPS   0x0100
#define KEYBOARD_MOD_NUM    0x0200

typedef struct {
    keyboard_event_type_t type;
    uint32_t    keycode;
    keyboard_key_state_t state;
    uint32_t    modifiers;
    char        ascii_char;
    uint64_t    timestamp;
} keyboard_event_t;

typedef struct {
    bool num_lock;
    bool caps_lock;
    bool scroll_lock;
} keyboard_led_t;

typedef enum {
    KEYBOARD_LAYOUT_QWERTY = 0,
    KEYBOARD_LAYOUT_AZERTY = 1,
    KEYBOARD_LAYOUT_QWERTZ = 2,
    KEYBOARD_LAYOUT_DVORAK = 3,
    KEYBOARD_LAYOUT_COLEMAK = 4
} keyboard_layout_t;

typedef struct {
    keyboard_layout_t layout;
    bool    repeat_enabled;
    uint32_t repeat_delay;
    uint32_t repeat_rate;
    keyboard_led_t led_state;
} keyboard_config_t;

typedef struct {
    char *name;
    char *description;
    int   (*init)(void);
    void  (*cleanup)(void);
    int   (*poll_event)(keyboard_event_t *pEvent);
    int   (*wait_event)(keyboard_event_t *pEvent);
    int   (*set_config)(keyboard_config_t *pConfig);
    int   (*get_config)(keyboard_config_t *pConfig);
    int   (*set_led)(keyboard_led_t *pLed);
    int   (*get_led)(keyboard_led_t *pLed);
    void *priv_data;
} keyboard_driver_t;

int  mkrn_keyboard_driver_register(
    keyboard_driver_t *pDriver);
int  mkrn_keyboard_driver_unregister(
    keyboard_driver_t *pDriver);
keyboard_driver_t *mkrn_keyboard_driver_get(
    const char *pName);

int  mkrn_ps2_keyboard_init(void);
void mkrn_ps2_keyboard_cleanup(void);
int  mkrn_ps2_keyboard_poll_event(
    keyboard_event_t *pEvent);

int  mkrn_usb_keyboard_init(void);
void mkrn_usb_keyboard_cleanup(void);
int  mkrn_usb_keyboard_poll_event(
    keyboard_event_t *pEvent);

int  mkrn_keyboard_process_event(
    keyboard_event_t *pEvent);
int  mkrn_keyboard_translate_keycode(
    uint32_t u32Keycode, uint32_t u32Modifiers,
    char *pAscii);

int  mkrn_keyboard_load_config(
    const char *pFilename, keyboard_config_t *pCfg);
int  mkrn_keyboard_save_config(
    const char *pFilename, keyboard_config_t *pCfg);

int  mkrn_keyboard_self_test(void);
int  mkrn_keyboard_diagnostic(void);

void mkrn_kbd_init(void);
char mkrn_keyboard_get_char(void);
bool mkrn_keyboard_has_char(void);
bool mkrn_keyboard_is_initialized(void);
uint32_t mkrn_keyboard_get_modifiers(void);
