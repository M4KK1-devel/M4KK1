/*
 * ==== ALTR2 buf.c — multi-document buffer + editing ====
 * M4KK1 4P1 - usr/src/cmd/altr/buf.c
 * Description: document tabs, insert/edit primitives, scroll
 *              clamping, incremental find (next/prev/count).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

/* line backing store: one shared arena, docs slice it */
static char line_arena[AL_MAX_DOCS][AL_MAX_LINES][AL_MAX_LINELEN];

static void reset_view(struct al_doc *d)
{
    d->cur_line = d->cur_col = d->top_line = d->left_col = 0;
    d->sel_anchor = -1;
}

struct al_doc *al_cur(void)
{
    return S.docs[S.cur_doc].in_use ? &S.docs[S.cur_doc] : NULL;
}

struct al_doc *al_doc_new(void)
{
    for (int i = 0; i < AL_MAX_DOCS; i++) {
        if (S.docs[i].in_use) continue;
        S.docs[i].in_use = 1;
        S.docs[i].path[0] = 0;
        S.docs[i].lines = line_arena[i];
        S.docs[i].lines[0][0] = 0;
        S.docs[i].nlines = 1;
        S.docs[i].dirty = 0;
        reset_view(&S.docs[i]);
        S.cur_doc = i;
        return &S.docs[i];
    }
    return NULL;
}

void al_doc_close(int idx)
{
    if (idx < 0 || idx >= AL_MAX_DOCS || !S.docs[idx].in_use)
        return;
    S.docs[idx].in_use = 0;
    S.cur_doc = (idx == S.cur_doc) ? -1 : S.cur_doc;
    if (S.cur_doc < 0)
        for (int i = 0; i < AL_MAX_DOCS; i++)
            if (S.docs[i].in_use) { S.cur_doc = i; break; }
    if (S.cur_doc < 0)
        al_doc_new();                    /* always keep one doc */
}

int al_line_len(struct al_doc *d, int ln)
{
    return (ln < 0 || ln >= d->nlines) ? 0 : al_strlen(d->lines[ln]);
}

void al_scroll(struct al_doc *d)
{
    d->top_line = (d->cur_line < d->top_line) ? d->cur_line : d->top_line;
    if (d->cur_line >= d->top_line + AL2_ROWS)
        d->top_line = d->cur_line - AL2_ROWS + 1;
    d->left_col = (d->cur_col < d->left_col) ? d->cur_col : d->left_col;
    if (d->cur_col >= d->left_col + AL2_COLS)
        d->left_col = d->cur_col - AL2_COLS + 1;
    d->top_line = d->top_line < 0 ? 0 : d->top_line;
    d->left_col = d->left_col < 0 ? 0 : d->left_col;
}

void al_insert_ch(struct al_doc *d, unsigned char ch)
{
    char *line = d->lines[d->cur_line];
    int len = al_strlen(line);
    if (ch < 0x20 || ch >= 0x7F || len >= AL_MAX_LINELEN - 1)
        return;
    for (int i = len; i > d->cur_col; i--)
        line[i] = line[i - 1];
    line[d->cur_col++] = ch;
    line[len + 1] = 0;
    d->dirty = 1;
}

void al_split_line(struct al_doc *d)
{
    if (d->nlines >= AL_MAX_LINES) return;
    for (int i = d->nlines; i > d->cur_line + 1; i--)
        musr_strncpy(d->lines[i], d->lines[i - 1], AL_MAX_LINELEN - 1);
    d->nlines++;
    char *line = d->lines[d->cur_line];
    char *rest = d->lines[d->cur_line + 1];
    int len = al_strlen(line);
    for (int i = 0; i <= len - d->cur_col; i++)
        rest[i] = line[d->cur_col + i];
    line[d->cur_col] = 0;
    d->cur_line++;
    d->cur_col = 0;
    d->dirty = 1;
}

void al_newline(struct al_doc *d)
{
    al_split_line(d);
}

void al_backspace(struct al_doc *d)
{
    char *line = d->lines[d->cur_line];
    int len = al_strlen(line);

    if (d->cur_col > 0) {
        for (int i = d->cur_col - 1; i <= len; i++)
            line[i] = line[i + 1];
        d->cur_col--;
        d->dirty = 1;
        return;
    }
    if (d->cur_line == 0) return;
    /* join with previous line */
    int plen = al_line_len(d, d->cur_line - 1);
    if (plen + len >= AL_MAX_LINELEN - 1) return;
    char *prev = d->lines[d->cur_line - 1];
    for (int i = 0; i <= len; i++)
        prev[plen + i] = line[i];
    al_del_line(d, d->cur_line);
    d->cur_line--;
    d->cur_col = plen;
    d->dirty = 1;
}

void al_del_line(struct al_doc *d, int ln)
{
    if (ln < 0 || ln >= d->nlines) return;
    for (int i = ln; i < d->nlines - 1; i++)
        musr_strncpy(d->lines[i], d->lines[i + 1], AL_MAX_LINELEN - 1);
    d->nlines = d->nlines > 1 ? d->nlines - 1 : 1;
    if (d->nlines == 1 && ln == 0)
        d->lines[0][0] = 0;
    if (d->cur_line >= d->nlines)
        d->cur_line = d->nlines - 1;
    d->dirty = 1;
}

/* ── find ── */

static int eq_ci(char a, char b)
{
    char la = (a >= 'A' && a <= 'Z') ? a + 32 : a;
    char lb = (b >= 'A' && b <= 'Z') ? b + 32 : b;
    return la == lb;
}

static int match_at(const char *line, int pos, const char *pat)
{
    for (int i = 0; pat[i]; i++) {
        char lc = line[pos + i];
        if (!lc) return 0;
        if (S.find_case ? (lc != pat[i]) : !eq_ci(lc, pat[i]))
            return 0;
    }
    return 1;
}

int al_find_next(const char *pat, int from_line, int from_col,
                 int *mline, int *mcol)
{
    struct al_doc *d = al_cur();
    if (!d || !pat[0]) return 0;
    for (int ln = from_line; ln < d->nlines; ln++) {
        int start = (ln == from_line) ? from_col : 0;
        for (int c = start; d->lines[ln][c]; c++) {
            if (match_at(d->lines[ln], c, pat)) {
                *mline = ln;
                *mcol = c;
                return 1;
            }
        }
    }
    return 0;
}

int al_find_prev(const char *pat, int from_line, int from_col,
                 int *mline, int *mcol)
{
    struct al_doc *d = al_cur();
    if (!d || !pat[0]) return 0;
    int plen = al_strlen(pat);
    for (int ln = from_line; ln >= 0; ln--) {
        int startc = al_line_len(d, ln) - plen;
        if (ln == from_line && startc >= from_col)
            startc = from_col - 1;
        for (int c = startc; c >= 0; c--) {
            if (match_at(d->lines[ln], c, pat)) {
                *mline = ln;
                *mcol = c;
                return 1;
            }
        }
    }
    return 0;
}

int al_count_matches(const char *pat)
{
    struct al_doc *d = al_cur();
    if (!d || !pat[0]) return 0;
    int n = 0;
    for (int ln = 0; ln < d->nlines; ln++)
        for (int c = 0; d->lines[ln][c]; c++)
            n += match_at(d->lines[ln], c, pat);
    return n;
}

/* ── doc open (load file contents; scratch when unreadable) ── */
struct al_doc *al_doc_open(const char *path)
{
    struct al_doc *d = al_doc_new();
    if (!d) return NULL;
    musr_strncpy(d->path, path, AL_MAX_PATH - 1);

    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0) {                        /* new file: scratch */
        reset_view(d);
        return d;
    }
    static char fbuf[96 * 1024];
    int n = musr_sc_read(fd, fbuf, sizeof(fbuf) - 1);
    musr_sc_close(fd);
    n = n < 0 ? 0 : n;
    fbuf[n] = 0;

    d->nlines = 0;
    int col = 0;
    for (int i = 0; i < n && d->nlines < AL_MAX_LINES; i++) {
        if (fbuf[i] == '\n') {
            d->lines[d->nlines][col] = 0;
            d->nlines++;
            col = 0;
        } else if (fbuf[i] != '\r' && col < AL_MAX_LINELEN - 1) {
            d->lines[d->nlines][col++] = fbuf[i];
        }
    }
    if (col > 0 && d->nlines < AL_MAX_LINES) {
        d->lines[d->nlines][col] = 0;
        d->nlines++;
    }
    d->nlines = d->nlines ? d->nlines : 1;
    reset_view(d);
    d->dirty = 0;
    return d;
}
