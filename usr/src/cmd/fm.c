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
#include "../lib/musr_inline.h"
#include "../lib/icons.h"
#include "../lib/icons_data.h"

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
#define FM_MAX_ENTRIES 48
static int fm_count = 0;
static int fm_truncated = 0;  /* dir had ≥48 entries — list clipped */

/* Text preview mode (Enter on a .txt/.c/.h file): renders the first
 * PREVIEW_LINES lines read-only.  Esc/Enter returns to the listing. */
static int fm_preview = 0;
static char fm_preview_title[64];
static char fm_preview_lines[24][76];
static int fm_preview_count = 0;

/* Mouse click forwarding (Sprach → FM).  Same mailbox ring as keys,
 * but control codes 0xF1 (click) / 0xF2 (double click) arrive with
 * two payload bytes (lx, ly — window-local coords) that follow in
 * the ring. */
#define FM_MB_CLICK     0xF1
#define FM_MB_DBLCLICK  0xF2

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
    /* Clip against the framebuffer geometry (FM_W×FM_H).  bw is a
     * *pitch*, never a width bound — callers may legitimately pass
     * a stride that differs from the drawable width someday. */
    int x0 = x > 0 ? x : 0;
    int y0 = y > 0 ? y : 0;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > FM_W)
        x1 = FM_W;
    if (y1 > FM_H)
        y1 = FM_H;
    for (int r = y0; r < y1; r++)
        musr_fill32(buf + (size_t)r * bw + x0,
                    (size_t)(x1 - x0), c);
}

static void fm_char(uint32_t *buf, int bw, int x, int y, char ch,
                    uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7F)
        return;
    const uint8_t *g = fm_font5x7[(int)ch - 0x20];
    /* Clip glyph box once against the framebuffer; pixels are
     * written directly — the old path made 35 fm_rect calls per
     * character, each redoing the full clip walkthrough. */
    int x0 = x > 0 ? x : 0;
    int y0 = y > 0 ? y : 0;
    int x1 = x + 5;
    int y1 = y + 7;
    if (x1 > FM_W)
        x1 = FM_W;
    if (y1 > FM_H)
        y1 = FM_H;
    for (int col = x0 - x; col < 5 && x + col < x1; col++)
        for (int row = y0 - y; row < 7 && y + row < y1; row++)
            if (g[col] & (1u << row))
                buf[(size_t)(y + row) * bw + x + col] = fg;
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

static int fm_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* ── Home directory: mini passwd.db lookup ──
 * passwd.db lines are "name:uid:gid:home:shell:gecos:hash".  We only
 * need uid → home, so a minimal field walker suffices (musr_getpwuid
 * drags the whole m4sh shell along). */
static void fm_default_home(char *out, int cap)
{
    int uid = m4k_getuid();
    out[0] = '/'; out[1] = '\0';
    int fd = musr_sc_open("/export/cfg/passwd.db", O_RDONLY);
    if (fd < 0)
        return;
    static char buf[1024];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0)
        return;
    buf[n] = '\0';
    /* walk lines */
    int i = 0;
    while (i < n) {
        int ls = i;
        while (i < n && buf[i] != '\n')
            i++;
        int le = i;
        if (i < n)
            i++;
        /* fields: name(0) uid(1) gid(2) home(3) ... */
        int f = 0, fs = ls, uid_v = -1, hs = -1, he = -1;
        for (int j = ls; j <= le; j++) {
            if (j == le || buf[j] == ':') {
                if (f == 1 && uid_v < 0) {
                    uid_v = 0;
                    for (int q = fs; q < j; q++)
                        if (buf[q] >= '0' && buf[q] <= '9')
                            uid_v = uid_v * 10 + (buf[q] - '0');
                }
                if (f == 3 && hs < 0) {
                    hs = fs; he = j;
                }
                f++;
                fs = j + 1;
            }
        }
        if (uid_v == uid && hs >= 0 && he > hs) {
            int len = he - hs;
            if (len > cap - 1)
                len = cap - 1;
            for (int q = 0; q < len; q++)
                out[q] = buf[hs + q];
            out[len] = '\0';
            return;
        }
    }
}

/* ── Rendering ── */
static uint32_t fm_buf[FM_W * FM_H];

static void fm_icon16(uint32_t *ic, int x, int y)
{
    for (int yy = 0; yy < 16; yy++)
        for (int xx = 0; xx < 16; xx++) {
            uint32_t px = ic[(yy * 2) * 32 + xx * 2];
            if (!(px >> 24))
                continue;   /* transparent */
            fm_buf[(y + yy) * FM_W + x + xx] = px;
        }
}

/* Entry icon by name: directory variants, extension match, generic. */
static uint32_t *fm_entry_icon(int idx)
{
    if (fm_entries[idx].is_dir) {
        /* Named folder variants (home/config/...) fall back to the
         * plain folder icon from the procedural set. */
        const char *nm = fm_entries[idx].name;
        uint32_t *dir_ic = (uint32_t *)icons_data_find("dir");
        if (!dir_ic)
            return icon_folder;
        if (fm_streq(nm, "home") || fm_streq(nm, "root"))
            dir_ic = (uint32_t *)icons_data_find("dir_home");
        else if (fm_streq(nm, "etc"))
            dir_ic = (uint32_t *)icons_data_find("dir_config");
        else if (fm_streq(nm, "doc") || fm_streq(nm, "docs")
                 || fm_streq(nm, "documents"))
            dir_ic = (uint32_t *)icons_data_find("dir_documents");
        else if (fm_streq(nm, "download") || fm_streq(nm, "downloads"))
            dir_ic = (uint32_t *)icons_data_find("dir_downloads");
        else if (fm_streq(nm, "music") || fm_streq(nm, "audio"))
            dir_ic = (uint32_t *)icons_data_find("dir_music");
        else if (fm_streq(nm, "desktop"))
            dir_ic = (uint32_t *)icons_data_find("dir_desktop");
        if (dir_ic)
            return dir_ic;
        return icon_folder;
    }
    const char *nm = fm_entries[idx].name;
    int len = fm_strlen(nm);
    if (len >= 4 && nm[len-4]=='.' && nm[len-3]=='t' && nm[len-2]=='x'
        && nm[len-1]=='t')
        return icon_text;
    if (len >= 2 && nm[len-2]=='.' && (nm[len-1]=='c' || nm[len-1]=='h'))
        return icon_code;
    return icon_file;
}

/* ── Directory loading ── */
static void fm_load_dir(struct fm_tab *t)
{
    fm_count = 0;
    int fd = musr_sc_open(t->path, O_RDONLY);
    if (fd < 0)
        return;
    /* Single call, capacity-sized: kernel getdents has no cookie —
     * repeated calls would re-walk the B-tree from entry 0 and
     * duplicate every name.  FM_MAX_ENTRIES == 48 slots. */
    struct dirent dbuf[48];
    int n = musr_sc_getdents(fd, dbuf, 48);
    musr_sc_close(fd);
    if (n < 0)
        n = 0;
    if (n > 48)
        n = 48;
    for (int i = 0; i < n && fm_count < 48; i++) {
        if (dbuf[i].name[0] == '.' && !t->hidden_files)
            continue;
        musr_strncpy(fm_entries[fm_count].name, dbuf[i].name,
                     DIRENT_NAME_MAX - 1);
        fm_entries[fm_count].is_dir = (dbuf[i].type == 2);
        fm_count++;
    }
    /* Surface truncation in the status bar instead of failing silent:
     * 48 shown of N means the dir is bigger than the cache. */
    fm_truncated = (n == 48);
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

    /* File rows (or the text preview when active) */
    fm_rect(fm_buf, FM_W, 0, FM_TITLE_H + FM_TAB_H + FM_ADDR_H,
            FM_W, FM_H - FM_TITLE_H - FM_TAB_H - FM_ADDR_H, FM_COL_BODY);
    if (fm_preview) {
        /* read-only text preview */
        fm_str(fm_buf, FM_W, 8, FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 4,
               fm_preview_title, 0x002040A0);
        int y0 = FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 18;
        for (int i = 0; i < fm_preview_count; i++)
            fm_str(fm_buf, FM_W, 8, y0 + i * 12, fm_preview_lines[i],
                   FM_COL_TEXT);
        fm_str(fm_buf, FM_W, 8, FM_H - 30,
               "Esc: back to listing", 0x00707070);
    } else {
    for (int i = 0; i < fm_count && i < FM_ROWS; i++) {
        int y = FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 2 + i * FM_ROW_H;
        if (i == t->sel)
            fm_rect(fm_buf, FM_W, 2, y - 1, FM_W - 4, FM_ROW_H, FM_COL_SEL);
        /* typed icon from the shared icons.c set (16x16 scaled) */
        fm_icon16(fm_entry_icon(i), 6, y);
        fm_str(fm_buf, FM_W, 26, y + 3, fm_entries[i].name,
               fm_entries[i].is_dir ? 0x002040A0 : FM_COL_TEXT);
    }
    /* Empty directory: a hint row instead of a blank body — an empty
     * list otherwise reads as "FM is broken", not "folder is empty"
     * (root's home /export/root boots empty by design). */
    if (fm_count == 0)
        fm_str(fm_buf, FM_W, 8,
               FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 4,
               "(empty folder)", 0x00707070);
    }

    /* Status bar: "items: N  tab M" — N printed with a generic digit
     * loop (the old /10 %10 form broke beyond 99 entries). */
    int sy = FM_H - 14;
    fm_rect(fm_buf, FM_W, 0, sy, FM_W, 14, 0x00D0D0D8);
    char st[48];
    int k = 0;
    const char *a = "items: ";
    while (*a && k < 40)
        st[k++] = *a++;
    char digits[12];
    int nd = 0;
    int cnt = fm_count;
    do {
        digits[nd++] = (char)('0' + cnt % 10);
        cnt /= 10;
    } while (cnt > 0 && nd < 11);
    while (nd > 0 && k < 42)
        st[k++] = digits[--nd];
    if (fm_truncated && k < 43)
        st[k++] = '+';   /* more than 48 entries — list is clipped */
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

/* Build /path/to/file from the current tab + entry name. */
static void fm_full_path(struct fm_tab *t, const char *name, char *out,
                         int cap)
{
    int n = fm_strlen(t->path);
    int i = 0;
    for (; i < n && i < cap - 1; i++)
        out[i] = t->path[i];
    if (i > 0 && out[i - 1] != '/' && i < cap - 1)
        out[i++] = '/';
    for (const char *p = name; *p && i < cap - 1; p++)
        out[i++] = *p;
    out[i] = '\0';
}

/* Load a text file into the preview buffer (first 24 lines x 75 chars). */
static void fm_preview_load(struct fm_tab *t, const char *name)
{
    char path[232];
    fm_full_path(t, name, path, sizeof(path));
    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0) {
        fm_preview_count = 0;
    } else {
        static char pbuf[4096];
        int n = musr_sc_read(fd, pbuf, sizeof(pbuf) - 1);
        musr_sc_close(fd);
        if (n < 0)
            n = 0;
        pbuf[n] = '\0';
        int line = 0, col = 0, i = 0;
        while (i < n && line < 24) {
            char c = pbuf[i];
            if (c == '\n') {
                fm_preview_lines[line][col] = '\0';
                line++; col = 0;
            } else if (c != '\r' && col < 75) {
                fm_preview_lines[line][col++] = (c < 0x20) ? ' ' : c;
            }
            i++;
        }
        if (col > 0 && line < 24)
            fm_preview_lines[line][col] = '\0';
        else if (col > 0)
            line++;   /* last partial line counts */
        fm_preview_count = (col > 0 && line <= 24) ? line :
                           (line > 24 ? 24 : line);
        if (fm_preview_count < 0)
            fm_preview_count = 0;
    }
    /* title: "[preview] name" */
    int k = 0;
    const char *p = "[preview] ";
    while (*p && k < 62) fm_preview_title[k++] = *p++;
    const char *q = name;
    while (*q && k < 62) fm_preview_title[k++] = *q++;
    fm_preview_title[k] = '\0';
    fm_preview = 1;
}

static void fm_open_selected(void)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];
    if (t->sel < 0 || t->sel >= fm_count)
        return;
    if (fm_entries[t->sel].is_dir) {
        fm_path_join(t, fm_entries[t->sel].name);
        fm_load_dir(t);
        fm_preview = 0;
        return;
    }
    /* Regular file: .c/.h/.txt → spawn the altr editor, .sh → the
     * shell; anything else falls back to the built-in preview.
     * (m4k_spawn has no argv, so the app starts at its default
     * location — altr opens /export/home/$USER from its own tree.) */
    {
        const char *nm = fm_entries[t->sel].name;
        int len = fm_strlen(nm);
        int is_c  = len >= 2 && nm[len-2] == '.' && nm[len-1] == 'c';
        int is_h  = len >= 2 && nm[len-2] == '.' && nm[len-1] == 'h';
        int is_tx = len >= 4 && nm[len-4] == '.' && nm[len-3] == 't'
                    && nm[len-2] == 'x' && nm[len-1] == 't';
        int is_sh = len >= 3 && nm[len-3] == '.' && nm[len-2] == 's'
                    && nm[len-1] == 'h';
        if (is_c || is_h || is_tx || is_sh) {
            const char *app = is_sh ? "/bin/m4shg" : "/bin/altr";
            int pid = musr_sc_fork();
            if (pid == 0) {
                m4k_spawn(app, 0);
                m4k_exit(1);   /* spawn failed */
            }
            return;
        }
    }
    fm_preview_load(t, fm_entries[t->sel].name);
}

/* Mouse click at window-local (lx, ly).  Single click selects the row
 * (or the tab under the strip); double click opens it. */
static void fm_click(int lx, int ly, int dbl)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];

    if (fm_preview) {
        if (dbl || ly > FM_TITLE_H)
            fm_preview = 0;   /* any click leaves the preview */
        return;
    }

    /* Tab strip click: switch to the tab under the cursor */
    if (ly >= FM_TITLE_H && ly < FM_TITLE_H + FM_TAB_H) {
        int idx = lx / 64;
        if (idx >= 0 && idx < FM_MAX_TABS && fm_tabs[idx].in_use) {
            fm_cur_tab = idx;
            fm_load_dir(&fm_tabs[idx]);
        }
        return;
    }

    /* File row click: select / (double) open */
    int list_y = ly - (FM_TITLE_H + FM_TAB_H + FM_ADDR_H + 2);
    if (lx >= 0 && lx < FM_W && list_y >= 0) {
        int row = list_y / FM_ROW_H;
        if (row >= 0 && row < fm_count && row < FM_ROWS) {
            t->sel = row;
            if (dbl)
                fm_open_selected();
        }
    }
}

static void fm_key(unsigned char ch)
{
    struct fm_tab *t = &fm_tabs[fm_cur_tab];

    /* Preview mode: any of Esc/Enter/q returns to the listing */
    if (fm_preview) {
        if (ch == 0x1B || ch == '\r' || ch == '\n' || ch == 'q')
            fm_preview = 0;
        return;
    }

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
    icons_init();
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

    /* Initial tab: the user's home directory from passwd.db
     * (root → /export/root, testuser → /home/testuser, ...).  Falls
     * back to "/" when the DB is unreadable. */
    {
        char home[200];
        fm_default_home(home, sizeof(home));
        musr_strncpy(fm_tabs[0].path, home, sizeof(fm_tabs[0].path) - 1);
    }
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

    /* Main loop: poll mailbox keys; render ONLY on input (full-frame
     * repaint at 50 FPS wasted a composite every tick for a static
     * UI — see skill perf rule #1).  Damage is reported as the FM
     * window's own rect so Copland re-composites incrementally
     * instead of dirty-ing the whole screen. */
    int need_render = 1;   /* first frame */
    for (;;) {
        if (!(shm->surfaces[my_slot].flags & COPLAND_SURF_VISIBLE))
            m4k_exit(0);   /* WM hid/closed us */

        while (fm_mb->read_idx != fm_mb->write_idx) {
            unsigned char ch = fm_mb->buf[fm_mb->read_idx];
            fm_mb->read_idx = (fm_mb->read_idx + 1) % 64;
            if (ch == FM_MB_CLICK || ch == FM_MB_DBLCLICK) {
                /* click payload: lx, ly follow in the ring */
                if (fm_mb->read_idx != fm_mb->write_idx) {
                    int lx = fm_mb->buf[fm_mb->read_idx];
                    fm_mb->read_idx = (fm_mb->read_idx + 1) % 64;
                    if (fm_mb->read_idx != fm_mb->write_idx) {
                        int ly = fm_mb->buf[fm_mb->read_idx];
                        fm_mb->read_idx = (fm_mb->read_idx + 1) % 64;
                        fm_click(lx, ly, ch == FM_MB_DBLCLICK);
                    }
                }
                need_render = 1;
                continue;
            }
            fm_key(ch);
            need_render = 1;
        }

        if (need_render) {
            fm_render();
            shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)fm_buf;
            shm->surfaces[my_slot].dmg_x = shm->surfaces[my_slot].x;
            shm->surfaces[my_slot].dmg_y = shm->surfaces[my_slot].y;
            shm->surfaces[my_slot].dmg_w = shm->surfaces[my_slot].w;
            shm->surfaces[my_slot].dmg_h = shm->surfaces[my_slot].h;
            need_render = 0;
        }
        m4k_yield();
    }
}
