/*
 * ==== LOGVIEW — graphical log viewer ====
 * M4KK1 4P1 - usr/src/cmd/logview.c
 * Description: Copland client that reads /var/log/messages into a
 *              line buffer and lets the user page through it
 *              (PageUp/PageDown / arrows / wheel codes 0x01/0x02).
 *              Filter mode: '/' starts a substring filter (Enter to
 *              apply, Backspace edits, Esc cancels) — matching lines
 *              are shown with the match highlighted; 'F' jumps to
 *              the next match; Esc clears the filter.
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
#define LV_PAT_MAX    32

static uint32_t lv_buf[LV_W * LV_H];

static struct ga_app app = {
    .w = LV_W, .h = LV_H,
    .mb_addr = LV_MB_ADDR, .mb_magic = LV_MB_MAGIC,
    .title = "LogView - /var/log/messages",
};

static char lines[LV_MAX_LINES][LV_LINE_BYTES];
static int line_count = 0;
static int top_line = 0;       /* index into the VISIBLE list */

/* filter state */
static char filter_pat[LV_PAT_MAX + 1];
static int filter_len;         /* 0 = filter off */
static int filter_input;       /* 1 while typing the pattern */
static uint16_t match_line[LV_MAX_LINES];  /* orig idx per match */
static int match_count;
static int cur_match;          /* current match, -1 = none */

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

/**
 * lv_find - first occurrence of pat (plen chars) in s, or -1.
 */
static int lv_find(const char *s, const char *pat, int plen)
{
    for (int i = 0; s[i]; i++) {
        int j = 0;
        while (j < plen && s[i + j] && s[i + j] == pat[j])
            j++;
        if (j == plen)
            return i;
    }
    return -1;
}

/**
 * lv_apply_filter - rebuild match_line[] from filter_pat.
 */
static void lv_apply_filter(void)
{
    match_count = 0;
    cur_match = -1;
    if (filter_len == 0)
        return;
    for (int i = 0; i < line_count && match_count < LV_MAX_LINES; i++)
        if (lv_find(lines[i], filter_pat, filter_len) >= 0)
            match_line[match_count++] = i;
}

static int lv_vis_count(void)
{
    return filter_len ? match_count : line_count;
}

static void lv_render(void)
{
    ga_rect(&app, 0, GA_TITLE_H, LV_W, LV_H - GA_TITLE_H, 0x00181820);
    ga_chrome(&app, app.title);
    int y = GA_TITLE_H + 4;
    int maxch = (LV_W - 8) / 6;
    for (int r = 0; r < LV_ROWS; r++) {
        int vi = top_line + r;
        if (vi >= lv_vis_count())
            break;
        const char *txt = filter_len
            ? lines[match_line[vi]] : lines[vi];
        if (filter_len) {
            /* matching line: highlight the matched substring */
            int occ = lv_find(txt, filter_pat, filter_len);
            if (occ < 0)
                occ = 0;
            ga_str_clip(&app, 4, y, txt, occ, 0x00C8C8C8);
            ga_str_clip(&app, 4 + occ * 6, y, txt + occ,
                        filter_len, 0x00FFD040);
            ga_str_clip(&app, 4 + (occ + filter_len) * 6, y,
                        txt + occ + filter_len, maxch, 0x00C8C8C8);
        } else {
            ga_str_clip(&app, 4, y, txt, maxch, 0x00C8C8C8);
        }
        y += GA_ROW_H;
    }
    /* footer */
    char n1[12], n2[12];
    ga_itoa(match_count, n1);
    ga_itoa(line_count, n2);
    if (filter_input) {
        ga_str(&app, 4, LV_H - 12, "Filter:", 0x00FFD040);
        ga_str(&app, 52, LV_H - 12, filter_pat, 0x00FFFFFF);
        ga_str(&app, 52 + filter_len * 6, LV_H - 12, "_",
               0x00FFD040);
    } else if (filter_len) {
        ga_str(&app, 4, LV_H - 12, "F next match, Esc clear [",
               0x00808080);
        ga_str(&app, 178, LV_H - 12, n1, 0x00FFD040);
        ga_str(&app, 178 + ga_strlen(n1) * 6, LV_H - 12, "/",
               0x00808080);
        ga_str(&app, 184 + ga_strlen(n1) * 6, LV_H - 12, n2,
               0x00808080);
        ga_str(&app, 184 + (ga_strlen(n1) + ga_strlen(n2)) * 6,
               LV_H - 12, "]", 0x00808080);
    } else {
        ga_str(&app, 4, LV_H - 12,
               "PgUp/PgDn scroll, / filter, q close", 0x00808080);
    }
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
            if (filter_input) {
                if (k == '\n') {        /* apply */
                    filter_input = 0;
                    if (filter_len == 0)
                        match_count = 0;
                    else
                        lv_apply_filter();
                    top_line = 0;
                    dirty = 1;
                    ser_puts("[LOGVIEW] FILTER \"");
                    ser_puts(filter_pat);
                    ser_puts("\" ");
                    {
                        char t[12];
                        ga_itoa(match_count, t);
                        ser_puts(t);
                        ser_puts("/");
                        ga_itoa(line_count, t);
                        ser_puts(t);
                    }
                    ser_puts(" lines\n");
                } else if (k == 0x1B) { /* cancel input */
                    filter_input = 0;
                    filter_len = 0;
                    match_count = 0;
                    dirty = 1;
                } else if (k == '\b') {
                    if (filter_len > 0)
                        filter_pat[--filter_len] = 0;
                    dirty = 1;
                } else if (k >= 32 && k < 127 &&
                           filter_len < LV_PAT_MAX) {
                    filter_pat[filter_len++] = (char)k;
                    filter_pat[filter_len] = 0;
                    dirty = 1;
                }
                continue;
            }
            if (k == 'q')
                goto out;
            if (k == 0x1B) {            /* Esc */
                if (filter_len > 0) {   /* clear filter */
                    filter_len = 0;
                    match_count = 0;
                    cur_match = -1;
                    top_line = 0;
                    dirty = 1;
                    ser_puts("[LOGVIEW] FILTER off\n");
                } else {
                    goto out;           /* no filter: quit */
                }
            } else if (k == '/') {      /* enter filter mode */
                filter_input = 1;
                filter_len = 0;
                filter_pat[0] = 0;
                dirty = 1;
            } else if (k == 'F' || k == 'f') {  /* next match */
                if (match_count > 0) {
                    cur_match = (cur_match + 1) % match_count;
                    top_line = cur_match;
                    dirty = 1;
                    char t[12];
                    ser_puts("[LOGVIEW] JUMP #");
                    ga_itoa(cur_match + 1, t);
                    ser_puts(t);
                    ser_puts(" line ");
                    ga_itoa(match_line[cur_match], t);
                    ser_puts(t);
                    ser_puts("\n");
                }
            } else if (k == 0x01 || k == 0x1B + 100) {  /* page up */
                top_line -= LV_ROWS;
                if (top_line < 0) top_line = 0;
                dirty = 1;
            } else if (k == 0x02) {              /* page down */
                top_line += LV_ROWS;
                if (top_line > lv_vis_count() - LV_ROWS)
                    top_line = lv_vis_count() > LV_ROWS
                        ? lv_vis_count() - LV_ROWS : 0;
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
