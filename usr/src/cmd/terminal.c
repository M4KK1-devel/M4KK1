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

/* ── Character grid + scrollback ── */

typedef struct {
    char ch;
    uint8_t attr;          /* palette index 0..7 */
} term_cell_t;

#define ATTR_TEXT   0
#define ATTR_PROMPT 1
#define ATTR_ERR    2

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
    for (int r = y; r < y + h; r++)
        for (int cc = x; cc < x + w; cc++)
            px(cc, r, c);
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

static uint32_t attr_color(uint8_t attr)
{
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

static void term_putc_attr(char ch, uint8_t attr)
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
 * index, and unknown sequences are swallowed. */

static int ansi_state = 0;   /* 0: normal, 1: saw ESC, 2: in CSI */

static void term_ansi_filter(char ch, uint8_t def_attr)
{
    if (ansi_state == 1) {
        if (ch == '[') {
            ansi_state = 2;
            return;
        }
        ansi_state = 0;      /* non-CSI escape: drop */
        return;
    }
    if (ansi_state == 2) {
        if (ch == 'm' || ch < 0x20) {
            /* end of SGR (or aborted): map basic colors */
            ansi_state = 0;
        }
        return;              /* all CSI params dropped */
    }
    if (ch == 0x1B) {
        ansi_state = 1;
        return;
    }
    term_putc_attr(ch, def_attr);
}

/* ── Shell child plumbing ── */

static void term_puts_err(const char *s)
{
    while (*s)
        term_putc_attr(*s++, ATTR_ERR);
}

static void term_spawn_shell(void)
{
    int fds[2];
    if (musr_sc_pipe(fds) != 0) {
        term_puts_err("pipe failed\n");
        return;
    }
    /* fds[0]=read end, fds[1]=write end.  Keep the shell-side ends in
     * the parent's table for now; the fork child inherits them. */
    int pid = musr_sc_fork();
    if (pid < 0) {
        musr_sc_close(fds[0]);
        musr_sc_close(fds[1]);
        term_puts_err("fork failed\n");
        return;
    }
    if (pid == 0) {
        /* Child: wire the pipe onto stdin/stdout, close the copies,
         * then exec the graphical shell in place. */
        musr_sc_dup2(fds[0], 0);
        musr_sc_dup2(fds[1], 1);
        musr_sc_close(fds[0]);
        musr_sc_close(fds[1]);
        int r = m4k_spawn("/bin/m4shg", 0);
        (void)r;
        m4k_exit(127);       /* exec failed */
    }
    /* Parent (the terminal): keep the write end for keystrokes and the
     * read end for output.  The child's dup2'd copies keep the pipe
     * alive after we close the originals below. */
    pipe_in_fd = fds[1];     /* we write to shell stdin  */
    pipe_out_fd = fds[0];    /* we read shell stdout     */
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
            term_ansi_filter(buf[i], ATTR_TEXT);
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
