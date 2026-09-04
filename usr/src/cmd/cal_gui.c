/*
 * ==== CAL — graphical calendar ====
 * M4KK1 4P1 - usr/src/cmd/cal_gui.c
 * Description: Copland client with a month grid, month/year
 *              navigation, and per-day event notes stored under
 *              /export/home/$USER/.calendar/YYYY-MM-DD.txt (root
 *              uses /export/root).  One event per line, 6 lines
 *              per day max.
 *
 * Keys:  q/Esc close, Left/Right (also ,/.) prev/next month,
 *       Up/Down prev/next year, Tab next day, Enter add-event
 *       mode (type text, Enter saves, Esc cancels), d delete
 *       the selected day's events, Home jump to today.
 * Click: day cell select (double = add-event mode), arrow
 *       buttons navigate months.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define CL_W    360
#define CL_H    340

#define CL_MB_ADDR    0x006B0000u
#define CL_MB_MAGIC   0x434C3200u   /* "CL2\0" */

#define CL_GRID_X     8
#define CL_GRID_Y     64
#define CL_CELL_W     48
#define CL_CELL_H     34
#define CL_EVENTS_MAX 6
#define CL_EV_LEN     38

static uint32_t cl_buf[CL_W * CL_H];

static struct ga_app app = {
    .w = CL_W, .h = CL_H,
    .mb_addr = CL_MB_ADDR, .mb_magic = CL_MB_MAGIC,
    .title = "Calendar",
};

static int cur_y, cur_m;         /* displayed month */
static int sel_d;                /* 1..31, 0 = none */
static int today_y, today_m, today_d;

static char ev_text[CL_EVENTS_MAX][CL_EV_LEN];
static int ev_lens[CL_EVENTS_MAX];
static int ev_count;

/* add-event input state */
static int input_mode;
static char input_buf[CL_EV_LEN];
static int input_len;

/* month lengths (non-leap Feb patched at runtime) */
static const int mdays[12] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char *mnames[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static int is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int days_in(int y, int m)
{
    if (m == 2 && is_leap(y))
        return 29;
    return mdays[m - 1];
}

/* day-of-week for y-m-d (0=Sun..6=Sat), Sakamoto's method */
static int dow(int y, int m, int d)
{
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3)
        y -= 1;
    int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    return w < 0 ? w + 7 : w;
}

/* events path for the selected day into out */
static void ev_path(char *out, int y, int m, int d)
{
    int o = 0;
    const char *base = m4k_getuid() == 0
        ? "/export/root/.calendar/"
        : "/export/home/makk1/.calendar/";
    while (*base) out[o++] = *base++;
    out[o++] = '0' + (y / 1000) % 10;
    out[o++] = '0' + (y / 100) % 10;
    out[o++] = '0' + (y / 10) % 10;
    out[o++] = '0' + y % 10;
    out[o++] = '-';
    out[o++] = '0' + m / 10;
    out[o++] = '0' + m % 10;
    out[o++] = '-';
    out[o++] = '0' + d / 10;
    out[o++] = '0' + d % 10;
    const char *sfx = ".txt";
    while (*sfx) out[o++] = *sfx++;
    out[o] = 0;
}

static void ev_load(int y, int m, int d)
{
    ev_count = 0;
    char path[64];
    ev_path(path, y, m, d);
    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0)
        return;
    int row = 0, col = 0;
    char b[1];
    while (row < CL_EVENTS_MAX) {
        int n = musr_sc_read(fd, b, 1);
        if (n <= 0)
            break;
        char c = b[0];
        if (c == '\n') {
            if (col > 0) {
                ev_text[row][col] = 0;
                ev_lens[row] = col;
                row++;
                col = 0;
            }
        } else if (c >= 32 && c < 127 && col < CL_EV_LEN - 1) {
            ev_text[row][col++] = c;
        }
    }
    if (col > 0 && row < CL_EVENTS_MAX) {
        ev_text[row][col] = 0;
        ev_lens[row] = col;
        row++;
    }
    ev_count = row;
    musr_sc_close(fd);
}

static void ev_save(int y, int m, int d)
{
    char path[64];
    ev_path(path, y, m, d);
    if (ev_count == 0) {
        musr_sc_unlink(path);
        return;
    }
    int fd = musr_sc_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0)
        return;
    for (int i = 0; i < ev_count; i++) {
        musr_sc_write(fd, ev_text[i], ev_lens[i]);
        musr_sc_write(fd, "\n", 1);
    }
    musr_sc_close(fd);
    ser_puts("[CAL] SAVED ");
    ser_puts(path);
    ser_puts("\n");
}

static void ensure_cal_dir(void)
{
    /* both candidate homes are probed; mkdir is idempotent-ish
     * (fails silently when the dir exists) */
    musr_sc_mkdir("/export/home/makk1");
    musr_sc_mkdir("/export/home/makk1/.calendar");
    musr_sc_mkdir("/export/root/.calendar");
}

static void load_today(void)
{
    uint32_t rtc[8];
    if (musr_sc_rtcread(rtc) == 1) {
        today_y = 2026;
        today_m = 9;
        today_d = 4;
        /* rtc[0]=sec [1]=min [2]=hour [3]=day [4]=month [5]=year */
        if (rtc[5] >= 2000 && rtc[5] <= 2099 && rtc[4] >= 1
            && rtc[4] <= 12 && rtc[3] >= 1 && rtc[3] <= 31) {
            today_y = (int)rtc[5];
            today_m = (int)rtc[4];
            today_d = (int)rtc[3];
        }
    }
    cur_y = today_y;
    cur_m = today_m;
    sel_d = today_d;
}

static void render(void)
{
    ga_rect(&app, 0, 0, CL_W, CL_H, 0x00F0F0F4);
    ga_chrome(&app, "Calendar");

    /* header: prev / month-year / next */
    ga_button(&app, 8, GA_TITLE_H + 6, 24, 20, "<", 0);
    ga_button(&app, CL_W - 32, GA_TITLE_H + 6, 24, 20, ">", 0);
    char hdr[24];
    int o = 0;
    const char *mn = mnames[cur_m - 1];
    while (*mn) hdr[o++] = *mn++;
    hdr[o++] = ' ';
    hdr[o++] = '0' + (cur_y / 1000) % 10;
    hdr[o++] = '0' + (cur_y / 100) % 10;
    hdr[o++] = '0' + (cur_y / 10) % 10;
    hdr[o++] = '0' + cur_y % 10;
    hdr[o] = 0;
    ga_str2(&app, (CL_W - o * 12) / 2, GA_TITLE_H + 8, hdr,
            0x00202020);

    /* weekday header */
    const char *wd[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    for (int i = 0; i < 7; i++)
        ga_str(&app, CL_GRID_X + i * CL_CELL_W
                   + CL_CELL_W / 2 - 6, CL_GRID_Y - 12,
               wd[i], 0x00606060);

    /* day grid: 6 rows x 7 cols */
    int first = dow(cur_y, cur_m, 1);
    int ndays = days_in(cur_y, cur_m);
    for (int d = 1; d <= ndays; d++) {
        int cell = first + d - 1;
        int cx = CL_GRID_X + (cell % 7) * CL_CELL_W;
        int cy = CL_GRID_Y + (cell / 7) * CL_CELL_H;
        int is_today = (d == today_d && cur_m == today_m
                        && cur_y == today_y);
        int is_sel = (d == sel_d);
        uint32_t bg = 0x00FFFFFF;
        if (is_sel)
            bg = 0x003060C0;
        else if (is_today)
            bg = 0x00FFE0B0;
        ga_rect(&app, cx + 1, cy + 1, CL_CELL_W - 2,
                CL_CELL_H - 2, bg);
        char b[4];
        b[0] = '0' + d / 10;
        b[1] = '0' + d % 10;
        b[2] = 0;
        ga_str(&app, cx + 3, cy + 3, b,
               is_sel ? 0x00FFFFFF : 0x00202020);
    }

    /* events panel */
    int ey = CL_GRID_Y + 6 * CL_CELL_H + 8;
    ga_rect(&app, 8, ey, CL_W - 16, CL_H - ey - 6, 0x00FFFFFF);
    ga_str(&app, 12, ey + 4, "Events (Enter=new, d=clear):",
           0x00606060);
    for (int i = 0; i < ev_count; i++)
        ga_str_clip(&app, 12, ey + 16 + i * 10, ev_text[i],
                    (CL_W - 28) / 6, 0x00202020);

    /* input mode indicator */
    if (input_mode) {
        ga_rect(&app, 12, CL_H - 16, CL_W - 24, 12, 0x00FFF0C0);
        ga_str_clip(&app, 14, CL_H - 14, input_buf,
                    (CL_W - 28) / 6, 0x00202020);
        ga_char(&app, 14 + input_len * 6, CL_H - 14, '_',
                0x00C03030);
    }
}

static void select_day(int d)
{
    sel_d = d;
    input_mode = 0;
    ev_load(cur_y, cur_m, d);
}

static void click(int lx, int ly)
{
    /* nav buttons */
    if (ga_in(lx, ly, 8, GA_TITLE_H + 6, 24, 20)) {
        if (--cur_m < 1) { cur_m = 12; cur_y--; }
        select_day(1);
        return;
    }
    if (ga_in(lx, ly, CL_W - 32, GA_TITLE_H + 6, 24, 20)) {
        if (++cur_m > 12) { cur_m = 1; cur_y++; }
        select_day(1);
        return;
    }
    /* grid */
    if (lx >= CL_GRID_X && ly >= CL_GRID_Y) {
        int col = (lx - CL_GRID_X) / CL_CELL_W;
        int row = (ly - CL_GRID_Y) / CL_CELL_H;
        if (col < 7 && row < 6) {
            int d = row * 7 + col - dow(cur_y, cur_m, 1) + 1;
            if (d >= 1 && d <= days_in(cur_y, cur_m)) {
                select_day(d);
                ser_puts("[CAL] SEL day\n");
            }
        }
    }
}

void _start(void)
{
    ser_puts("[CAL] starting\n");
    app.buf = cl_buf;
    if (ga_init(&app) != 0) {
        ser_puts("[CAL] copland not ready\n");
        m4k_exit(1);
    }
    ensure_cal_dir();
    load_today();
    ev_load(cur_y, cur_m, sel_d);
    ser_puts("[CAL] surface ready\n");

    render();
    ga_flip(&app);

    for (;;) {
        if (ga_dead(&app))
            break;
        int ch;
        int dirty = 0;
        int pend = 0, cx = 0;
        while ((ch = ga_getkey(&app)) >= 0) {
            if (pend == 1) { cx = ch; pend = 2; continue; }
            if (pend == 2) { click(cx, ch); dirty = 1; pend = 0; continue; }
            if (ch == 0xF1 || ch == 0xF2) { pend = 1; continue; }
            if (ch == 'q' || ch == 27) {
                if (input_mode) {
                    input_mode = 0;
                    input_len = 0;
                    dirty = 1;
                } else {
                    ser_puts("[CAL] exit\n");
                    m4k_exit(0);
                }
                continue;
            }
            if (input_mode) {
                if (ch == '\n' || ch == '\r') {
                    if (input_len > 0
                        && ev_count < CL_EVENTS_MAX) {
                        for (int i = 0; i < input_len; i++)
                            ev_text[ev_count][i] = input_buf[i];
                        ev_text[ev_count][input_len] = 0;
                        ev_lens[ev_count] = input_len;
                        ev_count++;
                        ev_save(cur_y, cur_m, sel_d);
                    }
                    input_mode = 0;
                    input_len = 0;
                    dirty = 1;
                } else if (ch == 8) {
                    if (input_len) input_len--;
                    dirty = 1;
                } else if (ch >= 32 && ch < 127
                           && input_len < CL_EV_LEN - 1) {
                    input_buf[input_len++] = (char)ch;
                    dirty = 1;
                }
                continue;
            }
            switch (ch) {
            case '<': case ',':
                if (--cur_m < 1) { cur_m = 12; cur_y--; }
                select_day(1);
                dirty = 1;
                break;
            case '>': case '.':
                if (++cur_m > 12) { cur_m = 1; cur_y++; }
                select_day(1);
                dirty = 1;
                break;
            case '\n': case '\r':
                if (sel_d) {
                    input_mode = 1;
                    input_len = 0;
                    dirty = 1;
                }
                break;
            case 'd':
                if (ev_count) {
                    ev_count = 0;
                    ev_save(cur_y, cur_m, sel_d);
                    dirty = 1;
                }
                break;
            case 't':
                cur_y = today_y;
                cur_m = today_m;
                select_day(today_d);
                dirty = 1;
                break;
            }
        }
        if (dirty) {
            render();
            ga_flip(&app);
        }
        m4k_sleep(120);
    }
    m4k_exit(0);
}
