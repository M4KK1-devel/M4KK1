/*
 * ==== CALC — Graphical Calculator ====
 * M4KK1 4P1 - calc_gui.c
 * Description: Four-operation calculator with parentheses,
 *              operator precedence, and an input history.
 *
 * A standalone Copland client (same model as /bin/fm): owns a pixel
 * buffer, receives keyboard events through the CALC key mailbox that
 * Sprach fills, and renders a display area plus a 4-column button
 * grid (clickless — keys typed directly, grid shows the layout).
 *
 * Keyboard: digits, + - * / ( ) . C CE =, Backspace, Enter ==,
 *           ArrowUp/Down walk the history.
 *
 * Integer arithmetic per spec (int); parentheses are tracked in the
 * expression string and evaluated by a recursive-descent parser.
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
#define CA_W           260
#define CA_H           340
#define CA_TITLE_H     18
#define CA_DISP_H      60

/* Colors (BGRA) */
#define CA_COL_TITLE   0x00A05010
#define CA_COL_BODY    0x00303038
#define CA_COL_DISP    0x00181820
#define CA_COL_TEXT    0x00FFFFFF
#define CA_COL_BTN     0x00404048
#define CA_COL_BTNOP   0x00A06010
#define CA_COL_BTNFN   0x00606068
#define CA_COL_HIST    0x00808080

/* ── Expression & history ── */
#define CA_EXPR_MAX    64
#define CA_HIST_MAX    16

static char expr[CA_EXPR_MAX];
static int expr_len = 0;
static char result_str[32] = "0";
static int error_flag = 0;       /* 1 = syntax error shown */

static char history[CA_HIST_MAX][CA_EXPR_MAX];
static int hist_count = 0;
static int hist_browse = -1;     /* -1 = live entry */

/* ── CALC key mailbox (Sprach writes, calc reads) ── */
#define CA_MAILBOX_BASE   0x00630000u
#define CA_MAILBOX_MAGIC  0x43414C31u   /* "CAL1" */
#define CA_MAILBOX_SIZE   64

struct ca_mailbox {
    uint32_t magic;
    uint32_t write_idx;
    uint32_t read_idx;
    unsigned char buf[CA_MAILBOX_SIZE];
};
static volatile struct ca_mailbox *ca_mb =
    (volatile struct ca_mailbox *)CA_MAILBOX_BASE;

/* ── Pixel buffer ── */
static uint32_t ca_buf[CA_W * CA_H];

/* ══════════════ tiny 5x7 font ══════════════ */
static const uint8_t ca_font5x7[96][5] = {
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
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,00,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},
    {0x08,0x08,0x2A,0x1C,0x08},{0,0,0,0,0},{0x00,0x00,0x00,0x00,0x00},
};

/* ── drawing helpers ── */
static void ca_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > CA_W) w = CA_W - x;
    if (y + h > CA_H) h = CA_H - y;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            ca_buf[yy * CA_W + xx] = c;
}

static void ca_char(int x, int y, char ch, uint32_t c)
{
    if (ch < 32 || ch > 126 || x < 0 || y < 0 ||
        x + 5 > CA_W || y + 7 > CA_H)
        return;
    const uint8_t *g = ca_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row))
                ca_buf[(y + row) * CA_W + x + col] = c;
    }
}

static int ca_strlen(const char *s)
{
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void ca_str(int x, int y, const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        ca_char(x + i * 6, y, s[i], c);
}

static void ca_str_clip(int x, int y, const char *s, int maxch, uint32_t c)
{
    for (int i = 0; s && s[i] && i < maxch; i++)
        ca_char(x + i * 6, y, s[i], c);
}

static void ca_itoa(int v, char *out)
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

/* ── 2x scale font for the display ── */
static void ca_char2(int x, int y, char ch, uint32_t c)
{
    if (ch < 32 || ch > 126) return;
    const uint8_t *g = ca_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row)) {
                ca_rect(x + col * 2, y + row * 2, 2, 2, c);
            }
    }
}

static void ca_str2(int x, int y, const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        ca_char2(x + i * 12, y, s[i], c);
}

/* ══════════════ recursive-descent integer parser ══════════════ */
/*
 * Grammar (classic precedence climbing):
 *   expr   := term (('+'|'-') term)*
 *   term   := factor (('*'|'/') factor)*
 *   factor := '-' factor | '(' expr ')' | number
 * Division by zero reports an error (error_flag).
 */
static const char *p_src;
static int p_err;

/* ── integer helpers for the extended operator set ── */

static int ca_isqrt(int v)
{
    /* integer sqrt, v >= 0 else error */
    if (v < 0) { p_err = 1; return 0; }
    int r = 0, odd = 1;
    while (v >= odd) {
        v -= odd;
        odd += 2;
        r++;
    }
    return r;
}

static int ca_ipow(int b, int e)
{
    if (e < 0) { p_err = 1; return 0; }
    int r = 1;
    while (e-- > 0) {
        /* overflow guard: |r*b| must stay under 2^30 */
        if (b != 0 && (r > (1 << 30) / (b < 0 ? -b : b))) {
            p_err = 1;
            return 0;
        }
        r *= b;
    }
    return r;
}

static int p_expr(void);

static void p_skip(void)
{
    while (*p_src == ' ') p_src++;
}

static int p_number(void)
{
    int v = 0;
    p_skip();
    if (!(*p_src >= '0' && *p_src <= '9')) {
        p_err = 1;
        return 0;
    }
    while (*p_src >= '0' && *p_src <= '9') {
        v = v * 10 + (*p_src - '0');
        p_src++;
    }
    return v;
}

static int p_factor(void)
{
    p_skip();
    if (*p_src == '-') {
        p_src++;
        return -p_factor();
    }
    if (*p_src == 'r') {            /* r<expr> = sqrt(expr) */
        p_src++;
        int v = ca_isqrt(p_factor());
        return v;
    }
    if (*p_src == '(') {
        p_src++;
        int v = p_expr();
        p_skip();
        if (*p_src != ')') {
            p_err = 1;
            return 0;
        }
        p_src++;
        return v;
    }
    return p_number();
}

static int p_power(void)
{
    int v = p_factor();
    p_skip();
    if (*p_src == '^') {
        p_src++;
        return ca_ipow(v, p_power());   /* right-assoc */
    }
    return v;
}

static int p_term(void)
{
    int v = p_power();
    for (;;) {
        p_skip();
        if (*p_src == '*') {
            p_src++;
            v = v * p_power();
        } else if (*p_src == '/') {
            p_src++;
            int d = p_power();
            if (d == 0) { p_err = 1; return 0; }
            v = v / d;
        } else if (*p_src == '%') {
            p_src++;
            int d = p_power();
            if (d == 0) { p_err = 1; return 0; }
            v = v % d;
        } else break;
    }
    return v;
}

static int p_expr(void)
{
    int v = p_term();
    for (;;) {
        p_skip();
        if (*p_src == '+') {
            p_src++;
            v = v + p_term();
        } else if (*p_src == '-') {
            p_src++;
            v = v - p_term();
        } else break;
    }
    return v;
}

static int ca_eval(const char *s, int *ok)
{
    p_src = s;
    p_err = 0;
    int v = p_expr();
    p_skip();
    if (*p_src != 0) p_err = 1;   /* trailing garbage */
    *ok = !p_err;
    return v;
}

/* ══════════════ key handling ══════════════ */
static void ca_push_hist(void)
{
    if (expr_len == 0) return;
    if (hist_count < CA_HIST_MAX) {
        musr_strncpy(history[hist_count], expr, CA_EXPR_MAX - 1);
        hist_count++;
    } else {
        for (int i = 0; i < CA_HIST_MAX - 1; i++)
            musr_strncpy(history[i], history[i + 1], CA_EXPR_MAX - 1);
        musr_strncpy(history[CA_HIST_MAX - 1], expr, CA_EXPR_MAX - 1);
    }
}

static void ca_compute(void)
{
    if (expr_len == 0) return;
    ca_push_hist();
    hist_browse = -1;
    int ok;
    int v = ca_eval(expr, &ok);
    if (ok) {
        ca_itoa(v, result_str);
        error_flag = 0;
        /* result becomes the next entry start (continuous math) */
        expr_len = 0;
        expr[0] = 0;
        musr_strncpy(expr, result_str, CA_EXPR_MAX - 1);
        expr_len = ca_strlen(expr);
    } else {
        musr_strncpy(result_str, "ERROR", 6);
        error_flag = 1;
    }
}

static void ca_hist_walk(int older)
{
    if (hist_count == 0) return;
    if (older) {
        if (hist_browse < 0)
            hist_browse = hist_count - 1;
        else if (hist_browse > 0)
            hist_browse--;
    } else {
        if (hist_browse >= 0)
            hist_browse++;
        if (hist_browse >= hist_count)
            hist_browse = -1;   /* back to live */
    }
    if (hist_browse < 0) {
        expr[0] = 0;
        expr_len = 0;
    } else {
        musr_strncpy(expr, history[hist_browse], CA_EXPR_MAX - 1);
        expr_len = ca_strlen(expr);
    }
}

static void ca_key(unsigned char ch)
{
    if (ch == '\r' || ch == '\n' || ch == '=') {
        ca_compute();
        return;
    }
    if (ch == 0x1B) {                 /* Esc = C (clear all) */
        expr[0] = 0; expr_len = 0;
        result_str[0] = '0'; result_str[1] = 0;
        error_flag = 0;
        return;
    }
    if (ch == '\b' || ch == 0x7F) {
        if (expr_len > 0) expr[--expr_len] = 0;
        return;
    }
    if (ch == 0x11) {                 /* Ctrl-Q raw up: history older */
        ca_hist_walk(1);
        return;
    }
    if (ch == 0x12) {                 /* Ctrl-R: history newer */
        ca_hist_walk(0);
        return;
    }
    if (ch == 'c' || ch == 'C') {     /* C: clear */
        expr[0] = 0; expr_len = 0;
        musr_strncpy(result_str, "0", 2);
        error_flag = 0;
        return;
    }
    if (ch == 'e' || ch == 'E') {     /* CE: clear entry only */
        expr[0] = 0; expr_len = 0;
        return;
    }
    if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' ||
        ch == '*' || ch == '/' || ch == '(' || ch == ')' ||
        ch == '.' || ch == '%' || ch == '^' || ch == 'r') {
        if (expr_len < CA_EXPR_MAX - 1) {
            expr[expr_len++] = ch;
            expr[expr_len] = 0;
        }
    }
}

/* ══════════════ rendering ══════════════ */
static const char *const ca_grid[6][4] = {
    { "C",  "CE", "(",  ")" },
    { "7",  "8",  "9",  "/" },
    { "4",  "5",  "6",  "*" },
    { "1",  "2",  "3",  "-" },
    { "0",  ".",  "=",  "+" },
    { "%",  "r",  "^",  "BK" },
};
#define CA_GRID_X 8
#define CA_GRID_Y (CA_TITLE_H + CA_DISP_H + 6)
#define CA_BTN_W  ((CA_W - 8 * 2 - 3 * 6) / 4)
#define CA_BTN_H  36

static void ca_render(struct copland_shm *shm, int slot)
{
    ca_rect(0, 0, CA_W, CA_H, CA_COL_BODY);
    ca_rect(0, 0, CA_W, CA_TITLE_H, CA_COL_TITLE);
    ca_str(6, 5, "calc", 0x00FFFFFF);

    /* display: history hint (top, small) + expression/result (2x) */
    ca_rect(4, CA_TITLE_H + 2, CA_W - 8, CA_DISP_H - 4, CA_COL_DISP);
    if (hist_browse >= 0) {
        char tag[CA_EXPR_MAX + 8];
        tag[0] = 0;
        musr_strncpy(tag + 0, "hist ", 6);
        musr_strncpy(tag + 5, history[hist_browse],
                     sizeof(tag) - 6);
        ca_str_clip(8, CA_TITLE_H + 6, tag, (CA_W - 20) / 6,
                    CA_COL_HIST);
    } else if (hist_count > 0) {
        ca_str_clip(8, CA_TITLE_H + 6, history[hist_count - 1],
                    (CA_W - 20) / 6, CA_COL_HIST);
    }
    /* big right-aligned result / live expression */
    {
        const char *big = error_flag ? result_str :
                          (expr_len ? expr : result_str);
        int len = ca_strlen(big);
        int maxch = (CA_W - 24) / 12;
        int show = len > maxch ? len - maxch : 0;
        ca_str2(CA_W - 12 - (len - show) * 12,
                CA_TITLE_H + CA_DISP_H - 26,
                big + show, error_flag ? 0x00FF6060 : CA_COL_TEXT);
    }

    /* button grid */
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 4; c++) {
            int x = CA_GRID_X + c * (CA_BTN_W + 6);
            int y = CA_GRID_Y + r * (CA_BTN_H + 6);
            const char *lbl = ca_grid[r][c];
            uint32_t bg = CA_COL_BTN;
            if (lbl[0] == '/' || lbl[0] == '*' ||
                (lbl[0] == '+' && !lbl[1]) ||
                (lbl[0] == '-' && !lbl[1]))
                bg = CA_COL_BTNOP;
            else if (lbl[0] == 'C' || (lbl[0] == '(' ) ||
                     (lbl[0] == ')' ))
                bg = CA_COL_BTNFN;
            ca_rect(x, y, CA_BTN_W, CA_BTN_H, bg);
            ca_rect(x, y, CA_BTN_W, 1, 0x00606068);
            int lw = ca_strlen(lbl) * 6;
            ca_str(x + (CA_BTN_W - lw) / 2, y + (CA_BTN_H - 7) / 2,
                   lbl, 0x00F0F0F0);
        }
    }

    /* hint line */
    ca_str(6, CA_H - 12, "type expr, Enter =, Ctrl-Q/R hist",
           CA_COL_HIST);

    /* publish */
    shm->surfaces[slot].buffer_ptr = (uint32_t)(uintptr_t)ca_buf;
    shm->surfaces[slot].dmg_x = shm->surfaces[slot].x;
    shm->surfaces[slot].dmg_y = shm->surfaces[slot].y;
    shm->surfaces[slot].dmg_w = shm->surfaces[slot].w;
    shm->surfaces[slot].dmg_h = shm->surfaces[slot].h;
    shm->dirty = 1;
}

/* ══════════════ main ══════════════ */
void _start(void)
{
    ser_puts("[CALC] calculator starting\n");

    struct copland_shm *shm = copland_shm_get();
    if (!shm || shm->magic != COPLAND_SHM_MAGIC) {
        ser_puts("[CALC] copland not ready\n");
        m4k_exit(1);
    }

    /* Register our key mailbox */
    ca_mb->magic = CA_MAILBOX_MAGIC;
    ca_mb->write_idx = 0;
    ca_mb->read_idx = 0;

    musr_strncpy(result_str, "0", 2);

    /* Create our surface */
    if (copland_cmd_push(shm, COPLAND_CMD_CREATE_SURFACE,
                         200, 100, CA_W, CA_H, (int32_t)CA_COL_TITLE,
                         COPLAND_SURF_VISIBLE) != 0) {
        ser_puts("[CALC] cmd ring full\n");
        m4k_exit(1);
    }

    /* Wait for Copland to allocate our slot (260-wide surface) */
    int guard = 0;
    int my_slot = -1;
    while (guard++ < 200000) {
        m4k_yield();
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (shm->surfaces[i].in_use &&
                !shm->surfaces[i].buffer_ptr &&
                shm->surfaces[i].w == CA_W) {
                my_slot = i;
                break;
            }
        if (my_slot >= 0) break;
    }
    if (my_slot < 0) {
        ser_puts("[CALC] surface allocation timeout\n");
        m4k_exit(1);
    }
    shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)ca_buf;
    ser_puts("[CALC] surface ready\n");

    int need_render = 1;
    for (;;) {
        if (!(shm->surfaces[my_slot].flags & COPLAND_SURF_VISIBLE))
            m4k_exit(0);

        while (ca_mb->read_idx != ca_mb->write_idx) {
            unsigned char ch = ca_mb->buf[ca_mb->read_idx];
            ca_mb->read_idx =
                (ca_mb->read_idx + 1) % CA_MAILBOX_SIZE;
            ca_key(ch);
            need_render = 1;
        }

        if (need_render) {
            ca_render(shm, my_slot);
            need_render = 0;
        }
        m4k_yield();
    }
}
