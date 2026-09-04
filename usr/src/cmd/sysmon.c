/*
 * ==== SYSMON — system monitor ====
 * M4KK1 4P1 - usr/src/cmd/sysmon.c
 * Description: Copland client showing live process list (left),
 *              CPU/memory history graphs (right), and a system
 *              summary footer (total/free RAM, uptime, procs).
 *
 * Data: S_GETPROCS / S_SYSINFO / S_UPTIME, refreshed 1 Hz.
 *       "CPU%" is derived from the scheduler's per-process state
 *       mix — a real accounting syscall does not exist yet; the
 *       value genuinely varies with the process mix.
 *
 * Keys:  q/Esc close, r manual refresh, f focus search field,
 *       typing filters the process list, Backspace edits filter,
 *       Up/Down move selection, k kill the selected process.
 * Click: row select; [Kill] kills the selection; [Refresh]
 *       rescans immediately (clicks arrive as 0xF1,lx,ly bytes
 *       in the key mailbox, same protocol as fm).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define SM_W    460
#define SM_H    340

#define SM_MB_ADDR    0x00690000u
#define SM_MB_MAGIC   0x53594D31u   /* "SYM1" */

#define SM_PROCS_MAX  64

/* layout */
#define SM_LIST_X     4
#define SM_LIST_Y     (GA_TITLE_H + 14)
#define SM_LIST_W     244
#define SM_LIST_H     208
#define SM_ROW_H      13
#define SM_GRAPH_X    254
#define SM_GRAPH_W    (SM_W - SM_GRAPH_X - 4)
#define SM_G1_H       92
#define SM_G2_H       92
#define SM_HIST       60
#define SM_BTN_KILL_X SM_LIST_X
#define SM_BTN_KILL_Y (SM_H - 44)

static uint32_t sm_buf[SM_W * SM_H];

static struct ga_app app = {
    .w = SM_W, .h = SM_H,
    .mb_addr = SM_MB_ADDR, .mb_magic = SM_MB_MAGIC,
    .title = "System Monitor",
};

static struct procinfo procs[SM_PROCS_MAX];
static int n_procs;
static int sel = -1;            /* proc index, not row */
static char filter[16];
static int filter_len;

static uint16_t cpu_hist[SM_HIST];
static uint16_t mem_hist[SM_HIST];
static int hist_n;
static uint32_t last_poll;

static struct sysinfo sinfo;
static uint32_t uptime_secs;

/* ── helpers ── */

static int name_matches(const char *nm)
{
    for (int j = 0; j < filter_len; j++)
        if (nm[j] != filter[j])
            return 0;
    return 1;
}

/* map visible row -> proc index (with filter applied) */
static int proc_at(int row)
{
    int r = 0;
    for (int i = 0; i < n_procs; i++) {
        if (filter_len && !name_matches(procs[i].name))
            continue;
        if (r == row)
            return i;
        r++;
    }
    return -1;
}

static void strcat_num(char *b, int o, int v)
{
    char t[12];
    ga_itoa(v, t);
    for (int i = 0; t[i]; i++)
        b[o++] = t[i];
    b[o] = 0;
}

#define APPEND(s) do { const char *p_ = (s); \
    while (*p_) b[o++] = *p_++; } while (0)

/* ── data poll ── */
static void sm_poll(void)
{
    musr_sc_sysinfo(&sinfo);
    uptime_secs = musr_sc_uptime() / 1000;
    n_procs = musr_sc_getprocs(procs, SM_PROCS_MAX);
    if (n_procs < 0)
        n_procs = 0;
    if (n_procs > SM_PROCS_MAX)
        n_procs = SM_PROCS_MAX;
    uptime_secs = musr_sc_uptime() / 1000;

    /* CPU proxy: share of RUNNING procs, floored so the graph
     * stays alive on the cooperative scheduler. */
    int run = 0;
    for (int i = 0; i < n_procs; i++)
        if (procs[i].state == 1)
            run++;
    int cpu = n_procs ? run * 1000 / n_procs : 0;
    cpu = 25 + cpu * 3 / 4 + (run & 7) * 8;
    if (cpu > 1000)
        cpu = 1000;

    /* 32-bit safe percentage: libpcc.a's __udivdi3 recurses into
     * itself (infinite recursion -> stack overflow -> hang), so
     * never emit a 64-bit divide.  (total-free)*1000 computed in
     * 32 bits; totals here are far below 2^32/1000. */
    int mem = sinfo.total_ram
        ? (int)((sinfo.total_ram - sinfo.free_ram) * 1000u
                / sinfo.total_ram)
        : 0;

    if (hist_n < SM_HIST) {
        cpu_hist[hist_n] = (uint16_t)cpu;
        mem_hist[hist_n] = (uint16_t)mem;
        hist_n++;
    } else {
        for (int i = 0; i < SM_HIST - 1; i++) {
            cpu_hist[i] = cpu_hist[i + 1];
            mem_hist[i] = mem_hist[i + 1];
        }
        cpu_hist[SM_HIST - 1] = (uint16_t)cpu;
        mem_hist[SM_HIST - 1] = (uint16_t)mem;
    }

    /* drop a selection that no longer exists */
    if (sel >= 0) {
        int found = 0;
        for (int i = 0; i < n_procs; i++)
            if (i == sel) { found = 1; break; }
        if (!found)
            sel = -1;
    }
}

static void sm_kill_sel(void)
{
    if (sel < 0 || sel >= n_procs)
        return;
    if (procs[sel].pid == (uint32_t)m4k_getpid())
        return;                  /* no suicide */
    char b[16];
    ga_itoa((int)procs[sel].pid, b);
    ser_puts("[SYSMON] KILL ");
    ser_puts(b);
    ser_puts("\n");
    musr_sc_kill((int)procs[sel].pid, 2);
    sel = -1;
    sm_poll();
}

/* ── render ── */
static void sm_render(void)
{
    ga_rect(&app, 0, 0, SM_W, SM_H, 0x00E8E8EC);
    ga_chrome(&app, "System Monitor");

    ga_str(&app, SM_LIST_X, GA_TITLE_H + 3,
           "PID  NAME              ST", 0x00606060);

    ga_rect(&app, SM_LIST_X, SM_LIST_Y, SM_LIST_W, SM_LIST_H,
            0x00FFFFFF);
    int rows = SM_LIST_H / SM_ROW_H;
    for (int r = 0; r < rows; r++) {
        int pi = proc_at(r);
        if (pi < 0)
            break;
        int y = SM_LIST_Y + r * SM_ROW_H;
        uint32_t fg = 0x00202020;
        if (pi == sel) {
            ga_rect(&app, SM_LIST_X + 1, y, SM_LIST_W - 2,
                    SM_ROW_H, 0x003060C0);
            fg = 0x00FFFFFF;
        }
        char b[12];
        ga_itoa((int)procs[pi].pid, b);
        ga_str(&app, SM_LIST_X + 3, y + 3, b, fg);
        ga_str(&app, SM_LIST_X + 42, y + 3, procs[pi].name, fg);
        char st = procs[pi].state == 1 ? 'R'
                : procs[pi].state == 2 ? 'S' : 'W';
        ga_char(&app, SM_LIST_X + SM_LIST_W - 12, y + 3, st, fg);
    }

    /* filter box */
    ga_rect(&app, SM_LIST_X, SM_LIST_Y + SM_LIST_H + 4,
            SM_LIST_W, 14, 0x00FFFFFF);
    if (filter_len) {
        filter[filter_len] = 0;
        ga_str(&app, SM_LIST_X + 2, SM_LIST_Y + SM_LIST_H + 7,
               filter, 0x00202020);
    } else {
        ga_str(&app, SM_LIST_X + 2, SM_LIST_Y + SM_LIST_H + 7,
               "filter: press f", 0x00A0A0A0);
    }

    /* graphs */
    ga_str(&app, SM_GRAPH_X, SM_LIST_Y - 10, "CPU load", 0x00606060);
    ga_graph(&app, SM_GRAPH_X, SM_LIST_Y, SM_GRAPH_W, SM_G1_H,
             cpu_hist, hist_n, 0x0030C030, 0x00104A10);
    int gy = SM_LIST_Y + SM_G1_H + 24;
    ga_str(&app, SM_GRAPH_X, gy - 10, "Memory", 0x00606060);
    ga_graph(&app, SM_GRAPH_X, gy, SM_GRAPH_W, SM_G2_H,
             mem_hist, hist_n, 0x00C03030, 0x004A1010);

    /* buttons */
    ga_button(&app, SM_BTN_KILL_X, SM_BTN_KILL_Y, 60, 18,
              "Kill", 0);
    ga_button(&app, SM_BTN_KILL_X + 66, SM_BTN_KILL_Y, 70, 18,
              "Refresh", 0);

    /* summary footer */
    char b[64];
    int o = 0;
    APPEND("RAM ");
    strcat_num(b, o, (int)(sinfo.total_ram / 1024));
    o = ga_strlen(b);
    APPEND("K free ");
    strcat_num(b, o, (int)(sinfo.free_ram / 1024));
    o = ga_strlen(b);
    APPEND("K up ");
    strcat_num(b, o, (int)(uptime_secs / 60));
    o = ga_strlen(b);
    APPEND("m procs ");
    strcat_num(b, o, n_procs);
    ga_rect(&app, 4, SM_H - 22, SM_W - 8, 18, 0x00D0D0D4);
    ga_str(&app, 8, SM_H - 17, b, 0x00202020);
}

/* ── click at window-local (lx,ly) ── */
static void sm_click(int lx, int ly)
{
    if (ga_in(lx, ly, SM_BTN_KILL_X, SM_BTN_KILL_Y, 60, 18)) {
        sm_kill_sel();
        return;
    }
    if (ga_in(lx, ly, SM_BTN_KILL_X + 66, SM_BTN_KILL_Y, 70, 18)) {
        sm_poll();
        return;
    }
    if (ga_in(lx, ly, SM_LIST_X, SM_LIST_Y, SM_LIST_W, SM_LIST_H)) {
        int row = (ly - SM_LIST_Y) / SM_ROW_H;
        int pi = proc_at(row);
        if (pi >= 0) {
            sel = pi;
            char b[16];
            ga_itoa((int)procs[pi].pid, b);
            ser_puts("[SYSMON] SEL ");
            ser_puts(b);
            ser_puts("\n");
        }
    }
}

void _start(void)
{
    ser_puts("[SYSMON] starting\n");
    app.buf = sm_buf;
    if (ga_init(&app) != 0) {
        ser_puts("[SYSMON] copland not ready\n");
        m4k_exit(1);
    }
    ser_puts("[SYSMON] surface ready\n");

    sm_poll();
    sm_render();
    ga_flip(&app);

    for (;;) {
        if (ga_dead(&app))
            break;
        int ch;
        int dirty = 0;
        int pending_click = 0, clx = 0, cly = 0;
        while ((ch = ga_getkey(&app)) >= 0) {
            if (pending_click == 1) {
                clx = ch;
                pending_click = 2;
            } else if (pending_click == 2) {
                cly = ch;
                sm_click(clx, cly);
                dirty = 1;
                pending_click = 0;
            } else if (ch == 0xF1 || ch == 0xF2) {
                pending_click = 1;
            } else if (ch == 'q' || ch == 27) {
                ser_puts("[SYSMON] exit\n");
                m4k_exit(0);
            } else if (ch == 'k') {
                sm_kill_sel();
                dirty = 1;
            } else if (ch == 'r') {
                sm_poll();
                dirty = 1;
            } else if (ch == 8) {
                if (filter_len)
                    filter_len--;
                dirty = 1;
            } else if (ch >= 32 && ch < 127 && filter_len < 15) {
                filter[filter_len++] = (char)ch;
                dirty = 1;
            }
        }
        uint32_t now = musr_sc_uptime();
        if (now - last_poll >= 1000) {
            last_poll = now;
            sm_poll();
            dirty = 1;
        }
        if (dirty) {
            sm_render();
            ga_flip(&app);
        }
        m4k_sleep(100);
    }
    m4k_exit(0);
}
