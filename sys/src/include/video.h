#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ── Bochs VBE I/O ports and registers ── */
#define VBE_DISPI_IOPORT_INDEX       0x01CE
#define VBE_DISPI_IOPORT_DATA        0x01CF

#define VBE_DISPI_INDEX_ID           0x0
#define VBE_DISPI_INDEX_XRES         0x1
#define VBE_DISPI_INDEX_YRES         0x2
#define VBE_DISPI_INDEX_BPP          0x3
#define VBE_DISPI_INDEX_ENABLE       0x4
#define VBE_DISPI_INDEX_BANK         0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH   0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT  0x7
#define VBE_DISPI_INDEX_X_OFFSET     0x8
#define VBE_DISPI_INDEX_Y_OFFSET     0x9

#define VBE_DISPI_DISABLED           0x00
#define VBE_DISPI_ENABLED            0x01
#define VBE_DISPI_GETCAPS            0x02
#define VBE_DISPI_8BIT_DAC           0x20
#define VBE_DISPI_LFB_ENABLED        0x40
#define VBE_DISPI_NOCLEARMEM         0x80

/* ── Default resolution ── */
#define VBE_DEFAULT_WIDTH  800
#define VBE_DEFAULT_HEIGHT 600
#define VBE_DEFAULT_BPP    32

/* ── Framebuffer info struct (returned to userspace) ── */
struct m4k_framebuffer_info {
    uint32_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
};

/* ── M4KK1 native syscall numbers ── */
#define M4K_SYS_GET_FRAMEBUFFER_INFO  0x4D000050
#define M4K_SYS_DRAW_TEST_PATTERN     0x4D000051
#define M4K_SYS_GET_MOUSE_EVENT       0x4D000052
#define M4K_SYS_FLIP                  0x4D000053
#define M4K_SYS_DRAW_RECT             0x4D000054
#define M4K_SYS_DRAW_TEXT             0x4D000055
#define M4K_SYS_GET_KEYBOARD_EVENT    0x4D000056
#define M4K_SYS_GFX_BLIT              0x4D000057

/* ── Mouse event (kernel→userspace) ── */
struct m4k_mouse_event {
    int16_t dx;
    int16_t dy;
    uint8_t buttons;
    uint8_t reserved;
};

/* ── Keyboard event (kernel→userspace) ── */
struct m4k_keyboard_event {
    uint8_t ascii_char;
    uint8_t keycode;
    uint8_t modifiers;
    uint8_t reserved;
};

/* ── Color constants ── */
#define VESA_COLOR_BLACK   0x00000000
#define VESA_COLOR_WHITE   0x00FFFFFF
#define VESA_COLOR_RED     0x00FF0000
#define VESA_COLOR_GREEN   0x0000FF00
#define VESA_COLOR_BLUE    0x000000FF
#define VESA_COLOR_YELLOW  0x00FFFF00
#define VESA_COLOR_CYAN    0x0000FFFF
#define VESA_COLOR_MAGENTA 0x00FF00FF
#define VESA_COLOR_ORANGE  0x00FF8800

/* ── Function declarations ── */
void mkrn_vesa_init(void);
void mkrn_vesa_draw_test_pattern(void);
uint32_t m4k_syscall_get_framebuffer_info_impl(
    uint32_t buf_ptr, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_draw_test_pattern_impl(
    uint32_t buf_ptr, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_flip_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_draw_rect_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_draw_text_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_gfx_blit_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
/* Fill a rect in back_buffer with a vertical gradient (kernel-side
 * per-row color, one syscall instead of one draw_rect per row).
 * arg5 points at struct m4k_gradient_params { top, bottom } in the
 * caller's memory (flat address space, gfx_blit-style). */
uint32_t m4k_syscall_fill_gradient_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_flip_rect_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);
uint32_t m4k_syscall_update_cursor_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5);

/* Phase 1: Double buffering */
void mkrn_vesa_flip(void);
void mkrn_vesa_flip_rect(int x, int y, int w, int h);
void mkrn_vesa_update_cursor(void);
void mkrn_vesa_clear(uint32_t color);

/* Phase 2: Drawing primitives */
void mkrn_vesa_put_pixel(int x, int y, uint32_t color);
void mkrn_vesa_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void mkrn_vesa_draw_rect(int x, int y, int w, int h, uint32_t color, int fill);
void mkrn_vesa_put_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void mkrn_vesa_put_string(int x, int y, const char *s, uint32_t fg, uint32_t bg);

/* ── Software cursor ── */
void mkrn_vesa_set_cursor_pos(int32_t x, int32_t y);
void mkrn_vesa_cursor_enable(int enable);

/* Color helper */
static inline uint32_t vesa_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
