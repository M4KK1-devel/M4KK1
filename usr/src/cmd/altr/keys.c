/*
 * ==== ALTR2 keys.c — key routing, find widget, command palette ====
 * M4KK1 4P1 - usr/src/cmd/altr/keys.c
 * Description: VSCode-style shortcuts over a modal core:
 *   Ctrl+S save, Ctrl+O open (palette), Ctrl+F find, Ctrl+B sidebar,
 *   Ctrl+M minimap, Ctrl+Tab next tab, Ctrl+W close tab, Ctrl+Q quit,
 *   Ctrl+Shift+P / F1 command palette.
 *   Insert-mode editing is always-on when focus is the editor and
 *   mode==MODE_INSERT (i toggles, Esc back to NORMAL).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

static void set_status(const char *msg, uint32_t col)
{
    musr_strncpy(S.status, msg, sizeof(S.status) - 1);
    S.status_col = col;
}

/* ── find widget actions ── */
void al_do_find(int dir)
{
    struct al_doc *d = al_cur();
    if (!d || !S.find_pat[0]) return;
    int ml, mc;
    int hit = (dir > 0)
        ? al_find_next(S.find_pat, d->cur_line, d->cur_col + 1, &ml, &mc)
          || al_find_next(S.find_pat, 0, 0, &ml, &mc)
        : al_find_prev(S.find_pat, d->cur_line, d->cur_col - 1, &ml, &mc)
          || al_find_prev(S.find_pat, d->nlines - 1,
                          al_line_len(d, d->nlines - 1), &ml, &mc);
    if (hit) {
        d->cur_line = ml;
        d->cur_col = mc;
        al_scroll(d);
    } else {
        set_status("no matches", C_MSG_ERR);
    }
    S.nmatches = al_count_matches(S.find_pat);
    S.match_line = hit ? ml : -1;
    S.match_col = hit ? mc : 0;
}

/* ── command palette ── */
void al_exec_palette(void)
{
    const char *c = S.palette_buf;
    int is = (c[0] == 's' && c[1] == 'a');
    int io = (c[0] == 'o' && c[1] == 'p');
    if (is || io) {
        /* "save <path>" / "open <path>" */
        const char *arg = c;
        while (*arg && *arg != ' ') arg++;
        arg = *arg == ' ' ? arg + 1 : arg;
        if (is && *arg) {
            struct al_doc *d = al_cur();
            if (d && al_save(d, arg) == 0) {
                musr_strncpy(d->path, arg, AL_MAX_PATH - 1);
                set_status("saved", C_MSG_OK);
            } else {
                set_status("save failed", C_MSG_ERR);
            }
        } else if (io && *arg) {
            altr_load(arg);
            set_status("opened", C_MSG_OK);
        } else {
            set_status("usage: save|open <path>", C_MSG_ERR);
        }
    } else if (c[0] == 'q') {
        altr_quit();
    } else if (c[0] == 'w') {
        altr_save_cur();
    } else if (c[0] == 'f') {
        S.find_on = 1;
        S.find_len = 0;
        S.find_pat[0] = 0;
        S.focus = FOC_FIND;
    } else if (c[0] == 'b') {
        S.sidebar_open = !S.sidebar_open;
    } else if (c[0] == 'm' && c[1] != 'a') {
        S.minimap_on = !S.minimap_on;
    } else if (c[0] == 't' && c[1] == 'a') {
        /* tabn: switch to doc n */
        int n = (c[3] >= '1' && c[3] <= '6') ? c[3] - '1' : -1;
        if (n >= 0 && S.docs[n].in_use)
            S.cur_doc = n;
    }
    S.palette_on = 0;
    S.palette_len = 0;
    S.palette_buf[0] = 0;
}

/* ── key routing ── */

static void key_palette(unsigned char ch)
{
    if (ch == '\r' || ch == '\n') {
        al_exec_palette();
        S.focus = FOC_EDITOR;
    } else if (ch == 0x1B) {
        S.palette_on = 0;
        S.palette_len = 0;
        S.palette_buf[0] = 0;
        S.focus = FOC_EDITOR;
    } else if ((ch == '\b' || ch == 0x7F) && S.palette_len > 0) {
        S.palette_buf[--S.palette_len] = 0;
    } else if (ch == '\t') {
        al_tab_complete_path(S.palette_buf, sizeof(S.palette_buf));
        S.palette_len = al_strlen(S.palette_buf);
    } else if (ch >= 0x20 && ch < 0x7F &&
               S.palette_len < (int)sizeof(S.palette_buf) - 1) {
        S.palette_buf[S.palette_len++] = ch;
        S.palette_buf[S.palette_len] = 0;
    }
}

static void key_find(unsigned char ch)
{
    if (ch == 0x1B) {
        S.find_on = 0;
        S.focus = FOC_EDITOR;
        return;
    }
    if (ch == '\r' || ch == '\n') {
        al_do_find(1);
        return;
    }
    if (ch == '\t') {                 /* Tab toggles replace field */
        S.find_rep_on = !S.find_rep_on;
        return;
    }
    if (ch == '\b' || ch == 0x7F) {
        if (S.find_len > 0) {
            S.find_pat[--S.find_len] = 0;
            S.nmatches = al_count_matches(S.find_pat);
        }
        return;
    }
    if (ch >= 0x20 && ch < 0x7F &&
        S.find_len < (int)sizeof(S.find_pat) - 1) {
        S.find_pat[S.find_len++] = ch;
        S.find_pat[S.find_len] = 0;
        S.nmatches = al_count_matches(S.find_pat);
        al_do_find(1);                /* incremental find */
        S.focus = FOC_FIND;
    }
}

static void key_tree(unsigned char ch)
{
    switch (ch) {
    case 'j':
        tree_sel = (tree_sel < tree_count - 1) ? tree_sel + 1 : tree_sel;
        break;
    case 'k':
        tree_sel = tree_sel > 0 ? tree_sel - 1 : tree_sel;
        break;
    case '\r':
    case '\n':
        tree_open_sel();
        break;
    case 'h':
    case 'l':
    case 0x1B:
        S.focus = FOC_EDITOR;
        break;
    default:
        break;
    }
}

static void key_editor(unsigned char ch)
{
    struct al_doc *d = al_cur();
    if (!d) return;

    if (S.mode == MODE_INSERT) {
        switch (ch) {
        case 0x1B:
            S.mode = MODE_NORMAL;
            break;
        case '\r':
        case '\n':
            al_split_line(d);
            break;
        case '\b':
        case 0x7F:
            al_backspace(d);
            break;
        case '\t': {                  /* 4-space indent */
            for (int k = 0; k < 4; k++)
                al_insert_ch(d, ' ');
            break;
        }
        default:
            al_insert_ch(d, ch);
            break;
        }
        al_scroll(d);
        return;
    }

    /* NORMAL mode */
    switch (ch) {
    case ':':                        /* command palette (vim-style
                                      * alias for Ctrl+P — HMP/probe
                                      * keyboards cannot reliably
                                      * deliver Ctrl chords) */
        S.palette_on = 1;
        S.palette_len = 0;
        S.palette_buf[0] = 0;
        S.focus = FOC_PALETTE;
        return;
    case 'h':
        d->cur_col = d->cur_col > 0 ? d->cur_col - 1 : 0;
        break;
    case 'l':
        d->cur_col = (d->cur_col < al_line_len(d, d->cur_line))
                     ? d->cur_col + 1 : d->cur_col;
        break;
    case 'j':
        if (d->cur_line < d->nlines - 1) d->cur_line++;
        d->cur_col = (d->cur_col > al_line_len(d, d->cur_line))
                     ? al_line_len(d, d->cur_line) : d->cur_col;
        break;
    case 'k':
        if (d->cur_line > 0) d->cur_line--;
        d->cur_col = (d->cur_col > al_line_len(d, d->cur_line))
                     ? al_line_len(d, d->cur_line) : d->cur_col;
        break;
    case 'g':
        d->cur_line = 0;
        d->cur_col = 0;
        break;
    case 'G':
        d->cur_line = d->nlines - 1;
        d->cur_col = 0;
        break;
    case '0':
        d->cur_col = 0;
        break;
    case '$': {
        int l = al_line_len(d, d->cur_line);
        d->cur_col = l > 0 ? l - 1 : 0;
        break;
    }
    case 'i':
        S.mode = MODE_INSERT;
        break;
    case 'a':
        if (d->cur_col < al_line_len(d, d->cur_line))
            d->cur_col++;
        S.mode = MODE_INSERT;
        break;
    case 'o':
    case 'O': {
        if (d->nlines >= AL_MAX_LINES) break;
        int at = (ch == 'o') ? d->cur_line + 1 : d->cur_line;
        for (int i = d->nlines; i > at; i--)
            musr_strncpy(d->lines[i], d->lines[i - 1],
                         AL_MAX_LINELEN - 1);
        d->lines[at][0] = 0;
        d->nlines++;
        d->cur_line = at;
        d->cur_col = 0;
        d->dirty = 1;
        S.mode = MODE_INSERT;
        break;
    }
    case 'x': {
        char *line = d->lines[d->cur_line];
        int len = al_strlen(line);
        if (d->cur_col < len) {
            for (int i = d->cur_col; i < len; i++)
                line[i] = line[i + 1];
            d->dirty = 1;
        }
        break;
    }
    case 'd':                         /* dd = delete line */
        al_del_line(d, d->cur_line);
        break;
    case 'n':
        al_do_find(1);
        break;
    case 'N':
        al_do_find(-1);
        break;
    case 'f':                         /* focus explorer */
        S.focus = FOC_TREE;
        break;
    default:
        break;
    }
    al_scroll(d);
}

/* raw control codes the sprach keyboard layer forwards:
 *  0x11 Ctrl+Q, 0x13 Ctrl+S, 0x0F Ctrl+O, 0x06 Ctrl+F,
 *  0x02 Ctrl+B, 0x0D Ctrl+M, 0x17 Ctrl+W, 0x09 Ctrl+Tab,
 *  0x10 Ctrl+P (palette, with Shift also 0x10)  */
static void key_ctrl(unsigned char ch)
{
    struct al_doc *d = al_cur();
    switch (ch) {
    case 0x11:                        /* Ctrl+Q quit */
        altr_quit();
        break;
    case 0x13:                        /* Ctrl+S save */
        altr_save_cur();
        break;
    case 0x0F:                        /* Ctrl+O open palette */
        S.palette_on = 1;
        S.palette_len = 0;
        S.palette_buf[0] = 'o';
        S.palette_buf[1] = 'p';
        S.palette_buf[2] = 'e';
        S.palette_buf[3] = 'n';
        S.palette_buf[4] = ' ';
        S.palette_buf[5] = 0;
        S.palette_len = 5;
        S.focus = FOC_PALETTE;
        break;
    case 0x06:                        /* Ctrl+F find */
        S.find_on = 1;
        S.focus = FOC_FIND;
        if (S.find_len == 0) { S.find_pat[0] = 0; }
        break;
    case 0x02:                        /* Ctrl+B sidebar */
        S.sidebar_open = !S.sidebar_open;
        break;
    case 0x0D:                        /* Ctrl+M minimap */
        S.minimap_on = !S.minimap_on;
        break;
    case 0x17:                        /* Ctrl+W close tab */
        al_doc_close(S.cur_doc);
        d = al_cur();
        break;
    case 0x09:                        /* Ctrl+Tab next tab */
        for (int k = 1; k <= AL_MAX_DOCS; k++) {
            int i = (S.cur_doc + k) % AL_MAX_DOCS;
            if (S.docs[i].in_use) { S.cur_doc = i; break; }
        }
        break;
    case 0x10:                        /* Ctrl+P palette */
        S.palette_on = 1;
        S.palette_len = 0;
        S.palette_buf[0] = 0;
        S.focus = FOC_PALETTE;
        break;
    default:
        break;
    }
    (void)d;
}

void al_key(unsigned char ch)
{
    /* Ctrl codes first (work in any focus) */
    if (ch == 0x11 || ch == 0x13 || ch == 0x0F || ch == 0x06 ||
        ch == 0x02 || ch == 0x0D || ch == 0x17 || ch == 0x09 ||
        ch == 0x10)
        { key_ctrl(ch); return; }

    if (S.palette_on || S.focus == FOC_PALETTE)
        { key_palette(ch); return; }
    if (S.find_on && S.focus == FOC_FIND)
        { key_find(ch); return; }
    if (S.focus == FOC_TREE)
        { key_tree(ch); return; }
    key_editor(ch);
}
