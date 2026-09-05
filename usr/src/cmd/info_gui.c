/*
 * ==== INFO — graphical system info panel ====
 * M4KK1 4P1 - usr/src/cmd/info_gui.c
 * Description: Copland client with two pages:
 *   Page 0 (SYS): memory (sysinfo), uptime, process count, uname.
 *   Page 1 (PROC): full process table (PID/name/state/mem), P/N
 *                 paging, 8 rows per page.  Each render also
 *                 prints the visible rows to the serial console
 *                 ([INFO] PROC ...) so host-side probes can verify
 *                 the table contents without screen scraping.
 *  Tab switches pages; q/ESC closes.  Refreshes every 2 s.
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

#define INF_PROC_ROWS 8             /* process rows per page */

static uint32_t inf_buf[INF_W * INF_H];

static struct ga_app app = {
    .w = INF_W, .h = INF_H,
    .mb_addr = INF_MB_ADDR, .mb_magic = INF_MB_MAGIC,
    .title = "System Info",
};

static int inf_page;                /* 0 = sys, 1 = processes */
static int inf_pgno;                /* process-list page index */
static struct procinfo inf_procs[PROCBUF_MAX];
static int inf_nprocs;

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

static const char *state_str3(uint32_t st)
{
    switch (st) {
    case 0:  return "RUN";
    case 1:  return "RDY";
    case 2:  return "BLK";
    case 3:  return "TER";
    default: return "???";
    }
}

/* pad dst with leading spaces to width w (right-aligned number) */
static void pad_num(char *dst, int v, int w)
{
    char t[12];
    ga_itoa(v, t);
    int n = 0;
    while (t[n]) n++;
    int p = 0;
    while (p < w - n) dst[p++] = ' ';
    int q = 0;
    while (t[q]) dst[p++] = t[q++];
    dst[p] = 0;
}

/* append src at end of dst */
static void append(char *dst, const char *src, int cap)
{
    int i = 0;
    while (dst[i]) i++;
    int j = 0;
    while (src[j] && i < cap - 1) dst[i++] = src[j++];
    dst[i] = 0;
}

static void inf_render_sys(void)
{
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
}

/* Build one table row "PID NAME STATE MEM" into line (<=62 chars). */
static void proc_row(char *line, const struct procinfo *pi)
{
    /* PID right-aligned width 3 */
    char num[12];
    pad_num(num, (int)pi->pid, 3);
    line[0] = 0;
    append(line, num, 62);
    append(line, " ", 62);
    append(line, pi->name, 62);
    /* pad name field to 14 chars */
    int n = 0;
    while (line[n]) n++;
    while (n < 4 + 14) line[n++] = ' ';
    line[n] = 0;
    append(line, state_str3(pi->state), 62);
    append(line, " ", 62);
    ga_itoa((int)pi->mem_kb, num);
    append(line, num, 62);
    append(line, "K", 62);
}

static void inf_render_proc(void)
{
    inf_nprocs = musr_sc_getprocs(inf_procs, PROCBUF_MAX);
    if (inf_nprocs < 0)
        inf_nprocs = 0;

    int pages = (inf_nprocs + INF_PROC_ROWS - 1) / INF_PROC_ROWS;
    if (pages < 1) pages = 1;
    if (inf_pgno >= pages) inf_pgno = pages - 1;
    if (inf_pgno < 0) inf_pgno = 0;

    char line[64], num[12];

    /* header */
    ga_str(&app, 8, GA_TITLE_H + 6,
           "PID  NAME           ST  MEM", 0x00FFB060);
    ga_rect(&app, 8, GA_TITLE_H + 15, INF_W - 16, 1, 0x00404050);

    int y = GA_TITLE_H + 20;
    int first = inf_pgno * INF_PROC_ROWS;
    char tag[24] = "[INFO] PROC p=";
    char tag2[24];

    for (int r = 0; r < INF_PROC_ROWS; r++) {
        int idx = first + r;
        if (idx >= inf_nprocs)
            break;
        proc_row(line, &inf_procs[idx]);
        ga_str(&app, 8, y, line, 0x00C8C8C8);
        y += GA_ROW_H + 2;
    }

    /* footer: page x/y + total */
    pad_num(num, inf_pgno + 1, 2);
    line[0] = 0;
    append(line, "Page ", 62);
    append(line, num, 62);
    append(line, "/", 62);
    ga_itoa(pages, num);
    append(line, num, 62);
    append(line, "  total ", 62);
    ga_itoa(inf_nprocs, num);
    append(line, num, 62);
    ga_str(&app, 8, INF_H - 26, line, 0x00808080);

    /* serial dump of the visible rows: host probe evidence */
    tag[0] = 0;
    append(tag, "[INFO] PROC p=", 23);
    ga_itoa(inf_pgno, tag2);
    append(tag, tag2, 23);
    append(tag, "/", 23);
    ga_itoa(pages - 1, tag2);
    append(tag, tag2, 23);
    append(tag, " n=", 23);
    ga_itoa(inf_nprocs, tag2);
    append(tag, tag2, 23);
    append(tag, "\n", 23);
    ser_puts(tag);
    for (int r = 0; r < INF_PROC_ROWS; r++) {
        int idx = first + r;
        if (idx >= inf_nprocs)
            break;
        proc_row(line, &inf_procs[idx]);
        ser_puts("  ");
        ser_puts(line);
        ser_puts("\n");
    }
}

static void inf_render(void)
{
    ga_rect(&app, 0, GA_TITLE_H, INF_W, INF_H - GA_TITLE_H, 0x00181820);
    ga_chrome(&app, app.title);

    if (inf_page == 1)
        inf_render_proc();
    else
        inf_render_sys();

    ga_str(&app, 8, INF_H - 14,
           inf_page ? "Tab sys  P/N page  q close"
                    : "Tab procs  q close", 0x00808080);
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
            if (k == '\t') {
                inf_page ^= 1;
                inf_pgno = 0;
                inf_render();
            } else if (inf_page == 1
                       && (k == 'n' || k == 'N')) {
                inf_pgno++;
                inf_render();
            } else if (inf_page == 1
                       && (k == 'p' || k == 'P')) {
                if (inf_pgno > 0)
                    inf_pgno--;
                inf_render();
            }
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
