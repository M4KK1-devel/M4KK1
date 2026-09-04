/*
 * ==== ALTR2 — VSCode-style code editor ====
 * M4KK1 4P1 - usr/src/cmd/altr/altr.h
 * Description: Shared definitions for the rewritten altr editor
 *              (VSCode-inspired layout, modular files).
 *
 * Layout (VSCode reference):
 *   ┌────────────────────────────────────────┬──┐
 *   │ Title bar  [file] ✕                    │  │
 *   ├──┬─────────────────────────────────────┤S │  S = sidebar (explorer)
 *   │A │ Tabs: [a.c] [b.h] +                 │I │  A = activity bar
 *   │c ├─────────────────────────────────────┤D │  m = minimap
 *   │t │  code area with line numbers    ┃┃m │  ✕ = panel placeholder
 *   │i ├─────────────────────────────────────┤  │
 *   │v │ find widget: [pat][rep] ↹↹ ⓐ ⓕ ... │  │
 *   ├──┴─────────────────────────────────────┴──┤
 *   │ status bar: mode | path | ln,col | creds  │
 *   └───────────────────────────────────────────┘
 *
 * Modules:
 *   altr.h     this file — geometry, colors, shared state
 *   draw.c     pixel/font primitives (5x7) + widget painters
 *   buf.c      multi-document buffer + editing ops + search
 *   files.c    file tree (explorer) + load/save + tab completion
 *   syntax.c   token scanner (.c/.h/.asm/.sh/.md/.txt)
 *   keys.c     modal/insert key routing, find widget, palette
 *   main.c     _start, surface claim, event loop
 *
 * Protocol compatibility (unchanged from altr v1):
 *   - surface width 640 (sprach ga dispatch key)
 *   - key mailbox at 0x00620000, magic "ALT1"
 *   - Copland client model (own pixel buffer)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4KK1_CMD_ALTR_ALTR_H_
#define _M4KK1_CMD_ALTR_ALTR_H_

#include "m4sh.h"
#include "../../lib/libcopland.h"

/* Globals required by m4sh.h ABI */
extern int out_fd;
extern char cwd[256];

/* ── Window geometry ── */
#define AL2_W            640
#define AL2_H            460
#define AL2_TITLE_H      20
#define AL2_TABS_H       22
#define AL2_ACTBAR_W     26
#define AL2_SIDEBAR_W    170
#define AL2_STATUS_H     18
#define AL2_MINIMAP_W    64
#define AL2_FIND_H       24
#define AL2_GUTTER_W     36          /* line-number column */

/* edit-area metrics */
#define AL2_CH_W         6
#define AL2_CH_H         12
#define AL2_TEXT_X       (AL2_ACTBAR_W + AL2_SIDEBAR_W + AL2_GUTTER_W + 8)
#define AL2_TEXT_Y       (AL2_TITLE_H + AL2_TABS_H + 6)
#define AL2_TEXT_W       (AL2_W - AL2_TEXT_X - AL2_MINIMAP_W - 8)
#define AL2_COLS         (AL2_TEXT_W / AL2_CH_W)
#define AL2_ROWS         ((AL2_H - AL2_TEXT_Y - AL2_STATUS_H \
                           - AL2_FIND_H - 10) / AL2_CH_H)

/* ── VSCode Dark+ palette (BGRA) ── */
#define C_BG            0x001E1E1E   /* editor background */
#define C_SIDEBAR_BG    0x00252626   /* explorer */
#define C_ACTBAR_BG     0x00333333   /* activity bar */
#define C_TITLE_BG      0x003C3C3C   /* title bar */
#define C_TABS_BG       0x002D2D2D   /* tab strip */
#define C_TAB_ACTIVE    0x001E1E1E   /* active tab (blends into editor) */
#define C_TAB_INACTIVE  0x002D2D2D
#define C_TAB_BORDER    0x00000000   /* hairline separators */
#define C_STATUS_BG     0x000070A0   /* status bar (VSCode blue) */
#define C_PANEL_BG      0x00181818   /* reserved */
#define C_BORDER        0x003C3C3C   /* 1px separators */
#define C_SELBG         0x00264F78   /* selection / list selection */
#define C_CURSOR        0x00AEAFAD   /* block/beam cursor */
#define C_LINENO        0x00858585   /* gutter numbers */
#define C_LINENO_CUR    0x00C6C6C6   /* current line number */
#define C_FG            0x00D4D4D4   /* default text */
#define C_ACCENT        0x00007ACC   /* focus borders, accents */
#define C_FIND_BG       0x00252626   /* find widget */
#define C_TREE_DIR      0x00C0955A   /* folder icon tone */
#define C_TREE_FILE     0x009CDCFE   /* file tone */
#define C_MSG_OK        0x0089D185   /* status message ok */
#define C_MSG_ERR       0x00F48771   /* status message error */
#define C_MINIMAP_FG    0x00505050   /* minimap density blocks */

/* syntax token colors (VSCode Dark+ semantics) */
#define C_SYN_KW        0x00C586C0   /* keywords  (pink)   */
#define C_SYN_TYPE      0x004EC9B0   /* types     (teal)   */
#define C_SYN_STR       0x00CE9178   /* strings   (orange) */
#define C_SYN_COM       0x006A9955   /* comments  (green)  */
#define C_SYN_NUM       0x00B5CEA8   /* numbers   (pale)   */
#define C_SYN_FN        0x00DCDCAA   /* function names (y) */
#define C_SYN_REG       0x009CDCFE   /* asm regs  (blue)   */
#define C_SYN_INSN      0x00D7BA7D   /* asm mnem  (gold)   */
#define C_SYN_VAR       0x009CDCFE   /* $VARS     (blue)   */
#define C_SYN_MATCH     0x00623315   /* find-match bg      */

/* ── key mailbox (protocol-stable) ── */
#define AL_MB_ADDR      0x00620000u
#define AL_MB_MAGIC     0x414C5431u   /* "ALT1" */
#define AL_MB_SIZE      64

struct al_mailbox {
    uint32_t magic;
    uint32_t write_idx;
    uint32_t read_idx;
    unsigned char buf[AL_MB_SIZE];
};

/* ── documents (tabs) ── */
#define AL_MAX_DOCS     6
#define AL_MAX_LINES    1200
#define AL_MAX_LINELEN  200
#define AL_MAX_PATH     160

struct al_doc {
    int in_use;
    char path[AL_MAX_PATH];
    char (*lines)[AL_MAX_LINELEN];
    int nlines;
    int cur_line, cur_col;
    int top_line, left_col;
    int dirty;
    int sel_anchor;               /* -1 = no selection (line idx) */
};

/* ── editor-wide state (main.c owns) ── */
enum al_focus { FOC_EDITOR, FOC_TREE, FOC_FIND, FOC_PALETTE, FOC_TABS };
enum al_mode { MODE_NORMAL, MODE_INSERT };

struct al_state {
    struct al_doc docs[AL_MAX_DOCS];
    int cur_doc;                  /* index into docs[] */
    int sidebar_open;             /* explorer toggle (Ctrl+B) */
    int minimap_on;               /* Ctrl+M */
    int palette_on;               /* Ctrl+Shift+P command palette */
    char palette_buf[80];
    int palette_len;
    int palette_sel;
    int find_on;                  /* Ctrl+F widget */
    char find_pat[64];
    int find_len;
    char find_rep[64];
    int find_rep_on;              /* toggle replace field */
    int find_case;                /* ignore case by default */
    int match_line, match_col;    /* current find match */
    int nmatches;                 /* count for status */
    enum al_focus focus;
    enum al_mode mode;
    char status[96];
    uint32_t status_col;
};

extern struct al_state S;

/* ── draw.c ── */
extern uint32_t al_buf[AL2_W * AL2_H];
void al_rect(int x, int y, int w, int h, uint32_t c);
void al_char(int x, int y, char ch, uint32_t c);
void al_str(int x, int y, const char *s, uint32_t c);
void al_str_clip(int x, int y, const char *s, int maxch, uint32_t c);
void al_hline(int y, int x0, int x1, uint32_t c);
void al_vline(int x, int y0, int y1, uint32_t c);
int  al_strlen(const char *s);
void al_itoa(int v, char *out);
void draw_frame(void);            /* everything except text area */
void draw_text_area(void);
void draw_all(void);

/* ── buf.c ── */
struct al_doc *al_cur(void);
struct al_doc *al_doc_new(void);
struct al_doc *al_doc_open(const char *path);   /* load or scratch */
void al_doc_close(int idx);
int  al_line_len(struct al_doc *d, int ln);
void al_insert_ch(struct al_doc *d, unsigned char ch);
void al_newline(struct al_doc *d);
void al_split_line(struct al_doc *d);
void al_backspace(struct al_doc *d);
void al_del_line(struct al_doc *d, int ln);
void al_scroll(struct al_doc *d);
int  al_find_next(const char *pat, int from_line, int from_col,
                  int *mline, int *mcol);
int  al_find_prev(const char *pat, int from_line, int from_col,
                  int *mline, int *mcol);
int  al_count_matches(const char *pat);

/* ── files.c ── */
struct al_tree_ent {
    char name[DIRENT_NAME_MAX];
    int is_dir;
};
#define AL_MAX_FILES 64
extern struct al_tree_ent tree_ents[AL_MAX_FILES];
extern int tree_count;
extern char tree_cwd[AL_MAX_PATH];
extern int tree_sel;
void tree_reload(void);
void tree_open_sel(void);
int  al_save(struct al_doc *d, const char *path);
void al_tab_complete_path(char *buf, int bufsz);

/* ── syntax.c ── */
enum al_syn { SYN_NONE, SYN_C, SYN_ASM, SYN_SH, SYN_MD };
enum al_syn al_syn_of(const char *path);
/* draw one syntax-colored line into al_buf; returns px width used */
void al_draw_syn_line(int x, int y, const char *s, int maxch,
                      enum al_syn syn);

/* ── keys.c ── */
void al_key(unsigned char ch);
void al_exec_palette(void);
void al_do_find(int dir);        /* dir: +1 next, -1 prev */

/* main.c: called by keys.c on palette commands */
void altr_load(const char *path);
void altr_save_cur(void);
void altr_quit(void);

#endif /* _M4KK1_CMD_ALTR_ALTR_H_ */
