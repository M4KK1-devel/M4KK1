/*
 * M4KK1 4P1 - mdm_mini.c
 * Description: Minimal MDM test program for debugging
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../lib/libgui.h"

/* Global variables required by m4sh.h functions */
int out_fd = 1;
char cwd[256] = "/";

/* Main entry point */
void _start(void) {
    ser_puts("[MDM_MINI] Starting minimal MDM test...\n");
    
    /* Test framebuffer info */
    struct m4k_framebuffer_info fb;
    ser_puts("[MDM_MINI] Getting framebuffer info...\n");
    int ret = m4k_get_framebuffer_info(&fb);
    if (ret < 0) {
        ser_puts("[MDM_MINI] ERROR: Failed to get framebuffer info!\n");
        ser_puts("[MDM_MINI] Exiting...\n");
        return;
    }
    
    ser_puts("[MDM_MINI] Framebuffer info:\n");
    ser_puts("  phys_addr=0x");
    print_u32(fb.phys_addr);
    ser_puts("\n");
    ser_puts("  resolution=");
    print_u32(fb.width);
    ser_puts("x");
    print_u32(fb.height);
    ser_puts("x");
    print_u32(fb.bpp);
    ser_puts("\n");
    ser_puts("  pitch=");
    print_u32(fb.pitch);
    ser_puts("\n");
    
    /* Draw test pattern */
    ser_puts("[MDM_MINI] Drawing test pattern...\n");
    ret = m4k_draw_test_pattern();
    if (ret < 0) {
        ser_puts("[MDM_MINI] ERROR: Failed to draw test pattern!\n");
        ser_puts("[MDM_MINI] Exiting...\n");
        return;
    }
    
    ser_puts("[MDM_MINI] Test pattern drawn successfully!\n");
    ser_puts("[MDM_MINI] MDM mini test completed.\n");
    
    /* 测试完成后直接返回 */
    ser_puts("[MDM_MINI] Exiting...\n");
}
