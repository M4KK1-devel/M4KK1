/*
 * M4KK1 4P1 - terminal.c
 * Description: Graphical terminal emulator hosting a real m4sh child
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Architecture (true fork + exec + pipes):
 *
 *   Sprach (WM)                    terminal (this)               m4shg
 *   ─────────────                  ───────────────────           ─────────
 *   keystrokes ──> term_mailbox ──> drain mailbox,
 *                                   write byte ──> pipe_in ──> fd 0 read
 *                                   read pipe_out <───────────── fd 1 write
 *                                   paint 80x25 grid
 *                                   surface->dirty = 1
 *
 * The terminal forks; the child creates a pipe, dup2's it onto
 * fd 0/1, then m4k_spawn("/bin/m4shg") — the 4P1 exec (in-place ELF
 * replace) of the pipe-redirected graphical shell, linked at
 * 0x1000000 so it never overlaps the live serial shell at 0x800000.
 *
 * Ownership split with Sprach is unchanged from the mailbox era
 * (see skill m4kk1-graphics-stack): Sprach owns the keyboard and
 * chrome clicks, the terminal owns its surface pixel buffer.
 *
 * 80x25 grid, 8x16 cells (kernel font_8x16 glyph table copied here so
 * the terminal stays self-contained), scrollback via PageUp/PageDown
 * (0x01/0x02 private keycodes from the E0-extension keyboard path),
 * ANSI color/filter for the shell's escape sequences.
 */

#include "../lib/libcopland.h"
#include "../lib/musr_inline.h"
#include "../lib/musr_memmove.h"
#include "m4sh.h"

/* Globals required by the m4sh.h ABI (unused here) */
int out_fd = 1;
char cwd[256] = "/";

/* ── Window geometry (must match Sprach's TERM_WIN_* constants) ── */

#define TERM_W          680
#define TERM_H          456
#define TERM_TITLE_H    18

#define TERM_COLS       80
#define TERM_ROWS       25
#define TERM_CHAR_W     8
#define TERM_CHAR_H     16
#define TERM_ORIGIN_X   ((TERM_W - TERM_COLS * TERM_CHAR_W) / 2)
#define TERM_ORIGIN_Y   (TERM_TITLE_H + 6)

/* Scrollback: rows kept above the visible screen */
#define TERM_SCROLLBACK 500

/* ── Colors (BGRA) ── */

#define TCOL_TITLE      0x00303060
#define TCOL_BODY       0x00101018
#define TCOL_TEXT       0x00D8D8D8
#define TCOL_PROMPT     0x0000E060
#define TCOL_ERR        0x000040E0

/* ── Mac OS 9 window controls (must mirror Sprach's chrome) ── */

#define CTRL_SIZE       10
#define CTRL_Y           4
#define CTRL_CLOSE_X     8
#define CTRL_MIN_X      22
#define CTRL_MAX_X      36

/* ── Pixel buffer (BSS; Copland blits this via surface->buffer_ptr) ──
 * Maximized work area under the 24px bar + 48px dock = 800x528. */

#define TERM_BUF_W      800
#define TERM_BUF_H      546

static uint32_t term_buf[TERM_BUF_W * TERM_BUF_H] __attribute__((aligned(16)));

/* ── 8x16 font (same table as kernel font_8x16.c, 0x20..0x7E) ── */

#define FONT_CHARS 95
#define FONT_W 8
#define FONT_H 16

static const unsigned char font8x16[FONT_CHARS * FONT_H] = {
#include "term_font_8x16.h"
};

/* ── xterm-256 palette ──
 * Values use the same 0xAARRGGBB layout as TCOL_* above
 * (TCOL_PROMPT 0x0000E060 = rgb(0,224,96)).  Indices 0-15 are the
 * standard colors, 16-231 the 6x6x6 cube (steps 0/95/135/175/215/255),
 * 232-255 a grayscale ramp 8..238. */

static const uint32_t term_256_rgb[256] = {
	0x00000000, 0x00800000, 0x00008000, 0x00808000, 0x00000080, 0x00800080,
	0x00008080, 0x00C0C0C0, 0x00808080, 0x00FF0000, 0x0000FF00, 0x00FFFF00,
	0x000000FF, 0x00FF00FF, 0x0000FFFF, 0x00FFFFFF, 0x00000000, 0x0000005F,
	0x00000087, 0x000000AF, 0x000000D7, 0x000000FF, 0x00005F00, 0x00005F5F,
	0x00005F87, 0x00005FAF, 0x00005FD7, 0x00005FFF, 0x00008700, 0x0000875F,
	0x00008787, 0x000087AF, 0x000087D7, 0x000087FF, 0x0000AF00, 0x0000AF5F,
	0x0000AF87, 0x0000AFAF, 0x0000AFD7, 0x0000AFFF, 0x0000D700, 0x0000D75F,
	0x0000D787, 0x0000D7AF, 0x0000D7D7, 0x0000D7FF, 0x0000FF00, 0x0000FF5F,
	0x0000FF87, 0x0000FFAF, 0x0000FFD7, 0x0000FFFF, 0x005F0000, 0x005F005F,
	0x005F0087, 0x005F00AF, 0x005F00D7, 0x005F00FF, 0x005F5F00, 0x005F5F5F,
	0x005F5F87, 0x005F5FAF, 0x005F5FD7, 0x005F5FFF, 0x005F8700, 0x005F875F,
	0x005F8787, 0x005F87AF, 0x005F87D7, 0x005F87FF, 0x005FAF00, 0x005FAF5F,
	0x005FAF87, 0x005FAFAF, 0x005FAFD7, 0x005FAFFF, 0x005FD700, 0x005FD75F,
	0x005FD787, 0x005FD7AF, 0x005FD7D7, 0x005FD7FF, 0x005FFF00, 0x005FFF5F,
	0x005FFF87, 0x005FFFAF, 0x005FFFD7, 0x005FFFFF, 0x00870000, 0x0087005F,
	0x00870087, 0x008700AF, 0x008700D7, 0x008700FF, 0x00875F00, 0x00875F5F,
	0x00875F87, 0x00875FAF, 0x00875FD7, 0x00875FFF, 0x00878700, 0x0087875F,
	0x00878787, 0x008787AF, 0x008787D7, 0x008787FF, 0x0087AF00, 0x0087AF5F,
	0x0087AF87, 0x0087AFAF, 0x0087AFD7, 0x0087AFFF, 0x0087D700, 0x0087D75F,
	0x0087D787, 0x0087D7AF, 0x0087D7D7, 0x0087D7FF, 0x0087FF00, 0x0087FF5F,
	0x0087FF87, 0x0087FFAF, 0x0087FFD7, 0x0087FFFF, 0x00AF0000, 0x00AF005F,
	0x00AF0087, 0x00AF00AF, 0x00AF00D7, 0x00AF00FF, 0x00AF5F00, 0x00AF5F5F,
	0x00AF5F87, 0x00AF5FAF, 0x00AF5FD7, 0x00AF5FFF, 0x00AF8700, 0x00AF875F,
	0x00AF8787, 0x00AF87AF, 0x00AF87D7, 0x00AF87FF, 0x00AFAF00, 0x00AFAF5F,
	0x00AFAF87, 0x00AFAFAF, 0x00AFAFD7, 0x00AFAFFF, 0x00AFD700, 0x00AFD75F,
	0x00AFD787, 0x00AFD7AF, 0x00AFD7D7, 0x00AFD7FF, 0x00AFFF00, 0x00AFFF5F,
	0x00AFFF87, 0x00AFFFAF, 0x00AFFFD7, 0x00AFFFFF, 0x00D70000, 0x00D7005F,
	0x00D70087, 0x00D700AF, 0x00D700D7, 0x00D700FF, 0x00D75F00, 0x00D75F5F,
	0x00D75F87, 0x00D75FAF, 0x00D75FD7, 0x00D75FFF, 0x00D78700, 0x00D7875F,
	0x00D78787, 0x00D787AF, 0x00D787D7, 0x00D787FF, 0x00D7AF00, 0x00D7AF5F,
	0x00D7AF87, 0x00D7AFAF, 0x00D7AFD7, 0x00D7AFFF, 0x00D7D700, 0x00D7D75F,
	0x00D7D787, 0x00D7D7AF, 0x00D7D7D7, 0x00D7D7FF, 0x00D7FF00, 0x00D7FF5F,
	0x00D7FF87, 0x00D7FFAF, 0x00D7FFD7, 0x00D7FFFF, 0x00FF0000, 0x00FF005F,
	0x00FF0087, 0x00FF00AF, 0x00FF00D7, 0x00FF00FF, 0x00FF5F00, 0x00FF5F5F,
	0x00FF5F87, 0x00FF5FAF, 0x00FF5FD7, 0x00FF5FFF, 0x00FF8700, 0x00FF875F,
	0x00FF8787, 0x00FF87AF, 0x00FF87D7, 0x00FF87FF, 0x00FFAF00, 0x00FFAF5F,
	0x00FFAF87, 0x00FFAFAF, 0x00FFAFD7, 0x00FFAFFF, 0x00FFD700, 0x00FFD75F,
	0x00FFD787, 0x00FFD7AF, 0x00FFD7D7, 0x00FFD7FF, 0x00FFFF00, 0x00FFFF5F,
	0x00FFFF87, 0x00FFFFAF, 0x00FFFFD7, 0x00FFFFFF, 0x00080808, 0x00121212,
	0x001C1C1C, 0x00262626, 0x00303030, 0x003A3A3A, 0x00444444, 0x004E4E4E,
	0x00585858, 0x00626262, 0x006C6C6C, 0x00767676, 0x00808080, 0x008A8A8A,
	0x00949494, 0x009E9E9E, 0x00A8A8A8, 0x00B2B2B2, 0x00BCBCBC, 0x00C6C6C6,
	0x00D0D0D0, 0x00DADADA, 0x00E4E4E4, 0x00EEEEEE,
};

/* ── Character grid + scrollback ── */

typedef struct {
    char ch;
    uint16_t attr;         /* bit15 set: xterm-256 index in low byte;
                            * else ATTR_* palette below */
} term_cell_t;

#define ATTR_TEXT   0
#define ATTR_PROMPT 1
#define ATTR_ERR    2
#define ATTR_256    0x8000  /* OR'ed with palette index 0..255 */

static term_cell_t term_lines[TERM_ROWS + TERM_SCROLLBACK][TERM_COLS];
static int term_top = TERM_SCROLLBACK;   /* index of first visible row */
static int term_row = 0;                 /* cursor row (screen space) */
static int term_col = 0;                 /* cursor column */
static int term_scrollback = 0;          /* >0: viewport raised rows */

/* ── Row-granular damage tracking ──
 * Which viewport rows changed since the last render.  -1 = none.
 * The main loop turns this into a tight dmg rect so Copland
 * re-composites one text row instead of the whole window per key. */
static int dmg_lo = -1, dmg_hi = -1;     /* inclusive [lo, hi] */

static void dmg_row(int r)
{
    if (r < 0)
        return;
    if (dmg_lo < 0 || r < dmg_lo)
        dmg_lo = r;
    if (dmg_hi < 0 || r > dmg_hi)
        dmg_hi = r;
}

static void dmg_all_rows(void)
{
    dmg_lo = 0;
    dmg_hi = TERM_ROWS - 1;
}

/* ── Child shell state ── */

static int shell_pid = -1;
static int pipe_in_fd = -1;    /* terminal writes keystrokes here */
static int pipe_out_fd = -1;   /* terminal reads shell output here */

/* ── Low-level drawing (BGRA little-endian) ── */

static void px(int x, int y, uint32_t c)
{
    if (x < 0 || x >= TERM_BUF_W || y < 0 || y >= TERM_BUF_H)
        return;
    term_buf[y * TERM_BUF_W + x] = c;
}

static void term_rect(int x, int y, int w, int h, uint32_t c)
{
    /* Row-granular fill with clipping — the old per-pixel loop
     * pushed every pixel through the px() bounds check call. */
    int x0 = x > 0 ? x : 0;
    int y0 = y > 0 ? y : 0;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > TERM_BUF_W)
        x1 = TERM_BUF_W;
    if (y1 > TERM_BUF_H)
        y1 = TERM_BUF_H;
    for (int r = y0; r < y1; r++)
        musr_fill32(&term_buf[(size_t)r * TERM_BUF_W + x0],
                    (size_t)(x1 - x0), c);
}

static void term_glyph(int x, int y, char ch, uint32_t fg)
{
    if (ch < 0x20 || ch > 0x7E)
        ch = '?';
    const unsigned char *g = &font8x16[(ch - 0x20) * FONT_H];
    for (int row = 0; row < FONT_H; row++) {
        unsigned char bits = g[row];
        if (!bits)
            continue;
        for (int col = 0; col < FONT_W; col++)
            if (bits & (0x80 >> col))
                px(x + col, y + row, fg);
    }
}

static uint32_t attr_color(uint16_t attr)
{
    if (attr & ATTR_256)
        return term_256_rgb[attr & 0xFF];
    switch (attr) {
    case ATTR_PROMPT: return TCOL_PROMPT;
    case ATTR_ERR:    return TCOL_ERR;
    default:          return TCOL_TEXT;
    }
}

/* ── Grid helpers ── */

static void term_clear_screen(void)
{
    for (int r = 0; r < TERM_ROWS + TERM_SCROLLBACK; r++)
        for (int c = 0; c < TERM_COLS; c++) {
            term_lines[r][c].ch = ' ';
            term_lines[r][c].attr = ATTR_TEXT;
        }
    term_top = TERM_SCROLLBACK;
    term_row = 0;
    term_col = 0;
    term_scrollback = 0;
}

static void term_scroll_up(void)
{
    if (term_top <= 0)
        return;
    for (int c = 0; c < TERM_COLS; c++) {
        term_lines[term_top - 1][c].ch = ' ';
        term_lines[term_top - 1][c].attr = ATTR_TEXT;
    }
    term_top--;
    if (term_row > 0)
        term_row--;
}

/* Move cursor to next line; scroll the buffer when at the bottom */
static void term_newline(void)
{
    term_col = 0;
    if (term_row < TERM_ROWS - 1) {
        dmg_row(term_row);
        term_row++;
        dmg_row(term_row);
        return;
    }
    /* Bottom: push everything up one row in the scrollback window */
    if (term_top > 0) {
        term_top--;
    } else {
        /* Scrollback exhausted: shift the whole array up.
         * Single overlap-safe block move (whole-array memmove,
         * aligned dword main loop).  Rows move up by one so
         * dst < src — the forward path is correct and fast. */
        musr_memmove(&term_lines[0], &term_lines[1],
                     sizeof(term_lines) - sizeof(term_lines[0]));
        for (int c = 0; c < TERM_COLS; c++) {
            term_lines[TERM_ROWS + TERM_SCROLLBACK - 1][c].ch = ' ';
            term_lines[TERM_ROWS + TERM_SCROLLBACK - 1][c].attr = ATTR_TEXT;
        }
    }
    for (int c = 0; c < TERM_COLS; c++) {
        term_lines[term_top + TERM_ROWS - 1][c].ch = ' ';
        term_lines[term_top + TERM_ROWS - 1][c].attr = ATTR_TEXT;
    }
    /* Scroll shifted every viewport row: whole body is damaged */
    dmg_all_rows();
}

static void term_putc_attr(char ch, uint16_t attr)
{
    if (ch == '\n') {
        term_newline();
        return;
    }
    if (ch == '\r') {
        term_col = 0;
        return;
    }
    if (ch == '\b') {
        if (term_col > 0)
            term_col--;
        term_lines[term_top + term_row][term_col].ch = ' ';
        term_lines[term_top + term_row][term_col].attr = ATTR_TEXT;
        dmg_row(term_row);
        return;
    }
    if (ch == '\t') {
        int n = 8 - (term_col % 8);
        while (n-- > 0 && term_col < TERM_COLS)
            term_lines[term_top + term_row][term_col++].ch = ' ';
        dmg_row(term_row);
        return;
    }
    if (ch < 0x20)
        return;
    if (term_col >= TERM_COLS) {
        term_newline();
    }
    term_lines[term_top + term_row][term_col].ch = ch;
    term_lines[term_top + term_row][term_col].attr = attr;
    term_col++;
    dmg_row(term_row);
}

/* ── ANSI escape filter ──
 * m4sh emits \x1B[..m color sequences; the grid stores a palette
 * index, and unknown sequences are swallowed.
 *
 * SGR support: 0 (reset) / 30-37 (fg) / 39 (default fg) / 90-97
 * (bright fg) / 38;5;n (xterm-256 fg) / 48;5;n (xterm-256 bg,
 * consumed but not rendered — the terminal keeps its dark body
 * background by design).  Anything else in a CSI sequence is
 * dropped as before. */

static int ansi_state = 0;   /* 0: normal, 1: saw ESC, 2: in CSI */
static int ansi_params[8];   /* collected SGR parameters */
static int ansi_nparams;
static int ansi_cur;         /* parameter under construction */

/* Current SGR foreground applied to incoming text (screen-space). */
static uint16_t sgr_fg = ATTR_TEXT;   /* ATTR_* or ATTR_256|idx */

/* Basic 8-color + bright maps → legacy ATTR_* (prompt/err keep
 * their dedicated shades; other basic colors fall back to text). */
static uint16_t sgr_basic_attr(int p)
{
    switch (p) {
    case 31: case 91: return ATTR_ERR;      /* red shades → err tint */
    case 32: case 92: return ATTR_PROMPT;   /* green shades → prompt */
    default:          return ATTR_TEXT;
    }
}

/* Serial evidence line: "[TERM] SGR <hex attr> rgb=r,g,b".
 * Pure digit/hex output (no printf machinery needed). */
static void sgr_report(void)
{
    char b[48];
    int n = 0;
    b[n++] = '['; b[n++] = 'T'; b[n++] = 'E'; b[n++] = 'R';
    b[n++] = 'M'; b[n++] = ']'; b[n++] = ' ';
    b[n++] = 'S'; b[n++] = 'G'; b[n++] = 'R'; b[n++] = ' ';
    uint16_t a = sgr_fg;
    for (int shift = 12; shift >= 0; shift -= 4) {
        int v = (a >> shift) & 0xF;
        b[n++] = v < 10 ? '0' + v : 'a' + v - 10;
    }
    b[n++] = ' ';
    b[n++] = 'r'; b[n++] = 'g'; b[n++] = 'b'; b[n++] = '=';
    uint32_t c = attr_color(sgr_fg);
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, bl = c & 0xFF;
    for (int comp = 0; comp < 3; comp++) {
        int v = comp == 0 ? r : comp == 1 ? g : bl;
        if (comp > 0)
            b[n++] = ',';
        if (v >= 100)
            b[n++] = '0' + v / 100;
        if (v >= 10)
            b[n++] = '0' + (v / 10) % 10;
        b[n++] = '0' + v % 10;
    }
    b[n++] = '\n';
    b[n] = 0;
    ser_puts(b);
}

/* Apply one parsed SGR sequence to the current colors. */
static void sgr_apply(const int *p, int np)
{
    int i = 0;
    if (np == 0) {
        /* \x1B[m == reset */
        sgr_fg = ATTR_TEXT;
        return;
    }
    while (i < np) {
        int v = p[i];
        if (v == 0) {
            sgr_fg = ATTR_TEXT;
        } else if (v == 39) {
            sgr_fg = ATTR_TEXT;
        } else if ((v >= 30 && v <= 37) || (v >= 90 && v <= 97)) {
            sgr_fg = sgr_basic_attr(v);
        } else if (v == 38 || v == 48) {
            /* extended color: only 5;n (256-color) supported */
            if (i + 2 < np && p[i + 1] == 5 &&
                p[i + 2] >= 0 && p[i + 2] <= 255) {
                if (v == 38)
                    sgr_fg = (uint16_t)(ATTR_256 | p[i + 2]);
                /* 48;5;n: consumed, not rendered (dark bg by design) */
                i += 2;
            } else {
                /* 38;2;r;g;b truecolor or malformed: skip the
                 * sub-params so they are not misread as codes */
                int j = i + 1;
                while (j < np && p[j] != 38 && p[j] != 48 &&
                       !(p[j] >= 30 && p[j] <= 39) &&
                       !(p[j] >= 90 && p[j] <= 97) && p[j] != 0)
                    j++;
                i = j - 1;
            }
        }
        i++;
    }
    sgr_report();
}

static void term_ansi_filter(char ch)
{
    if (ansi_state == 1) {
        if (ch == '[') {
            ansi_state = 2;
            ansi_nparams = 0;
            ansi_cur = 0;
            return;
        }
        ansi_state = 0;      /* non-CSI escape: drop */
        return;
    }
    if (ansi_state == 2) {
        if (ch >= '0' && ch <= '9') {
            if (ansi_cur < 1000)
                ansi_cur = ansi_cur * 10 + (ch - '0');
            return;
        }
        if (ch == ';') {
            if (ansi_nparams < 8)
                ansi_params[ansi_nparams++] = ansi_cur;
            ansi_cur = 0;
            return;
        }
        /* any other byte ends the sequence */
        ansi_state = 0;
        if (ansi_nparams < 8)
            ansi_params[ansi_nparams++] = ansi_cur;
        if (ch == 'm')
            sgr_apply(ansi_params, ansi_nparams);
        return;              /* non-SGR CSI: swallowed */
    }
    if (ch == 0x1B) {
        ansi_state = 1;
        return;
    }
    term_putc_attr(ch, sgr_fg);
}

/* ── Shell child plumbing ── */

static void term_puts_err(const char *s)
{
    while (*s)
        term_putc_attr(*s++, ATTR_ERR);
}

static void term_spawn_shell(void)
{
    /* Two separate pipes: the original single bidirectional pipe let
     * m4shg read back its own echo (global fd table, one shared
     * buffer for both directions) — every echoed char re-entered
     * cmd_buf and duplicated itself on each scheduling round
     * (observed: typed "echo" became "eeeeccc..." and the command
     * was never found).  kfd carries keystrokes (we write, shell
     * reads fd 0); ofd carries shell output (shell writes fd 1,
     * we read). */
    int kfd[2], ofd[2];
    if (musr_sc_pipe(kfd) != 0 || musr_sc_pipe(ofd) != 0) {
        term_puts_err("pipe failed\n");
        return;
    }
    int pid = musr_sc_fork();
    if (pid < 0) {
        term_puts_err("fork failed\n");
        return;
    }
    if (pid == 0) {
        /* Child: wire stdin/stdout onto the two pipes, then exec the
         * graphical shell in place.  NOTE: the 4P1 fd table is a
         * GLOBAL singleton — mkrn_fork_status(RFFDG) does not copy
         * it (see process.c) — so closing any end here would tear
         * down the shared pipe for the parent too; the parent owns
         * all four fds and keeps them open for the process lifetime. */
        musr_sc_dup2(kfd[0], 0);
        musr_sc_dup2(ofd[1], 1);
        int r = m4k_spawn("/bin/m4shg", 0);
        (void)r;
        m4k_exit(127);       /* exec failed */
    }
    pipe_in_fd = kfd[1];     /* we write keystrokes to shell stdin */
    pipe_out_fd = ofd[0];    /* we read shell stdout              */
    shell_pid = pid;
}

/* Drain shell output from the pipe into the grid (non-blocking). */
static int term_poll_output(void)
{
    if (pipe_out_fd < 0)
        return 0;
    char buf[128];
    int total = 0;
    for (;;) {
        int n = musr_sc_read(pipe_out_fd, buf, sizeof(buf));
        if (n <= 0)
            break;
        for (int i = 0; i < n; i++)
            term_ansi_filter(buf[i]);
        total += n;
        if (n < (int)sizeof(buf))
            break;
    }
    return total;
}

/* ── Rendering ── */

static void term_render(void)
{
    /* Chrome: title bar + Mac OS 9 controls */
    term_rect(0, 0, TERM_BUF_W, TERM_TITLE_H, TCOL_TITLE);
    term_glyph(CTRL_CLOSE_X, CTRL_Y, 'X', 0x00FFFFFF);
    term_glyph(CTRL_MIN_X, CTRL_Y, '-', 0x00FFFFFF);
    term_glyph(CTRL_MAX_X, CTRL_Y, '+', 0x00FFFFFF);
    const char *title = "m4sh";
    for (int i = 0; title[i]; i++)
        term_glyph(56 + i * FONT_W, CTRL_Y, title[i], 0x00C0C0C0);

    /* Body */
    term_rect(0, TERM_TITLE_H, TERM_BUF_W,
              TERM_BUF_H - TERM_TITLE_H, TCOL_BODY);

    /* Visible rows: term_top + term_scrollback .. + TERM_ROWS */
    int first = term_top + term_scrollback;
    for (int r = 0; r < TERM_ROWS; r++) {
        int src = first + r;
        int y = TERM_ORIGIN_Y + r * TERM_CHAR_H;
        for (int c = 0; c < TERM_COLS; c++) {
            term_cell_t cell = term_lines[src][c];
            if (cell.ch == ' ')
                continue;
            term_glyph(TERM_ORIGIN_X + c * TERM_CHAR_W, y,
                       cell.ch, attr_color(cell.attr));
        }
    }

    /* Scrollback indicator */
    if (term_scrollback > 0) {
        char ind[16];
        int n = 0;
        ind[n++] = '-';
        int v = term_scrollback;
        char tmp[8];
        int t = 0;
        do { tmp[t++] = '0' + v % 10; v /= 10; } while (v);
        while (t) ind[n++] = tmp[--t];
        ind[n++] = ' ';
        ind[n++] = 'u';
        ind[n++] = 'p';
        ind[n] = '\0';
        for (int i = 0; ind[i]; i++)
            term_glyph(TERM_BUF_W - 8 - i * FONT_W, TERM_TITLE_H + 4,
                       ind[i], TCOL_PROMPT);
    }

    /* Cursor block (only when viewport is at the live bottom) */
    if (term_scrollback == 0) {
        term_rect(TERM_ORIGIN_X + term_col * TERM_CHAR_W,
                  TERM_ORIGIN_Y + term_row * TERM_CHAR_H,
                  FONT_W, FONT_H, TCOL_TEXT);
    }
}

/* ── Key input: drain the mailbox from Sprach ── */

static void term_forward_key(unsigned char ch)
{
    if (pipe_in_fd < 0)
        return;
    musr_sc_write(pipe_in_fd, &ch, 1);
}

static void term_handle_key(unsigned char ch)
{
    if (ch == 0x01) {           /* PageUp: scroll up one page */
        int max_sb = TERM_SCROLLBACK > term_top ? term_top : TERM_SCROLLBACK;
        if (term_scrollback < max_sb) {
            term_scrollback += TERM_ROWS;
            if (term_scrollback > max_sb)
                term_scrollback = max_sb;
            dmg_all_rows();     /* whole viewport shifted */
        }
        return;
    }
    if (ch == 0x02) {           /* PageDown: scroll back down */
        if (term_scrollback > 0) {
            term_scrollback -= TERM_ROWS;
            if (term_scrollback < 0)
                term_scrollback = 0;
            dmg_all_rows();
        }
        return;
    }
    /* Any typed key snaps the viewport to the live bottom */
    if (term_scrollback) {
        term_scrollback = 0;
        dmg_all_rows();
        term_render();
    }
    term_forward_key(ch);
}

/* ── Entry point ── */

void _start(void)
{
    struct copland_shm *shm = copland_shm_get();
    struct term_mailbox *mb = (struct term_mailbox *)TERM_MAILBOX_BASE;

    /* Announce ourselves to Sprach (keyboard forwarding target) */
    mb->magic = TERM_MAILBOX_MAGIC;
    mb->write_idx = 0;
    mb->read_idx = 0;

    /* Create our window surface (Copland composites by slot order) */
    if (copland_cmd_push(shm, COPLAND_CMD_CREATE_SURFACE,
                         60, 40, TERM_W, TERM_H, (int32_t)TCOL_BODY,
                         COPLAND_SURF_VISIBLE) != 0) {
        ser_puts("[TERM] cmd ring full\n");
        m4k_exit(1);
    }

    /* Wait for Copland to process the create command: our surface is
     * the one matching our exact geometry with no buffer attached
     * yet.  Waiting on raw in-use counts is fragile when other
     * clients create/free surfaces concurrently. */
    int guard = 0;
    int my_slot = -1;
    while (guard++ < 2000000) {
        /* Yield to Copland WITHOUT consuming keyboard events —
         * m4k_get_keyboard_event() drains the kernel key buffer and
         * would steal the WM's keystrokes (Sprach forwards keys to us
         * through the mailbox, never through the kernel buffer). */
        m4k_yield();
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
            if (shm->surfaces[i].in_use && !shm->surfaces[i].buffer_ptr &&
                shm->surfaces[i].w == TERM_W &&
                shm->surfaces[i].h == TERM_H) {
                my_slot = i;
                break;
            }
        }
        if (my_slot >= 0)
            break;
    }
    if (my_slot < 0) {
        ser_puts("[TERM] no surface slot\n");
        m4k_exit(1);
    }

    /* Own the buffer: Sprach never repaints this surface */
    shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)term_buf;

    /* Announce our geometry so Sprach's poll adopts THIS surface,
     * not some boot demo window that happens to be ≥50x50. */
    mb->surf_w = TERM_W;
    mb->surf_h = TERM_H;

    term_clear_screen();
    term_puts_err("M4KK1 Terminal [m4sh]\n");

    /* Fork + exec the real graphical shell */
    term_spawn_shell();

    term_render();
    shm->dirty = 1;

    ser_puts("[TERM] terminal ready (slot=");
    print_u32((uint32_t)my_slot);
    ser_puts(")\n");

    /* Main loop */
    for (;;) {
        if (!shm->surfaces[my_slot].in_use)
            break;                      /* WM closed us */

        int visible =
            (shm->surfaces[my_slot].flags & COPLAND_SURF_VISIBLE);
        int need_render = 0;

        if (visible) {
            while (mb->read_idx != mb->write_idx) {
                unsigned char ch = mb->buf[mb->read_idx];
                mb->read_idx = (mb->read_idx + 1) % TERM_MAILBOX_SIZE;
                term_handle_key(ch);
                need_render = 1;
            }
            if (term_poll_output() > 0)
                need_render = 1;
            if (need_render) {
                term_render();
                /* Row-granular damage: only the text rows that actually
                 * changed since the last render are re-composited (one
                 * 16-px row per keystroke instead of the whole window).
                 * Falls back to the full window when the damage tracker
                 * is empty (first frame) or the viewport scrolled. */
                if (dmg_lo >= 0 && term_scrollback == 0) {
                    shm->surfaces[my_slot].dmg_x =
                        shm->surfaces[my_slot].x;
                    shm->surfaces[my_slot].dmg_y =
                        shm->surfaces[my_slot].y + TERM_ORIGIN_Y +
                        dmg_lo * TERM_CHAR_H;
                    shm->surfaces[my_slot].dmg_w =
                        shm->surfaces[my_slot].w;
                    shm->surfaces[my_slot].dmg_h =
                        (dmg_hi - dmg_lo + 1) * TERM_CHAR_H;
                } else {
                    shm->surfaces[my_slot].dmg_x =
                        shm->surfaces[my_slot].x;
                    shm->surfaces[my_slot].dmg_y =
                        shm->surfaces[my_slot].y;
                    shm->surfaces[my_slot].dmg_w =
                        shm->surfaces[my_slot].w;
                    shm->surfaces[my_slot].dmg_h =
                        shm->surfaces[my_slot].h;
                }
                dmg_lo = dmg_hi = -1;
            }
        }

        m4k_yield();
    }

    ser_puts("[TERM] surface gone, killing shell pid=");
    print_u32((uint32_t)(shell_pid > 0 ? shell_pid : 0));
    ser_puts("\n");
    if (shell_pid > 0)
        m4k_kill(shell_pid, 2 /* SIGKILL */);
    m4k_exit(0);
}
