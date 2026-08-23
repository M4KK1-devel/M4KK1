/*
 * M4KK1 4P1 - libgui.h
 * Description: User-space graphics library header
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef LIBGUI_H
#define LIBGUI_H

#include "m4sh.h"

/* Color constants */
#define GUI_COLOR_BLACK   0x00000000
#define GUI_COLOR_WHITE   0x00FFFFFF
#define GUI_COLOR_GRAY    0x00808080
#define GUI_COLOR_DARK    0x00404040
#define GUI_COLOR_LIGHT   0x00C0C0C0
#define GUI_COLOR_BLUE    0x000000FF
#define GUI_COLOR_RED     0x00FF0000
#define GUI_COLOR_GREEN   0x0000FF00

/* Drawing primitives */
int gui_draw_rect(int x, int y, int w, int h, uint32_t color);
int gui_draw_text(int x, int y, const char *str, uint32_t fg, uint32_t bg);
int gui_draw_input_box(int x, int y, int w, int h, const char *text, int focused);
int gui_draw_button(int x, int y, int w, int h, const char *label, int pressed);

/* Screen operations */
int gui_clear(uint32_t color);
int gui_draw_gradient(uint32_t color_top, uint32_t color_bottom);

/* Unified system wallpaper (blue gradient 0x000044 → 0x0066FF) */
int gui_draw_wallpaper(void);
int gui_flip(void);

/* Input events */
int gui_get_mouse_event(struct m4k_mouse_event *ev);
int gui_get_keyboard_event(struct m4k_keyboard_event *ev);

/* Utility */
int gui_point_in_rect(int px, int py, int x, int y, int w, int h);

/* Icon blit: copy a 32x32 ARGB icon (alpha 0 = transparent) into a
 * caller-owned pixel buffer (dst, width bw) at (x, y).  This is the
 * surface-buffer side; MDM uses m4k_gfx_blit for full-screen icons. */
int gui_draw_icon(uint32_t *dst, int bw, int x, int y,
                  const uint32_t *icon);

#endif /* LIBGUI_H */
