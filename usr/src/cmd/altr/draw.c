/*
 * ==== ALTR2 draw.c — pixel primitives + VSCode layout ====
 * M4KK1 4P1 - usr/src/cmd/altr/draw.c
 * Description: 5x7 font, clipped rect/hline/vline, and the full
 *              editor chrome painter (activity bar, sidebar, tabs,
 *              gutter, find widget, status bar, minimap).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

/* Globals required by m4sh.h ABI (defined once for all altr objects) */
int out_fd = 1;
char cwd[256] = "/";

/* editor-wide state (owned here, used everywhere) */
struct al_state S;

/* ── pixel buffer ── */
uint32_t al_buf[AL2_W * AL2_H];

/* ══════════════ tiny 5x7 font (same subset as altr v1) ══════════════ */
static const uint8_t al_font5x7[96][5] = {
    {0,0,0,0,0},{0,0x7F,0,0,0},{0,0x7F,0,0,0},{0x21,0x7F,0x21,0x7F,0x21},
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
    {0,0x44,0x7D,0x40,0},{0x20,0x40,0x44,0x3D,0x08},{0x7F,0x10,0x28,0x44,0},
    {0,0x41,0x7F,0x40,0},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},{0xFC,0x24,0x24,0x24,0x18},{0x18,0x24,0x24,0x18,0xFC},
    {0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3E},{0x44,0x64,0x54,0x4C,0x44},
    {0x08,0x08,0x2A,0x1C,0x08},{0,0,0,0,0},{0x00,0x00,0x00,0x00,0x00},
};

/* ══════════════ primitives ══════════════ */

void al_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > AL2_W) w = AL2_W - x;
    if (y + h > AL2_H) h = AL2_H - y;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            al_buf[yy * AL2_W + xx] = c;
}

void al_char(int x, int y, char ch, uint32_t c)
{
    if (ch < 32 || ch > 126 || x < 0 || y < 0 ||
        x + 5 > AL2_W || y + 7 > AL2_H)
        return;
    const uint8_t *g = al_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row))
                al_buf[(y + row) * AL2_W + x + col] = c;
    }
}

int al_strlen(const char *s)
{
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

void al_str(int x, int y, const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        al_char(x + i * 6, y, s[i], c);
}

void al_str_clip(int x, int y, const char *s, int maxch, uint32_t c)
{
    for (int i = 0; s && s[i] && i < maxch; i++)
        al_char(x + i * 6, y, s[i], c);
}

void al_hline(int y, int x0, int x1, uint32_t c)
{
    al_rect(x0 < 0 ? 0 : x0, y, (x1 - x0) + 1, 1, c);
}

void al_vline(int x, int y0, int y1, uint32_t c)
{
    al_rect(x, y0 < 0 ? 0 : y0, 1, (y1 - y0) + 1, c);
}

void al_itoa(int v, char *out)
{
    char tmp[12];
    int neg = v < 0;
    unsigned uv = neg ? -v : v;
    int i = 0;
    do { tmp[i++] = '0' + uv % 10; uv /= 10; } while (uv);
    int o = 0;
    if (neg) out[o++] = '-';
    while (i > 0) out[o++] = tmp[--i];
    out[o] = 0;
}

/* ══════════════ VSCode chrome ══════════════ */

/* file-name tail of a path (after the last '/') */
static const char *path_tail(const char *p)
{
    const char *t = p;
    for (const char *q = p; *q; q++)
        t = (*q == '/') ? q + 1 : t;
    return t;
}

/* right-align helper: x for drawing s at right edge x1 */
static int right_x(const char *s, int x1)
{
    return x1 - al_strlen(s) * 6;
}

void draw_frame(void)
{
    struct al_doc *d = al_cur();

    /* base */
    al_rect(0, 0, AL2_W, AL2_H, C_BG);

    /* ── title bar ── */
    al_rect(0, 0, AL2_W, AL2_TITLE_H, C_TITLE_BG);
    {
        char t[96];
        int o = 0;
        const char *p = (d && d->path[0]) ? d->path : "Untitled";
        for (const char *s = "altr — "; *s && o < 90; s++) t[o++] = *s;
        for (int i = 0; p[i] && o < 92; i++) t[o++] = p[i];
        t[o] = 0;
        al_str(6, 6, t, C_FG);
    }

    /* ── activity bar (left icon strip) ── */
    al_rect(0, AL2_TITLE_H, AL2_ACTBAR_W,
            AL2_H - AL2_TITLE_H - AL2_STATUS_H, C_ACTBAR_BG);
    /* explorer icon: a stylized "document" glyph */
    al_str(8, AL2_TITLE_H + 10, "E", S.sidebar_open ? 0x00FFFFFF : 0x00909090);
    al_rect(0, AL2_TITLE_H + 24, AL2_ACTBAR_W, 2,
            S.sidebar_open ? C_ACCENT : C_ACTBAR_BG);
    /* search icon (decorative; find via Ctrl+F) */
    al_str(8, AL2_TITLE_H + 34, "S", 0x00909090);

    /* ── sidebar (explorer) ── */
    int sb_w = S.sidebar_open ? AL2_SIDEBAR_W : 0;
    if (S.sidebar_open) {
        al_rect(AL2_ACTBAR_W, AL2_TITLE_H, AL2_SIDEBAR_W,
                AL2_H - AL2_TITLE_H - AL2_STATUS_H, C_SIDEBAR_BG);
        al_str(AL2_ACTBAR_W + 8, AL2_TITLE_H + 6,
               "EXPLORER", 0x00BBBBBB);
        al_hline(AL2_TITLE_H + 16, AL2_ACTBAR_W,
                 AL2_ACTBAR_W + AL2_SIDEBAR_W - 1, C_BORDER);
        /* tree rows (from files.c) */
        int trows = (AL2_H - AL2_TITLE_H - 26 - AL2_STATUS_H) / 10;
        for (int i = 0; i < tree_count && i < trows; i++) {
            int y = AL2_TITLE_H + 26 + i * 10;
            int sel = (i == tree_sel) && (S.focus == FOC_TREE);
            if (sel)
                al_rect(AL2_ACTBAR_W + 2, y - 1, AL2_SIDEBAR_W - 4, 10,
                        C_SELBG);
            else if (i == tree_sel)
                al_rect(AL2_ACTBAR_W + 2, y - 1, AL2_SIDEBAR_W - 4, 10,
                        0x00373737);
            char nm[DIRENT_NAME_MAX + 4];
            int o = 0;
            nm[o++] = tree_ents[i].is_dir ? '>' : ' ';
            for (int k = 0; tree_ents[i].name[k] && o < (int)sizeof(nm) - 1; k++)
                nm[o++] = tree_ents[i].name[k];
            nm[o] = 0;
            al_str_clip(AL2_ACTBAR_W + 6, y, nm,
                        (AL2_SIDEBAR_W - 12) / 6,
                        tree_ents[i].is_dir ? C_TREE_DIR : C_TREE_FILE);
        }
        /* cwd footer */
        al_str_clip(AL2_ACTBAR_W + 4, AL2_H - AL2_STATUS_H - 12,
                    tree_cwd, (AL2_SIDEBAR_W - 8) / 6, 0x00707070);
    }
    al_vline(AL2_ACTBAR_W + sb_w, AL2_TITLE_H,
             AL2_H - AL2_STATUS_H, C_BORDER);

    /* ── tab strip ── */
    int tabs_x0 = AL2_ACTBAR_W + sb_w;
    al_rect(tabs_x0, AL2_TITLE_H, AL2_W - tabs_x0, AL2_TABS_H, C_TABS_BG);
    {
        int tx = tabs_x0 + 2;
        int tw_max = (AL2_W - tabs_x0 - 8) / AL_MAX_DOCS;
        for (int i = 0; i < AL_MAX_DOCS; i++) {
            if (!S.docs[i].in_use) continue;
            int act = (i == S.cur_doc);
            al_rect(tx, AL2_TITLE_H, tw_max, AL2_TABS_H,
                    act ? C_TAB_ACTIVE : C_TAB_INACTIVE);
            if (act)
                al_rect(tx, AL2_TITLE_H, tw_max, 1, C_ACCENT);
            const char *nm = (S.docs[i].path[0])
                             ? path_tail(S.docs[i].path) : "Untitled";
            al_str_clip(tx + 4, AL2_TITLE_H + 7, nm, (tw_max - 20) / 6,
                        act ? 0x00FFFFFF : 0x00909090);
            if (S.docs[i].dirty)
                al_str(tx + tw_max - 10, AL2_TITLE_H + 7, "*", 0x00E0C060);
            al_vline(tx + tw_max, AL2_TITLE_H,
                     AL2_TITLE_H + AL2_TABS_H - 1, C_BORDER);
            tx += tw_max;
        }
    }

    /* ── gutter + text area background ── */
    /* (text painted in draw_text_area) */

    /* ── find widget ── */
    if (S.find_on) {
        int fy = AL2_H - AL2_STATUS_H - AL2_FIND_H - 2;
        al_rect(AL2_TEXT_X - 8, fy, AL2_TEXT_W + 24, AL2_FIND_H, C_FIND_BG);
        al_rect(AL2_TEXT_X - 8, fy, AL2_TEXT_W + 24, 1, C_BORDER);
        al_str(AL2_TEXT_X - 2, fy + 5, "Find", 0x00BBBBBB);
        al_rect(AL2_TEXT_X + 24, fy + 3, 150, 12, 0x003C3C3C);
        al_str_clip(AL2_TEXT_X + 26, fy + 5, S.find_pat, 24, C_FG);
        if (S.find_rep_on) {
            al_str(AL2_TEXT_X + 180, fy + 5, "Rep", 0x00BBBBBB);
            al_rect(AL2_TEXT_X + 202, fy + 3, 120, 12, 0x003C3C3C);
            al_str_clip(AL2_TEXT_X + 204, fy + 5, S.find_rep, 19, C_FG);
        }
        char cnt[24];
        int o = 0;
        for (const char *s = "  matches: "; *s; s++) cnt[o++] = *s;
        char num[8];
        al_itoa(S.nmatches, num);
        for (int k = 0; num[k] && o < 22; k++) cnt[o++] = num[k];
        cnt[o] = 0;
        al_str(AL2_TEXT_X + (S.find_rep_on ? 330 : 200), fy + 5,
               cnt, 0x00909090);
    }

    /* ── command palette (overlay dropdown) ── */
    if (S.palette_on) {
        int px = AL2_TEXT_X - 8;
        int py = AL2_TITLE_H + AL2_TABS_H + 2;
        int pw = AL2_TEXT_W + 24;
        al_rect(px, py, pw, 118, 0x00252626);
        al_rect(px, py, pw, 1, C_ACCENT);
        al_rect(px + 8, py + 8, pw - 16, 16, 0x003C3C3C);
        al_str(px + 12, py + 12, ">", 0x00BBBBBB);
        al_str_clip(px + 24, py + 12, S.palette_buf, (pw - 40) / 6, C_FG);
        static const char *const cmds[] = {
            "Save file            Ctrl+S",
            "Open file...         Ctrl+O",
            "Find...              Ctrl+F",
            "Toggle sidebar       Ctrl+B",
            "Toggle minimap       Ctrl+M",
            "Next tab             Ctrl+Tab",
            "Close tab            Ctrl+W",
            "Quit                 Ctrl+Q",
        };
        int ncmd = (int)(sizeof(cmds) / sizeof(cmds[0]));
        for (int i = 0; i < ncmd; i++) {
            int hit = S.palette_buf[0] == 0;
            if (!hit) {
                /* simple prefix-insensitive substring filter */
                const char *q = S.palette_buf;
                const char *t = cmds[i];
                int oksub = 1;
                for (int k = 0; q[k]; k++) {
                    int found = 0;
                    for (; *t; t++)
                        if (*t == q[k]) { found = 1; break; }
                    if (!found) { oksub = 0; break; }
                }
                hit = oksub;
            }
            if (!hit) continue;
            al_str(px + 12, py + 30 + i * 10, cmds[i],
                   (i == S.palette_sel) ? 0x00FFFFFF : 0x00A0A0A0);
        }
    }

    /* ── status bar ── */
    {
        int sy = AL2_H - AL2_STATUS_H;
        al_rect(0, sy, AL2_W, AL2_STATUS_H, C_STATUS_BG);
        const char *mode = (S.mode == MODE_INSERT) ? " INSERT "
                           : (S.focus == FOC_TREE) ? " FILES "
                           : " NORMAL ";
        al_str(6, sy + 5, mode, 0x00FFFFFF);
        /* branch placeholder (decorative, VSCode-like) */
        al_str(70, sy + 5, "ahead*", 0x00B0D0B0);
        char pos[40];
        int o = 0;
        for (const char *s = "Ln "; *s; s++) pos[o++] = *s;
        char num[12];
        al_itoa(d ? d->cur_line + 1 : 1, num);
        for (int k = 0; num[k] && o < 36; k++) pos[o++] = num[k];
        for (const char *s = ", Col "; *s && o < 36; s++) pos[o++] = *s;
        al_itoa(d ? d->cur_col + 1 : 1, num);
        for (int k = 0; num[k] && o < 36; k++) pos[o++] = num[k];
        pos[o] = 0;
        al_str(right_x(pos, AL2_W - 8), sy + 5, pos, 0x00FFFFFF);
        if (S.status[0])
            al_str_clip(150, sy + 5, S.status, 30,
                        S.status_col ? S.status_col : C_FG);
    }
}

void draw_text_area(void)
{
    struct al_doc *d = al_cur();
    enum al_syn syn = al_syn_of(d ? d->path : "");
    int text_h = AL2_H - AL2_TEXT_Y - AL2_STATUS_H
                 - (S.find_on ? AL2_FIND_H + 4 : 4);

    /* current-line highlight (VSCode subtle) */
    if (d)
        al_rect(AL2_TEXT_X - AL2_GUTTER_W, 
                AL2_TEXT_Y + (d->cur_line - d->top_line) * AL2_CH_H,
                AL2_TEXT_W + AL2_GUTTER_W, AL2_CH_H, 0x00222228);

    for (int r = 0; r < AL2_ROWS; r++) {
        int ln = d ? d->top_line + r : r;
        if (!d || ln >= d->nlines) break;
        int y = AL2_TEXT_Y + r * AL2_CH_H;
        /* gutter number, right-aligned into the gutter column */
        char num[8];
        al_itoa(ln + 1, num);
        al_str(AL2_TEXT_X - 10 - al_strlen(num) * 6, y, num,
               (ln == d->cur_line) ? C_LINENO_CUR : C_LINENO);
        /* syntax-colored line */
        al_draw_syn_line(AL2_TEXT_X, y,
                         d->lines[ln] + d->left_col, AL2_COLS, syn);
        /* find-match highlight */
        if (S.find_on && S.find_pat[0] && ln == S.match_line) {
            int c = S.match_col - d->left_col;
            if (c >= 0 && c < AL2_COLS)
                for (int k = 0; k < al_strlen(S.find_pat) && c + k < AL2_COLS; k++)
                    al_char(AL2_TEXT_X + (c + k) * 6, y,
                            d->lines[ln][d->left_col + c + k],
                            0x00FFFFFF);
        }
        /* cursor */
        if (ln == d->cur_line) {
            int cx = AL2_TEXT_X + (d->cur_col - d->left_col) * 6;
            if (S.mode == MODE_INSERT)
                al_rect(cx, y, 1, 9, C_CURSOR);
            else
                al_rect(cx, y, 5, 9, C_CURSOR);
        }
    }

    /* ── minimap ── */
    if (S.minimap_on && d) {
        int mx = AL2_W - AL2_MINIMAP_W - 4;
        al_vline(mx - 4, AL2_TEXT_Y, AL2_H - AL2_STATUS_H, C_BORDER);
        int mrows = AL2_H - AL2_TEXT_Y - AL2_STATUS_H - 4;
        int step = d->nlines > mrows ? d->nlines / mrows + 1 : 1;
        for (int i = 0; i * step < d->nlines; i++) {
            int y = AL2_TEXT_Y + i;
            if (y >= AL2_TEXT_Y + mrows) break;
            int ll = al_line_len(d, i * step);
            int w = ll * (AL2_MINIMAP_W - 8) / AL_MAX_LINELEN;
            al_rect(mx, y, w > 0 ? w : 1, 1, C_MINIMAP_FG);
        }
        /* viewport indicator */
        int vy = AL2_TEXT_Y +
                 d->top_line * mrows / (d->nlines > 0 ? d->nlines : 1);
        al_rect(mx - 1, vy, AL2_MINIMAP_W + 2, 1, 0x00808080);
    }
}

void draw_all(void)
{
    draw_frame();
    draw_text_area();
}
