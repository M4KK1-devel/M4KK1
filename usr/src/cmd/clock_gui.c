/*
 * ==== CLOCK — graphical desktop clock with alarms ====
 * M4KK1 4P1 - usr/src/cmd/clock_gui.c
 * Description: Copland client that shows the RTC time and date,
 *              refreshed once a second.  Q/Esc closes.
 *              Keys (window focused):
 *                a — add an alarm one minute from now (max 4)
 *                d — delete the last alarm
 *              When the RTC reaches an alarm minute, the title bar
 *              flashes red and the beeper sounds for 15 seconds (any
 *              key silences it).  Alarm state is printed to serial
 *              as evidence lines for probes.
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

#define CLK_MAX_ALARMS 4
#define CLK_RING_SECS  15

struct clk_alarm {
	int hh, mm;        /* trigger time (RTC wall clock)      */
	int armed;         /* 1 = waiting, 2 = ringing           */
	uint32_t ring_end; /* uptime ms deadline while ringing   */
};

static uint32_t clk_buf[CLK_W * CLK_H];

static struct ga_app app = {
    .w = CLK_W, .h = CLK_H,
    .mb_addr = CLK_MB_ADDR, .mb_magic = CLK_MB_MAGIC,
    .title = "Clock",
};

static struct clk_alarm alarms[CLK_MAX_ALARMS];
static int n_alarms;

static void two(char *p, int v)
{
    p[0] = '0' + (v / 10) % 10;
    p[1] = '0' + v % 10;
}

/* serial evidence line: "[CLOCK] ALARM ADD 12:34" style */
static void ser_alarm(const char *tag, int hh, int mm)
{
	char b[10];
	b[0] = '['; b[1] = 'C'; b[2] = 'L'; b[3] = 'O'; b[4] = 'C';
	b[5] = 'K'; b[6] = ']'; b[7] = ' '; b[8] = 0;
	ser_puts(b);
	ser_puts(tag);
	b[0] = '0' + (hh / 10) % 10; b[1] = '0' + hh % 10; b[2] = ':';
	b[3] = '0' + (mm / 10) % 10; b[4] = '0' + mm % 10; b[5] = '\n';
	b[6] = 0;
	ser_puts(b);
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
            if (k == 'a' && n_alarms < CLK_MAX_ALARMS) {
                /* arm for one minute from now */
                uint32_t rtc[6];
                if (musr_sc_rtcread(rtc) == 0) {
                    int hh = (int)rtc[3], mm = (int)rtc[4];
                    int ss = (int)rtc[5];
                    mm++;
                    if (mm >= 60) { mm = 0; hh = (hh + 1) % 24; }
                    (void)ss;
                    alarms[n_alarms].hh = hh;
                    alarms[n_alarms].mm = mm;
                    alarms[n_alarms].armed = 1;
                    alarms[n_alarms].ring_end = 0;
                    n_alarms++;
                    ser_alarm("ALARM ADD ", hh, mm);
                }
            } else if (k == 'a') {
                ser_puts("[CLOCK] ALARM FULL\n");
            } else if (k == 'd' && n_alarms > 0) {
                n_alarms--;
                ser_alarm("ALARM DEL ",
                          alarms[n_alarms].hh, alarms[n_alarms].mm);
            } else if (k == 'd') {
                ser_puts("[CLOCK] ALARM EMPTY\n");
            } else if (n_alarms > 0 &&
                       alarms[n_alarms - 1].armed == 2) {
                /* any other key silences a ringing alarm */
                alarms[n_alarms - 1].armed = 0;
                ser_puts("[CLOCK] ALARM OFF\n");
            }
        }

        uint32_t rtc[6];
        if (musr_sc_rtcread(rtc) == 0 && (int)rtc[5] != last_sec) {
            last_sec = (int)rtc[5];
            int hh = (int)rtc[3], mm = (int)rtc[4];

            /* arm → ring transition on the exact minute */
            for (int i = 0; i < n_alarms; i++) {
                if (alarms[i].armed == 1 &&
                    hh == alarms[i].hh && mm == alarms[i].mm) {
                    alarms[i].armed = 2;
                    alarms[i].ring_end = musr_sc_uptime() +
                                         CLK_RING_SECS * 1000u;
                    ser_alarm("ALARM RING ", hh, mm);
                }
            }

            char line[16];
            /* HH:MM:SS */
            two(line, hh); line[2] = ':';
            two(line + 3, mm); line[5] = ':';
            two(line + 6, (int)rtc[5]); line[8] = 0;

            /* title bar: solid normally, red blink while ringing */
            int ringing = 0;
            for (int i = 0; i < n_alarms; i++)
                if (alarms[i].armed == 2)
                    ringing = 1;
            uint32_t tcol = 0x00A05010;
            if (ringing && (rtc[5] & 1))
                tcol = 0x00E03030;
            ga_rect(&app, 0, 0, CLK_W, GA_TITLE_H, tcol);
            ga_str(&app, 4, 5, "Clock", 0x00FFFFFF);
            int cx = CLK_W - GA_CLOSE_W - 3;
            ga_rect(&app, cx, 3, GA_CLOSE_W, GA_CLOSE_W, 0x00C03030);
            ga_char(&app, cx + 3, 5, 'x', 0x00FFFFFF);

            ga_rect(&app, 0, GA_TITLE_H, CLK_W, CLK_H - GA_TITLE_H,
                    0x00181820);
            ga_str(&app, 44, 34, line, 0x00FFFFFF);
            /* date below: MM/DD/YYYY */
            char d[12];
            two(d, (int)rtc[1]); d[2] = '/';
            two(d + 3, (int)rtc[2]); d[5] = '/';
            ga_itoa((int)rtc[0], d + 6);
            ga_str(&app, 80, 62, d, 0x00909090);

            /* alarm status line (probe target) */
            char aline[30];
            char *p = aline;
            const char *tag = "ALARMS:";
            for (const char *q = tag; *q; ) *p++ = *q++;
            for (int i = 0; i < n_alarms; i++) {
                if (p - aline + 6 > 28)
                    break;
                if (i > 0)
                    *p++ = ' ';
                two(p, alarms[i].hh); p += 2;
                *p++ = ':';
                two(p, alarms[i].mm); p += 2;
                if (alarms[i].armed == 2) {
                    *p++ = '*';
                }
            }
            *p = 0;
            ga_str(&app, 4, 94, aline,
                   ringing ? 0x00FF6060 : 0x00909090);

            ga_flip(&app);
        }

        /* beeper + ring expiry (runs every loop, not just on new sec) */
        uint32_t now = musr_sc_uptime();
        for (int i = 0; i < n_alarms; i++) {
            if (alarms[i].armed != 2)
                continue;
            if ((int32_t)(now - alarms[i].ring_end) >= 0) {
                alarms[i].armed = 0;
                ser_puts("[CLOCK] ALARM DONE\n");
                continue;
            }
            /* beep in a 1s on/1s off cadence (600 ms tone) */
            if ((now / 1000u) % 2u == 0u)
                m4k_beep(880, 600);
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
