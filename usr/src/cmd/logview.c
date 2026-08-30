/*
 * ==== LOGVIEW — graphical log viewer ====
 * M4KK1 4P1 - usr/src/cmd/logview.c
 * Description: Copland client that reads /var/log/messages into a
 *              line buffer and lets the user page through it
 *              (PageUp/PageDown / arrows / wheel codes 0x01/0x02).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app */
#define LV_W    420
#define LV_H    300
#define LV_ROWS 24             /* text rows below the title bar */

#define LV_MB_ADDR   0x00650000u
#define LV_MB_MAGIC  0x4C475631u   /* "LGV1" */

#define LV_MAX_LINES 256
#define LV_LINE_BYTES 72

static uint32_t lv_buf[LV_W * LV_H];

static struct ga_app app = {
    .w = LV_W, .h = LV_H,
    .mb_addr = LV_MB_ADDR, .mb_magic = LV_MB_MAGIC,
    .title = "LogView - /var/log/messages",
};

static char lines[LV_MAX_LINES][LV_LINE_BYTES];
static int line_count = 0;
static int top_line = 0;

static void lv_load(void)
{
    int fd = musr_sc_open("/var/log/messages", O_RDONLY);
    if (fd < 0)
        return;
    int n = musr_sc_read(fd, lines[0], LV_MAX_LINES * LV_LINE_BYTES - 4);
    musr_sc_close(fd);
    if (n <= 0)
        return;

    /* split the flat buffer into rows in place (via scratch copy) */
    char *flat = (char *)lines;
    int row = 0, col = 0;
    static char tmp[LV_MAX_LINES * LV_LINE_BYTES];
    if (n > LV_MAX_LINES * LV_LINE_BYTES)
        n = LV_MAX_LINES * LV_LINE_BYTES;
    for (int i = 0; i < n; i++)
        tmp[i] = flat[i];
    for (int i = 0; i < n && row < LV_MAX_LINES; i++) {
        char c = tmp[i];
        if (c == '\n') {
            lines[row][col] = 0;
            row++; col = 0;
        } else if (col < LV_LINE_BYTES - 1) {
            lines[row][col++] = c;
        }
    }
    if (col > 0 && row < LV_MAX_LINES)
        lines[row][col] = 0;
    line_count = row + (col > 0 ? 1 : 0);
}

static void lv_render(void)
{
    ga_rect(&app, 0, GA_TITLE_H, LV_W, LV_H - GA_TITLE_H, 0x00181820);
    ga_chrome(&app, app.title);
    int y = GA_TITLE_H + 4;
    for (int r = 0; r < LV_ROWS; r++) {
        int ln = top_line + r;
        if (ln >= line_count)
            break;
        ga_str_clip(&app, 4, y, lines[ln], (LV_W - 8) / 6, 0x00C8C8C8);
        y += GA_ROW_H;
    }
    /* footer: line x of n */
    char f[32];
    ga_str(&app, 4, LV_H - 12, "PgUp/PgDn scroll, q close",
           0x00808080);
    (void)f;
    ga_flip(&app);
}

void _start(void)
{
    ser_puts("[LOGVIEW] starting\n");
    lv_load();
    if (ga_init(&app) != 0) {
        ser_puts("[LOGVIEW] copland not ready\n");
        m4k_exit(1);
    }
    ser_puts("[LOGVIEW] surface ready\n");
    lv_render();

    for (;;) {
        if (ga_dead(&app))
            break;
        int k;
        int dirty = 0;
        while ((k = ga_getkey(&app)) >= 0) {
            if (k == 'q' || k == 0x1B)
                goto out;
            if (k == 0x01 || k == 0x1B + 100) {  /* page up */
                top_line -= LV_ROWS;
                if (top_line < 0) top_line = 0;
                dirty = 1;
            } else if (k == 0x02) {              /* page down */
                top_line += LV_ROWS;
                if (top_line > line_count - LV_ROWS)
                    top_line = line_count > LV_ROWS
                        ? line_count - LV_ROWS : 0;
                dirty = 1;
            }
        }
        if (dirty)
            lv_render();
        m4k_sleep(100);
    }
out:
    app.shm->surfaces[app.slot].in_use = 0;
    if (app.shm->surface_count > 0)
        app.shm->surface_count--;
    app.shm->dirty = 1;
    m4k_exit(0);
}
