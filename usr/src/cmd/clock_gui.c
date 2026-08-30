/*
 * ==== CLOCK — graphical desktop clock ====
 * M4KK1 4P1 - usr/src/cmd/clock_gui.c
 * Description: Copland client that shows the RTC time and date,
 *              refreshed once a second.  Q/Esc closes.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define CLK_W   268
#define CLK_H   120

#define CLK_MB_ADDR   0x00640000u
#define CLK_MB_MAGIC  0x434C4B31u   /* "CLK1" */

static uint32_t clk_buf[CLK_W * CLK_H];

static struct ga_app app = {
    .w = CLK_W, .h = CLK_H,
    .mb_addr = CLK_MB_ADDR, .mb_magic = CLK_MB_MAGIC,
    .title = "Clock",
};

static void two(char *p, int v)
{
    p[0] = '0' + (v / 10) % 10;
    p[1] = '0' + v % 10;
}

void _start(void)
{
    ser_puts("[CLOCK] starting\n");
    if (ga_init(&app) != 0) {
        ser_puts("[CLOCK] copland not ready\n");
        m4k_exit(1);
    }
    ser_puts("[CLOCK] surface ready\n");

    int last_sec = -1;
    for (;;) {
        if (ga_dead(&app))
            break;

        int k;
        while ((k = ga_getkey(&app)) >= 0) {
            if (k == 'q' || k == 0x1B)
                goto out;
        }

        uint32_t rtc[6];
        if (musr_sc_rtcread(rtc) == 0 && (int)rtc[5] != last_sec) {
            last_sec = (int)rtc[5];
            char line[16];
            /* HH:MM:SS */
            two(line, (int)rtc[3]); line[2] = ':';
            two(line + 3, (int)rtc[4]); line[5] = ':';
            two(line + 6, (int)rtc[5]); line[8] = 0;
            ga_rect(&app, 0, GA_TITLE_H, CLK_W, CLK_H - GA_TITLE_H,
                    0x00181820);
            ga_str(&app, 44, 34, line, 0x00FFFFFF);
            /* date below: MM/DD/YYYY */
            char d[12];
            two(d, (int)rtc[1]); d[2] = '/';
            two(d + 3, (int)rtc[2]); d[5] = '/';
            ga_itoa((int)rtc[0], d + 6);
            ga_str(&app, 80, 62, d, 0x00909090);
            ga_flip(&app);
        }
        m4k_sleep(200);
    }
out:
    app.shm->surfaces[app.slot].in_use = 0;
    if (app.shm->surface_count > 0)
        app.shm->surface_count--;
    app.shm->dirty = 1;
    m4k_exit(0);
}
