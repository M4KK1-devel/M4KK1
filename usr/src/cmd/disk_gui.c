/*
 * ==== DISK — graphical disk manager ====
 * M4KK1 4P1 - usr/src/cmd/disk_gui.c
 * Description: Copland client listing mounted filesystems from
 *              S_MOUNTINFO + S_STATFS (source, target, fstype,
 *              capacity, used permille bar).  Mount/unmount are
 *              root-only and operate on the selected row's target.
 *
 * Keys:  q/Esc close, r refresh, m mount selection (root),
 *       u unmount selection (root).
 * Click: row select; [Mount] [Unmount] [Refresh] buttons.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "guiapp.h"

int out_fd = 1;
char cwd[256] = "/";

/* width unique per GUI app (sprach keyboard dispatch table) */
#define DK_W    424
#define DK_H    300

#define DK_MB_ADDR    0x006C0000u
#define DK_MB_MAGIC   0x44534B31u   /* "DSK1" */

#define DK_LIST_X     6
#define DK_LIST_Y     (GA_TITLE_H + 16)
#define DK_ROW_H      46
#define DK_ROWS       5
#define DK_BTN_Y      (DK_H - 40)

static uint32_t dk_buf[DK_W * DK_H];

static struct ga_app app = {
    .w = DK_W, .h = DK_H,
    .mb_addr = DK_MB_ADDR, .mb_magic = DK_MB_MAGIC,
    .title = "Disk Manager",
};

static struct mount_entry mounts[MOUNT_MAX];
static int n_mounts;
static int sel = -1;

static void poll(void)
{
    int n = musr_sc_mountinfo(mounts, MOUNT_MAX);
    n_mounts = n < 0 ? 0 : (n > MOUNT_MAX ? MOUNT_MAX : n);
    if (sel >= n_mounts)
        sel = n_mounts - 1;
    ser_puts("[DISK] mounts ");
    char b[8];
    ga_itoa(n_mounts, b);
    ser_puts(b);
    ser_puts("\n");
}

static void do_mount(void)
{
    if (sel < 0 || sel >= n_mounts)
        return;
    if (m4k_getuid() != 0) {
        ser_puts("[DISK] MOUNT denied (not root)\n");
        return;
    }
    ser_puts("[DISK] MOUNT ");
    ser_puts(mounts[sel].target);
    ser_puts("\n");
    musr_sc_mount(mounts[sel].source, mounts[sel].target,
                  mounts[sel].fstype);
    poll();
}

static void do_umount(void)
{
    if (sel < 0 || sel >= n_mounts)
        return;
    if (m4k_getuid() != 0) {
        ser_puts("[DISK] UMOUNT denied (not root)\n");
        return;
    }
    ser_puts("[DISK] UMOUNT ");
    ser_puts(mounts[sel].target);
    ser_puts("\n");
    musr_sc_umount(mounts[sel].target);
    poll();
}

/* render one row: name/target/fstype + capacity bar */
static void render_row(int i, int y, struct statfs *st)
{
    int selrow = (i == sel);
    ga_rect(&app, DK_LIST_X, y, DK_W - 12, DK_ROW_H - 6,
            selrow ? 0x00D8E4F8 : 0x00FFFFFF);
    ga_str(&app, DK_LIST_X + 4, y + 4, mounts[i].source,
           0x00202020);
    ga_str(&app, DK_LIST_X + 4, y + 14, mounts[i].target,
           0x00404040);
    ga_str(&app, DK_LIST_X + 190, y + 14, mounts[i].fstype,
           0x00806010);
    const char *stt = mounts[i].mounted ? "mounted" : "unmounted";
    ga_str(&app, DK_W - 76, y + 4, stt,
           mounts[i].mounted ? 0x0030A030 : 0x00A03030);

    /* capacity bar from statfs (32-bit safe: no 64-bit divide —
     * libpcc.a __udivdi3 is a broken self-recursion) */
    if (st && st->total_blocks) {
        int used = (int)((st->total_blocks - st->free_blocks)
                * 1000u / st->total_blocks);
        ga_progress(&app, DK_LIST_X + 4, y + 26, 240, 10, used);
        char b[40];
        int o = 0;
        const char *p;
        p = "used "; while (*p) b[o++] = *p++;
        char t[12];
        ga_itoa((int)((st->total_blocks - st->free_blocks)
                      * st->block_size / 1024), t);
        for (int k = 0; t[k]; k++) b[o++] = t[k];
        p = "K / "; while (*p) b[o++] = *p++;
        ga_itoa((int)(st->total_blocks * st->block_size / 1024), t);
        for (int k = 0; t[k]; k++) b[o++] = t[k];
        b[o++] = 'K';
        b[o] = 0;
        ga_str(&app, DK_LIST_X + 250, y + 24, b, 0x00606060);
    }
}

static void render(void)
{
    ga_rect(&app, 0, 0, DK_W, DK_H, 0x00E8E8EC);
    ga_chrome(&app, "Disk Manager");
    ga_str(&app, DK_LIST_X, GA_TITLE_H + 4,
           "Filesystems (YAFS root + mounts)", 0x00606060);

    /* capacity header line */
    {
        struct statfs st;
        if (musr_sc_statfs(&st) == 0 && st.total_blocks) {
            char b[44];
            int o = 0;
            const char *p = "/ : ";
            while (*p) b[o++] = *p++;
            char t[12];
            ga_itoa((int)(st.total_blocks * st.block_size / 1024), t);
            for (int k = 0; t[k]; k++) b[o++] = t[k];
            b[o++] = 'K';
            b[o] = 0;
            ga_str(&app, DK_W - 140, GA_TITLE_H + 4, b, 0x00606060);
        }
    }

    for (int i = 0; i < n_mounts && i < DK_ROWS; i++) {
        struct statfs st;
        /* statfs reports the root fs; per-mount stats are the
         * same device in this build — show root numbers for the
         * yafs row, zeros otherwise */
        int have = 0;
        if (mounts[i].mounted) {
            have = (musr_sc_statfs(&st) == 0);
        }
        render_row(i, DK_LIST_Y + i * DK_ROW_H, have ? &st : NULL);
    }

    ga_button(&app, DK_LIST_X, DK_BTN_Y, 64, 18, "Mount", 0);
    ga_button(&app, DK_LIST_X + 72, DK_BTN_Y, 80, 18, "Unmount", 0);
    ga_button(&app, DK_LIST_X + 160, DK_BTN_Y, 72, 18, "Refresh", 0);
}

static void click(int lx, int ly)
{
    if (ga_in(lx, ly, DK_LIST_X, DK_BTN_Y, 64, 18)) {
        do_mount();
        return;
    }
    if (ga_in(lx, ly, DK_LIST_X + 72, DK_BTN_Y, 80, 18)) {
        do_umount();
        return;
    }
    if (ga_in(lx, ly, DK_LIST_X + 160, DK_BTN_Y, 72, 18)) {
        poll();
        return;
    }
    if (lx >= DK_LIST_X && ly >= DK_LIST_Y) {
        int row = (ly - DK_LIST_Y) / DK_ROW_H;
        if (row < n_mounts && row < DK_ROWS) {
            sel = row;
            ser_puts("[DISK] SEL ");
            ser_puts(mounts[row].target);
            ser_puts("\n");
        }
    }
}

void _start(void)
{
    ser_puts("[DISK] starting\n");
    app.buf = dk_buf;
    if (ga_init(&app) != 0) {
        ser_puts("[DISK] copland not ready\n");
        m4k_exit(1);
    }
    poll();
    ser_puts("[DISK] surface ready\n");

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
                ser_puts("[DISK] exit\n");
                m4k_exit(0);
            }
            if (ch == 'r') { poll(); dirty = 1; }
            if (ch == 'm') { do_mount(); dirty = 1; }
            if (ch == 'u') { do_umount(); dirty = 1; }
        }
        if (dirty) {
            render();
            ga_flip(&app);
        }
        m4k_sleep(120);
    }
    m4k_exit(0);
}
