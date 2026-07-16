/*
 * M4KK1 4P1 - mouse.h
 * Description: Mouse driver interface for PS/2 and
 *              USB mouse devices.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MOUSE_EVENT_BUTTON_PRESS = 0,
    MOUSE_EVENT_BUTTON_RELEASE = 1,
    MOUSE_EVENT_MOTION = 2,
    MOUSE_EVENT_WHEEL = 3
} mouse_event_type_t;

typedef enum {
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_SIDE = 3,
    MOUSE_BUTTON_EXTRA = 4
} mouse_button_t;

typedef enum {
    MOUSE_BUTTON_RELEASED = 0,
    MOUSE_BUTTON_PRESSED = 1
} mouse_button_state_t;

typedef struct {
    mouse_event_type_t  type;
    mouse_button_t      button;
    mouse_button_state_t button_state;
    int32_t x;
    int32_t y;
    int32_t wheel_x;
    int32_t wheel_y;
    int32_t dx;
    int32_t dy;
    int32_t dwheel_x;
    int32_t dwheel_y;
    bool left_button;
    bool right_button;
    bool middle_button;
    uint64_t timestamp;
} mouse_event_t;

typedef struct {
    bool    enabled;
    int32_t acceleration;
    int32_t sensitivity;
    int32_t threshold;
    bool    swap_buttons;
    bool    wheel_emulation;
} mouse_config_t;

typedef struct {
    char *name;
    char *description;
    int   (*init)(void);
    void  (*cleanup)(void);
    int   (*poll_event)(mouse_event_t *pEvent);
    int   (*wait_event)(mouse_event_t *pEvent);
    int   (*set_config)(mouse_config_t *pConfig);
    int   (*get_config)(mouse_config_t *pConfig);
    void *priv_data;
} mouse_driver_t;

int  mkrn_mouse_driver_register(
    mouse_driver_t *pDriver);
int  mkrn_mouse_driver_unregister(
    mouse_driver_t *pDriver);
mouse_driver_t *mkrn_mouse_driver_get(
    const char *pName);

int  mkrn_ps2_mouse_init(void);
void mkrn_ps2_mouse_cleanup(void);
int  mkrn_ps2_mouse_poll_event(
    mouse_event_t *pEvent);

int  mkrn_usb_mouse_init(void);
void mkrn_usb_mouse_cleanup(void);
int  mkrn_usb_mouse_poll_event(
    mouse_event_t *pEvent);

int  mkrn_mouse_process_event(mouse_event_t *pEvent);
int  mkrn_mouse_calibrate(int32_t *pXScale,
                          int32_t *pYScale);

int  mkrn_mouse_load_config(
    const char *pFilename, mouse_config_t *pCfg);
int  mkrn_mouse_save_config(
    const char *pFilename, mouse_config_t *pCfg);

int  mkrn_mouse_self_test(void);
int  mkrn_mouse_diagnostic(void);
