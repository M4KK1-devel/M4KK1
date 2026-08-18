/*
 * ==== FILE MANAGER ====
 * M4KK1 4P1 - fm.c
 * Description: Graphical file manager with tab support
 *
 * A standalone Copland client (same model as /bin/terminal): owns a
 * pixel buffer rendered via m4k_gfx_blit, receives keyboard events
 * through the Sprach FM mailbox, and manages up to 16 directory tabs.
 *
 * Tabs: Ctrl+T new tab, Ctrl+W close tab, Ctrl+Tab next tab.
 * Address bar: Ctrl+L focus.  Hidden files: Ctrl+H toggle.
 * Navigation: single click enters a directory.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"
#include "../lib/libcopland.h"

/* Globals required by m4sh.h */
int out_fd = 1;
char cwd[256] = "/";

/* ── Window geometry ── */
#define FM_W          560
#define FM_H          400
#define FM_TITLE_H    18
#define FM_TAB_H      20
#define FM_ADDR_H     22
#define FM_ROW_H      16
#define FM_ROWS       ((FM_H - FM_TITLE_H - FM_TAB_H - FM_ADDR_H - 18) / FM_ROW_H)

/* Colors (BGRA) */
#define FM_COL_TITLE   0x00A05010
#define FM_COL_BODY    0x00F0F0F0
#define FM_COL_TAB     0x00C8C8D0
#define FM_COL_TABSEL  0x00FFFFFF
#define FM_COL_TEXT    0x00202020
#define FM_COL_SEL     0x003060C0

/* ── Tabs ── */
#define FM_MAX_TABS 16
struct fm_tab {
    int in_use;
    char path[200];
    int hidden_files;   /* per-tab Ctrl+H state */
    int sel;            /* selected row in the list */
};
static struct fm_tab fm_tabs[FM_MAX_TABS];
static int fm_cur_tab = 0;

/* Directory listing cache (reloaded on navigation) */
struct fm_entry {
    char name[DIRENT_NAME_MAX];
    int is_dir;
};
static struct fm_entry fm_entries[48];
static int fm_count = 0;

/* Address-bar edit buffer (Ctrl+L activates) */
static int fm_addr_edit = 0;
static char fm_addr_buf[200];

/* Sprach → FM key mailbox (same protocol as the terminal mailbox;
 * Sprach writes keys of the focused FM window here). */
struct fm_mailbox {
    uint32_t magic;
#define FM_MAILBOX_MAGIC 0x464D4B31u   /* "FMK1" */
    uint32_t write_idx;
    uint32_t read_idx;
    unsigned char buf[64];
};
#define FM_MAILBOX_BASE 0x00610000
static volatile struct fm_mailbox *fm_mb =
    (volatile struct fm_mailbox *)FM_MAILBOX_BASE;

/* ── Tiny 5x7 font glyph renderer (subset, matches sprach style) ── */
static const uint8_t fm_font5x7[96][5] = {
    {0,0,0,0,0},{0,0,0x7F,0,0},{0,0x7F,0,0,0},{0x21,0x7F,0x21,0x7F,0x21},
    {0x14,0x7F,0x7F,0x14,0x7F},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0,0x05,0x03,0,0},{0,0x1C,0x22,0x41,0},
    {0x14,0x7F,0x41,0x7F,0x14},{0x08,0x3E,0x41,0x3E,0x08},{0,0x08,0x08,0,0},
    {0x08,0x08,0x3E,0x08,0x08},{0,0xA0,0x60,0,0},{0x08,0x08,0x08,0x08,0x08},
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
    {0x21,0x45,0x4B,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x48,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x41,0x36,0x08,0},
    {0x08,0x14,0x14,0x08,0x3E},{0x08,0x14,0x22,0x22,0x3E},{0x3E,0x08,0x14,0x22,0x3E},
    {0x46,0x29,0x19,0x09,0x07},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x41,0x41,0x41,0x3E},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x3A},
    {0x7F,0x08,0x08,0x08,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},{0x02,0x04,0x08,0x10,0x20},
    {0,0x41,0x41,0x7F,0},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0,0x01,0x02,0x04,0},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},{0x7F,0x08,0x04,0x04,0x78},
    {0,0x44,0x7D,0x40,0},{0x20,0x40,0x44,0x3D,0x08},{0x7F,0x10,0x28,0x44,0x00},
    {0,0x41,0x7F,0x40,0},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},{0xFC,0x24,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},
    {0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},
    {0x08,0x08,0x2A,0x1C,0x08},{0,0,0,0,0},{0x00,0x00,0x00,0x00,0x00},
};

static void fm_rect(uint32_t *buf, int bw, int x, int y, int w, int h,
                    uint32_t c)
{
    for (int r = y; r < y + h; r++)
        for (int cc = x; cc < x + w; cc++)
            if (r >= 0 && r < FM_H && cc >= 0 && cc < bw)
                buf[r * bw + cc] = c;
}

static void fm_char(uint32_t *buf, int bw, int x, int y, char ch,
                    uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7F)
        return;
    const uint8_t *g = fm_font5x7[(int)ch - 0x20];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (g[col] & (1u << row))
                fm_rect(buf, bw, x + col, y + row, 1, 1, fg);
}

static void fm_str(uint32_t *buf, int bw, int x, int y, const char *s,
                   uint32_t fg)
{
    while (*s) {
        fm_char(buf, bw, x, y, *s++, fg);
        x += 6;
    }
}

static int fm_strlen(const char *s)
{
    int n = 0;
    while (s && s[n])
        n++;
    return n;
}

/* ── Directory loading ── */
static void fm_load_dir(struct fm_tab *t)
{
    fm_count = 0;
    int fd = musr_sc_open(t->path, O_RDONLY);
    if (fd < 0)
        return;
    struct dirent dbuf[24];
    int n = musr_sc_getdents(fd, dbuf, 24);
    musr_sc_close(fd);
    for (int i = 0; i < n && fm_count < 48; i++) {
        if (dbuf[i].name[0] == '.' && !t->hidden_files)
            continue;
        musr_strncpy(fm_entries[fm_count].name, dbuf[i].name,
                     DIRENT_NAME_MAX - 1);
        fm_entries[fm_count].is_dir = (dbuf[i].type == 2);
        fm_count++;
    }
    t->sel = 0;
}

static void fm_path_join(struct fm_tab *t, const char *sub)
{
    int n = fm_strlen(t->path);
    if (n > 1 && t->path[n - 1] == '/')
        t->path[n - 1] = '\0';
    /* handle ".." */
    if (sub[0] == '.' && sub[1] == '.' && sub[2] == '\0') {
        int i = fm_strlen(t->path);
        while (i > 0 && t->path[i] != '/')
            i--;
        if (i > 0)
            t->path[i] = '\0';
        else {
            t->path[0] = '/';
            t->path[1] = '\0';
        }
        return;
    }
    if (n + 1 + fm_strlen(sub) >= (int)sizeof(t->path) - 1)
        return;
    if (t->path[1] != '\0' || t->path[0] != '/') {
        t->path[n] = '/';
        n++;
    } else
        t->path[n] = '\0';
    musr_strncpy(t->path + n, sub, sizeof(t->path) - 1 - n);
}

/* ── Rendering ── */
static uint32_t fm_buf[FM_W * FM_H];

static void fm_render(void)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];

    /* Title bar */
    fm_rect(fm_buf, FM_W, 0, 0, FM_W, FM_TITLE_H, FM_COL_TITLE);
    fm_str(fm_buf, FM_W, 6, 5, "Files", 0x00FFFFFF);

    /* Tab strip */
    int tx = 0;
    for (int i = 0; i < FM_MAX_TABS; i++) {
        if (!fm_tabs[i].in_use)
            continue;
        int tw = 64;
        fm_rect(fm_buf, FM_W, tx, FM_TITLE_H, tw, FM_TAB_H,
                (i == fm_cur_tab) ? FM_COL_TABSEL : FM_COL_TAB);
        /* short path tail as the tab label */
        const char *lbl = fm_tabs[i].path;
        for (const char *p = lbl; *p; p++)
            if (*p == '/')
                lbl = p + 1;
        if (!*lbl)
            lbl = "/";
        fm_str(fm_buf, FM_W, tx + 4, FM_TITLE_H + 6, lbl,
               (i == fm_cur_tab) ? FM_COL_TEXT : 0x00505050);
        tx += tw;
        if (tx >= FM_W - 70)
            break;
    }

    /* Address bar */
    fm_rect(fm_buf, FM_W, 0, FM_TITLE_H + FM_TAB_H, FM_W, FM_ADDR_H,
            fm_addr_edit ? 0x00FFFFFF : 0x00E0E0E8);
    fm_rect(fm_buf, FM_W, 0, FM_TITLE_H + FM_TAB_H + FM_ADDR_H - 1, FM_W, 1,
            0x00A0A0A8);
    fm_str(fm_buf, FM_W, 6, FM_TITLE_H + FM_TAB_H + 7,
           fm_addr_edit ? fm_addr_buf : t->path, FM_COL_TEXT);

    /* File rows */
    fm_rect(fm_buf, FM_W, 0, FM_TITLE_H + FM_TAB_H + FM_ADDR_H,
            FM_W, FM_H - FM_TITLE_H - FM_TAB_H - FM_ADDR_H, FM_COL_BODY);
    for (int i = 0; i < fm_count && i < FM_ROWS; i++) {
        int y = FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 2 + i * FM_ROW_H;
        if (i == t->sel)
            fm_rect(fm_buf, FM_W, 2, y - 1, FM_W - 4, FM_ROW_H, FM_COL_SEL);
        /* folder glyph vs file glyph */
        if (fm_entries[i].is_dir) {
            fm_rect(fm_buf, FM_W, 8, y + 2, 12, 9, 0x00E0B040);
            fm_rect(fm_buf, FM_W, 8, y, 5, 3, 0x00E0B040);
        } else {
            fm_rect(fm_buf, FM_W, 9, y, 10, 12, 0x00FFFFFF);
            fm_rect(fm_buf, FM_W, 9, y, 10, 12 - 1, 0x00808080 + 0);
            fm_rect(fm_buf, FM_W, 10, y + 1, 8, 10, 0x00FFFFFF);
        }
        fm_str(fm_buf, FM_W, 26, y + 3, fm_entries[i].name,
               fm_entries[i].is_dir ? 0x002040A0 : FM_COL_TEXT);
    }

    /* Status bar: "N items, M selected" */
    int sy = FM_H - 14;
    fm_rect(fm_buf, FM_W, 0, sy, FM_W, 14, 0x00D0D0D8);
    char st[48];
    int k = 0;
    const char *a = "items: ";
    while (*a && k < 40)
        st[k++] = *a++;
    st[k++] = '0' + (char)(fm_count / 10);
    st[k++] = '0' + (char)(fm_count % 10);
    const char *b = "  tab ";
    while (*b && k < 46)
        st[k++] = *b++;
    st[k++] = '0' + (char)(fm_cur_tab + 1);
    st[k] = '\0';
    fm_str(fm_buf, FM_W, 6, sy + 4, st, 0x00404040);
}

/* ── Input handling ── */
static void fm_new_tab(void)
{
    for (int i = 0; i < FM_MAX_TABS; i++) {
        if (!fm_tabs[i].in_use) {
            fm_tabs[i].in_use = 1;
            fm_tabs[i].hidden_files = 0;
            musr_strncpy(fm_tabs[i].path, fm_tabs[fm_cur_tab].path,
                         sizeof(fm_tabs[i].path) - 1);
            fm_cur_tab = i;
            fm_load_dir(&fm_tabs[i]);
            return;
        }
    }
}

static void fm_close_tab(void)
{
    fm_tabs[fm_cur_tab].in_use = 0;
    for (int i = 0; i < FM_MAX_TABS; i++) {
        if (fm_tabs[i].in_use) {
            fm_cur_tab = i;
            fm_load_dir(&fm_tabs[i]);
            return;
        }
    }
    /* last tab closed: open a fresh one at / */
    fm_tabs[fm_cur_tab].in_use = 1;
    musr_strncpy(fm_tabs[fm_cur_tab].path, "/", 2);
    fm_load_dir(&fm_tabs[fm_cur_tab]);
}

static void fm_next_tab(void)
{
    for (int k = 1; k <= FM_MAX_TABS; k++) {
        int i = (fm_cur_tab + k) % FM_MAX_TABS;
        if (fm_tabs[i].in_use) {
            fm_cur_tab = i;
            fm_load_dir(&fm_tabs[i]);
            return;
        }
    }
}

static void fm_open_selected(void)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];
    if (t->sel < 0 || t->sel >= fm_count)
        return;
    if (fm_entries[t->sel].is_dir) {
        fm_path_join(t, fm_entries[t->sel].name);
        fm_load_dir(t);
    }
    /* regular files: no default handler wired yet — flash the status */
}

static void fm_key(unsigned char ch)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];

    /* Address-bar editing mode */
    if (fm_addr_edit) {
        if (ch == '\r' || ch == '\n') {
            musr_strncpy(t->path, fm_addr_buf, sizeof(t->path) - 1);
            fm_load_dir(t);
            fm_addr_edit = 0;
        } else if (ch == 0x1B) {
            fm_addr_edit = 0;
        } else if ((ch == '\b' || ch == 0x7F) && fm_strlen(fm_addr_buf) > 0) {
            fm_addr_buf[fm_strlen(fm_addr_buf) - 1] = '\0';
        } else if (ch >= 0x20 && ch < 0x7F &&
                   fm_strlen(fm_addr_buf) < (int)sizeof(fm_addr_buf) - 1) {
            fm_addr_buf[fm_strlen(fm_addr_buf)] = ch;
        }
        return;
    }

    /* Ctrl combos (ch arrives as raw control code from Sprach:
     * 0x14=Ctrl+T, 0x17=Ctrl+W, 0x0C=Ctrl+L, 0x08=Ctrl+H, 0x09=Tab) */
    if (ch == 0x14) { fm_new_tab(); return; }
    if (ch == 0x17) { fm_close_tab(); return; }
    if (ch == 0x0C) {
        fm_addr_edit = 1;
        musr_strncpy(fm_addr_buf, t->path, sizeof(fm_addr_buf) - 1);
        return;
    }
    if (ch == 0x08) { t->hidden_files = !t->hidden_files; fm_load_dir(t); return; }
    if (ch == 0x09) { fm_next_tab(); return; }

    if (ch == '\r' || ch == '\n') { fm_open_selected(); return; }
    if (ch == 0x02 || ch == 'k') {   /* up */
        if (t->sel > 0) t->sel--;
        return;
    }
    if (ch == 0x06 || ch == 'j') {   /* down */
        if (t->sel < fm_count - 1) t->sel++;
        return;
    }
}

/* ── Main ── */
void _start(void)
{
    ser_puts("[FM] file manager starting\n");

    struct copland_shm *shm = copland_shm_get();
    if (!shm || shm->magic != COPLAND_SHM_MAGIC) {
        ser_puts("[FM] copland not ready\n");
        m4k_exit(1);
    }

    /* Register the FM key mailbox */
    fm_mb->magic = FM_MAILBOX_MAGIC;
    fm_mb->write_idx = 0;
    fm_mb->read_idx = 0;

    /* Initial tab: root → /export/root, others → / (getpwuid lives in
     * the m4sh binary; the FM ELF links without it). */
    if (m4k_getuid() == 0)
        musr_strncpy(fm_tabs[0].path, "/export/root",
                     sizeof(fm_tabs[0].path) - 1);
    else
        musr_strncpy(fm_tabs[0].path, "/", 2);
    fm_tabs[0].in_use = 1;
    fm_tabs[0].hidden_files = 0;
    fm_load_dir(&fm_tabs[0]);

    /* Create our surface */
    if (copland_cmd_push(shm, COPLAND_CMD_CREATE_SURFACE,
                         100, 60, FM_W, FM_H, (int32_t)FM_COL_TITLE,
                         COPLAND_SURF_VISIBLE) != 0) {
        ser_puts("[FM] cmd ring full\n");
        m4k_exit(1);
    }

    /* Wait for Copland to allocate our slot */
    int guard = 0;
    int my_slot = -1;
    while (guard++ < 200000) {
        m4k_yield();
        int before_slot = -1;
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (shm->surfaces[i].in_use && !shm->surfaces[i].buffer_ptr &&
                shm->surfaces[i].w == FM_W) {
                before_slot = i;
                break;
            }
        if (before_slot >= 0) {
            my_slot = before_slot;
            break;
        }
    }
    if (my_slot < 0) {
        ser_puts("[FM] surface allocation timeout\n");
        m4k_exit(1);
    }
    shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)fm_buf;
    ser_puts("[FM] surface ready (slot=");
    print_u32((uint32_t)my_slot);
    ser_puts(")\n");

    /* Main loop: poll mailbox keys, render, keep dirty */
    for (;;) {
        if (!(shm->surfaces[my_slot].flags & COPLAND_SURF_VISIBLE))
            m4k_exit(0);   /* WM hid/closed us */

        while (fm_mb->read_idx != fm_mb->write_idx) {
            unsigned char ch = fm_mb->buf[fm_mb->read_idx];
            fm_mb->read_idx = (fm_mb->read_idx + 1) % 64;
            fm_key(ch);
        }

        fm_render();
        shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)fm_buf;
        shm->dirty = 1;
        m4k_yield();
    }
}
