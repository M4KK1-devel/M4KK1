/*
 * ==== INFO — graphical system info panel ====
 * M4KK1 4P1 - usr/src/cmd/info_gui.c
 * Description: Copland client showing memory (sysinfo), uptime,
 *              process count and uname data.  Refreshes every 2 s.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app */
#define INF_W   380
#define INF_H   240

#define INF_MB_ADDR   0x00660000u
#define INF_MB_MAGIC  0x494E4631u   /* "INF1" */

static uint32_t inf_buf[INF_W * INF_H];

static struct ga_app app = {
    .w = INF_W, .h = INF_H,
    .mb_addr = INF_MB_ADDR, .mb_magic = INF_MB_MAGIC,
    .title = "System Info",
};

static void fmt_kb(char *dst, uint32_t kb)
{
    /* "12345 KB (12 MB)" */
    ga_itoa((int)kb, dst);
    int i = 0;
    while (dst[i]) i++;
    dst[i++] = ' '; dst[i++] = 'K'; dst[i++] = 'B';
    dst[i++] = ' '; dst[i++] = '(';
    ga_itoa((int)(kb / 1024), dst + i);
    while (dst[i]) i++;
    dst[i++] = ' '; dst[i++] = 'M'; dst[i++] = 'B'; dst[i++] = ')';
    dst[i] = 0;
}

static void row_str(char *dst, const char *label, const char *val)
{
    int i = 0;
    while (label[i] && i < 20) { dst[i] = label[i]; i++; }
    while (i < 22) dst[i++] = ' ';
    int j = 0;
    while (val[j] && i < 60) dst[i++] = val[j++];
    dst[i] = 0;
}

static void inf_render(void)
{
    ga_rect(&app, 0, GA_TITLE_H, INF_W, INF_H - GA_TITLE_H, 0x00181820);
    ga_chrome(&app, app.title);

    char v[40], line[64];
    int y = GA_TITLE_H + 8;

    struct sysinfo si;
    if (musr_sc_sysinfo(&si) == 0) {
        fmt_kb(v, si.total_ram / 1024);
        row_str(line, "Memory total:", v);
        ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;

        fmt_kb(v, si.used_ram / 1024);
        row_str(line, "Memory used:", v);
        ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;

        ga_itoa((int)si.process_count, v);
        row_str(line, "Processes:", v);
        ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;
    } else {
        row_str(line, "sysinfo:", "unavailable");
        ga_str(&app, 8, y, line, 0x00C06060); y += GA_ROW_H + 2;
    }

    uint32_t up_ms = musr_sc_uptime();
    uint32_t up_s = up_ms / 1000;
    ga_itoa((int)(up_s / 60), v);
    int i = 0; while (v[i]) i++;
    v[i++] = 'm'; v[i++] = ' ';
    ga_itoa((int)(up_s % 60), v + i);
    while (v[i]) i++;
    v[i++] = 's'; v[i] = 0;
    row_str(line, "Uptime:", v);
    ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;

    struct utsname un;
    if (musr_sc_uname(&un) == 0) {
        row_str(line, "OS:", un.sysname);
        ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;
        row_str(line, "Version:", un.release);
        ga_str(&app, 8, y, line, 0x00C8C8C8); y += GA_ROW_H + 2;
    }

    ga_str(&app, 8, INF_H - 14, "q close", 0x00808080);
    ga_flip(&app);
}

void _start(void)
{
    ser_puts("[INFO] starting\n");
    if (ga_init(&app) != 0) {
        ser_puts("[INFO] copland not ready\n");
        m4k_exit(1);
    }
    ser_puts("[INFO] surface ready\n");
    inf_render();

    int tick = 0;
    for (;;) {
        if (ga_dead(&app))
            break;
        int k;
        while ((k = ga_getkey(&app)) >= 0) {
            if (k == 'q' || k == 0x1B)
                goto out;
        }
        if (++tick >= 20) {         /* ~2 s at 100 ms sleep */
            tick = 0;
            inf_render();
        }
        m4k_sleep(100);
    }
out:
    app.shm->surfaces[app.slot].in_use = 0;
    if (app.shm->surface_count > 0)
        app.shm->surface_count--;
    app.shm->dirty = 1;
    m4k_exit(0);
}
