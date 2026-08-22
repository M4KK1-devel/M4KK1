/*
 * M4KK1 4P1 - libgui.c
 * Description: User-space graphics library for MDM
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

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

/* Framebuffer info cache */
static struct m4k_framebuffer_info fb_cache;
static int fb_cached = 0;

/* Get framebuffer info (cached) */
static int gui_get_fb_info(void) {
    if (!fb_cached) {
        if (m4k_get_framebuffer_info(&fb_cache) < 0)
            return -1;
        fb_cached = 1;
    }
    return 0;
}

/* Draw a filled rectangle */
int gui_draw_rect(int x, int y, int w, int h, uint32_t color) {
    return m4k_draw_rect(x, y, w, h, color);
}

/* Draw text string */
int gui_draw_text(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    return m4k_draw_text(x, y, str, fg, bg);
}

/* Draw input box with border */
int gui_draw_input_box(int x, int y, int w, int h, const char *text, int focused) {
    uint32_t border_color = focused ? GUI_COLOR_BLUE : GUI_COLOR_GRAY;
    uint32_t bg_color = GUI_COLOR_WHITE;
    
    /* Draw border */
    gui_draw_rect(x, y, w, h, border_color);
    /* Draw background (inside) */
    gui_draw_rect(x + 2, y + 2, w - 4, h - 4, bg_color);
    /* Draw text */
    if (text && text[0]) {
        gui_draw_text(x + 6, y + 6, text, GUI_COLOR_BLACK, bg_color);
    }
    return 0;
}

/* Draw 3D button */
int gui_draw_button(int x, int y, int w, int h, const char *label, int pressed) {
    uint32_t top_color = pressed ? GUI_COLOR_DARK : GUI_COLOR_LIGHT;
    uint32_t bottom_color = pressed ? GUI_COLOR_LIGHT : GUI_COLOR_DARK;
    uint32_t face_color = GUI_COLOR_GRAY;
    
    /* Draw face */
    gui_draw_rect(x + 2, y + 2, w - 4, h - 4, face_color);
    
    /* Draw top/left highlight */
    gui_draw_rect(x, y, w, 2, top_color);
    gui_draw_rect(x, y, 2, h, top_color);
    
    /* Draw bottom/right shadow */
    gui_draw_rect(x, y + h - 2, w, 2, bottom_color);
    gui_draw_rect(x + w - 2, y, 2, h, bottom_color);
    
    /* Draw label centered */
    if (label) {
        int text_x = x + (w - (int)musr_strlen(label) * 9) / 2;
        int text_y = y + (h - 16) / 2;
        gui_draw_text(text_x, text_y, label, GUI_COLOR_BLACK, face_color);
    }
    return 0;
}

/* Clear screen with color */
int gui_clear(uint32_t color) {
    if (gui_get_fb_info() < 0)
        return -1;
    return gui_draw_rect(0, 0, fb_cache.width, fb_cache.height, color);
}

/* Draw gradient background */
int gui_draw_gradient(uint32_t color_top, uint32_t color_bottom) {
    if (gui_get_fb_info() < 0)
        return -1;

    /* Kernel-side fill: one syscall for the whole gradient (the old
     * per-scanline loop paid a syscall + scheduler yield per row). */
    return m4k_fill_gradient(0, 0, fb_cache.width, fb_cache.height,
                             color_top, color_bottom);
}

/* Unified system wallpaper: vertical blue gradient 0x000044 → 0x0066FF.
 * Both MDM (login) and Sprach/Copland (desktop) call this so the boot
 * chain never flashes a mismatched background.  Single kernel
 * fill_gradient syscall — no separate flip needed (next composite
 * presents it). */
#define GUI_WALLPAPER_TOP    0x00000044u
#define GUI_WALLPAPER_BOTTOM 0x000066FFu

int gui_draw_wallpaper(void)
{
    return gui_draw_gradient(GUI_WALLPAPER_TOP, GUI_WALLPAPER_BOTTOM);
}

/* Present back buffer to screen */
int gui_flip(void) {
    return m4k_flip();
}

/* Get mouse event */
int gui_get_mouse_event(struct m4k_mouse_event *ev) {
    return m4k_get_mouse_event(ev);
}

/* Get keyboard event */
int gui_get_keyboard_event(struct m4k_keyboard_event *ev) {
    return m4k_get_keyboard_event(ev);
}

/* Check if point is inside rectangle */
int gui_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

/* Copy a 32x32 ARGB icon into a caller-owned buffer (alpha 0 skips) */
int gui_draw_icon(uint32_t *dst, int bw, int x, int y,
                  const uint32_t *icon) {
    if (!dst || !icon)
        return -1;
    for (int row = 0; row < 32; row++)
        for (int col = 0; col < 32; col++) {
            uint32_t c = icon[row * 32 + col];
            if (c == 0x00000000)
                continue;   /* transparent */
            dst[(y + row) * bw + (x + col)] = c;
        }
    return 0;
}
