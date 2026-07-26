/*
 * M4KK1 4P1 - flip_test.c
 * Description: Continuous VESA flip test - draws animated pattern
 *
 * This program continuously redraws and flips the framebuffer to verify
 * that VESA graphics display works correctly in QEMU.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/* Global variables required by m4sh.h functions */
int out_fd = 1;
char cwd[256] = "/";

#define FB_W 800
#define FB_H 600

/* Colors */
#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_BLACK   0x00000000
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_MAGENTA 0x00FF00FF

/* Simple delay - short delay for animation */
static void delay_short(void)
{
    for (volatile uint32_t i = 0; i < 500000; i++);
}

/* Main entry point */
void _start(void)
{
    ser_puts("[FLIP_TEST] ========================================\n");
    ser_puts("[FLIP_TEST] Continuous VESA flip test starting\n");
    ser_puts("[FLIP_TEST] ========================================\n");

    /* Get framebuffer info */
    struct m4k_framebuffer_info fb;
    int ret = m4k_get_framebuffer_info(&fb);
    if (ret < 0) {
        ser_puts("[FLIP_TEST] ERROR: Cannot get framebuffer info!\n");
        return;
    }

    ser_puts("[FLIP_TEST] FB: ");
    print_u32(fb.width);
    ser_puts("x");
    print_u32(fb.height);
    ser_puts(" @ 0x");
    print_u32(fb.phys_addr);
    ser_puts("\n");

    volatile uint32_t *lfb = (volatile uint32_t *)fb.phys_addr;
    int frame = 0;

    /* Step 1: Direct LFB write test (no syscalls) */
    {
        ser_puts("[FLIP_TEST] Direct LFB write test at 0x");
        print_u32(fb.phys_addr);
        ser_puts("\n");
        lfb[0] = 0x00FF0000;
        lfb[1] = 0x0000FF00;
        lfb[100] = 0x000000FF;
        uint32_t dr0 = lfb[0];
        uint32_t dr1 = lfb[1];
        uint32_t dr2 = lfb[100];
        ser_puts("[FLIP_TEST] direct wrote FF0000 00FF00 0000FF\n");
        ser_puts("[FLIP_TEST] direct read: ");
        print_u32(dr0);
        ser_puts(" ");
        print_u32(dr1);
        ser_puts(" ");
        print_u32(dr2);
        ser_puts("\n");
        if (dr0 == 0x00FF0000 && dr1 == 0x0000FF00 && dr2 == 0x000000FF)
            ser_puts("[FLIP_TEST] DIRECT LFB WRITE: PASS\n");
        else
            ser_puts("[FLIP_TEST] DIRECT LFB WRITE: FAIL\n");
    }

    ser_puts("[FLIP_TEST] Entering animation loop NOW...\n");

    for (;;) {
        uint32_t color;
        if (frame % 3 == 0) color = COLOR_RED;
        else if (frame % 3 == 1) color = COLOR_GREEN;
        else color = COLOR_BLUE;

        m4k_draw_rect(0, 0, FB_W, FB_H, color);

        int px = (frame * 7) % (FB_W - 20);
        int py = (frame * 5) % (FB_H - 20);
        m4k_draw_rect(px, py, 10, 10, COLOR_WHITE);

        m4k_flip();

        if (frame % 60 == 0) {
            ser_puts("[FLIP_TEST] frame=");
            print_u32((uint32_t)frame);
            ser_puts("\n");
        }

        frame++;
        delay_short();
    }
}