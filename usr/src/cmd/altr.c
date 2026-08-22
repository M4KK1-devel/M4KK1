/*
 * ==== ALTR — Text Editor (Vim-style) ====
 * M4KK1 4P1 - altr.c
 * Description: Graphical text editor with a file tree, Vim modal
 *              editing, syntax highlighting and a sead-style
 *              substitute command.
 *
 * A standalone Copland client (same model as /bin/fm): owns a pixel
 * buffer rendered via its surface, receives keyboard events through
 * the ALTR key mailbox that Sprach fills, and renders:
 *   - left panel  : file tree of /export/root (j/k navigate,
 *                   Enter descends into dirs / opens files)
 *   - right area  : editing buffer with syntax highlighting
 *                   (.c/.h keywords+strings+comments, .asm mnemonics
 *                   +registers, .sh keywords + $VARS)
 *
 * Vim modal editing:
 *   NORMAL: h/j/k/l move, x delete char, dd delete line, /search,
 *           n/N repeat, : command line (w/wq/q!/s/%s/n/N/sead)
 *   INSERT: i/a/o/O enter, Esc leaves, printable chars, Backspace,
 *           Enter, Tab (4 spaces)
 *   Tab completion: in :e/:w paths and / search — completes file
 *           names from the referenced directory.
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
#define AL_W           640
#define AL_H           460
#define AL_TITLE_H     18
#define AL_TREE_W      190
#define AL_STATUS_H    16
#define AL_CMD_H       16

/* Colors (BGRA) */
#define AL_COL_TITLE   0x00A03010
#define AL_COL_BODY    0x00181820
#define AL_COL_TREEBG  0x00202028
#define AL_COL_TEXT    0x00D0D0D0
#define AL_COL_SEL     0x003060C0
#define AL_COL_CURSOR  0x00FFFFFF
#define AL_COL_STATUS  0x00A0A0A0

/* Syntax colors */
#define SY_COL_KW      0x0000B0FF   /* blue   (BGRA: B high) */
#define SY_COL_STR     0x0000C050   /* green  */
#define SY_COL_COM     0x00808080   /* grey   */
#define SY_COL_INSN    0x000090FF   /* orange */
#define SY_COL_REG     0x00E000B0   /* purple */
#define SY_COL_VAR     0x0000E0E0   /* yellow */

/* ── Editor buffer ── */
#define AL_MAX_LINES   1200
#define AL_MAX_LINELEN 200
#define AL_MAX_FILES   64

static char ed_lines[AL_MAX_LINES][AL_MAX_LINELEN];
static int ed_nlines = 0;
static char ed_path[160] = "";
static int ed_dirty = 0;

/* Cursor / view */
static int cur_line, cur_col;        /* logical cursor */
static int top_line;                 /* first visible line */
static int left_col;
static int mode_insert;              /* 0 = NORMAL, 1 = INSERT */
#define AL_TEXT_X   (AL_TREE_W + 8)
#define AL_TEXT_Y   (AL_TITLE_H + 4)
#define AL_CH_W     6
#define AL_CH_H     12
#define AL_COLS     ((AL_W - AL_TEXT_X - 8) / AL_CH_W)
#define AL_ROWS     ((AL_H - AL_TEXT_Y - AL_STATUS_H - AL_CMD_H - 8) / AL_CH_H)

/* ── File tree ── */
struct al_tree_ent {
    char name[DIRENT_NAME_MAX];
    int is_dir;
};
static struct al_tree_ent tree_ents[AL_MAX_FILES];
static int tree_count = 0;
static char tree_cwd[160] = "/export/root";
static int tree_sel = 0;
static int tree_active = 0;          /* 1 = focus on tree panel */

/* ── Command line (: prefix) and search (/ prefix) ── */
static char cmdline[96];
static int cmdline_len = 0;
static int cmdline_on = 0;
static char cmdline_pfx;             /* ':' or '/' */
static char search_pat[64] = "";

/* ── Tabs ── */
static char tab_complete_buf[160];
static int tab_pending = 0;

/* ── ALTR key mailbox (Sprach writes, altr reads) ── */
#define AL_MAILBOX_BASE   0x00620000u
#define AL_MAILBOX_MAGIC  0x414C5431u   /* "ALT1" */
#define AL_MAILBOX_SIZE   64

struct al_mailbox {
    uint32_t magic;
    uint32_t write_idx;
    uint32_t read_idx;
    unsigned char buf[AL_MAILBOX_SIZE];
};
static volatile struct al_mailbox *al_mb =
    (volatile struct al_mailbox *)AL_MAILBOX_BASE;

/* ── Pixel buffer (placed in .bss at the altr load address) ── */
static uint32_t al_buf[AL_W * AL_H];

/* ══════════════ tiny 5x7 font (same subset as fm.c) ══════════════ */
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
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},
    {0x08,0x08,0x2A,0x1C,0x08},{0,0,0,0,0},{0x00,0x00,0x00,0x00,0x00},
};

/* ── drawing helpers ── */
static void al_rect(int x, int y, int w, int h, uint32_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > AL_W) w = AL_W - x;
    if (y + h > AL_H) h = AL_H - y;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            al_buf[yy * AL_W + xx] = c;
}

static void al_char(int x, int y, char ch, uint32_t c)
{
    if (ch < 32 || ch > 126 || x < 0 || y < 0 ||
        x + 5 > AL_W || y + 7 > AL_H)
        return;
    const uint8_t *g = al_font5x7[ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++)
            if (bits & (1u << row))
                al_buf[(y + row) * AL_W + x + col] = c;
    }
}

static int al_strlen(const char *s)
{
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void al_str(int x, int y, const char *s, uint32_t c)
{
    for (int i = 0; s && s[i]; i++)
        al_char(x + i * 6, y, s[i], c);
}

static void al_str_clip(int x, int y, const char *s, int maxch, uint32_t c)
{
    for (int i = 0; s && s[i] && i < maxch; i++)
        al_char(x + i * 6, y, s[i], c);
}

/* itoa into caller buffer (decimal, signed) */
static void al_itoa(int v, char *out)
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

/* local memmove — the PCC runtime does not provide one */
static void *al_memmove(void *dst, const void *src, unsigned n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *sr = (const unsigned char *)src;
    if (d < sr) {
        for (unsigned i = 0; i < n; i++)
            d[i] = sr[i];
    } else if (d > sr) {
        for (unsigned i = n; i > 0; i--)
            d[i - 1] = sr[i - 1];
    }
    return dst;
}

/* forward decls */
void altr_load(const char *path);

/* ══════════════ string helpers (m4sh.h has musr_strncpy etc.) ══ */
static int al_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static int al_starts(const char *s, const char *pfx)
{
    while (*pfx) {
        if (*s != *pfx) return 0;
        s++; pfx++;
    }
    return 1;
}

static int al_endswith(const char *s, const char *sfx)
{
    int sl = al_strlen(s), fl = al_strlen(sfx);
    if (fl > sl) return 0;
    for (int i = 0; i < fl; i++)
        if (s[sl - fl + i] != sfx[i]) return 0;
    return 1;
}

static void al_path_join(char *dst, int dstsz, const char *dir,
                         const char *name)
{
    int dl = al_strlen(dir);
    int i = 0;
    if (dl >= dstsz - 2) return;
    for (; i < dl; i++) dst[i] = dir[i];
    if (dl && dst[dl - 1] != '/' && dl < dstsz - 2)
        dst[dl++] = '/';
    for (int j = 0; name[j] && dl < dstsz - 1; j++)
        dst[dl++] = name[j];
    dst[dl] = 0;
}

/* ══════════════ file tree ══════════════ */
static void al_tree_reload(void)
{
    tree_count = 0;
    int fd = musr_sc_open(tree_cwd, O_RDONLY);
    if (fd < 0)
        return;
    static struct dirent ents[16];
    for (;;) {
        int n = musr_sc_getdents(fd, ents, 16);
        if (n <= 0) break;
        for (int i = 0; i < n && tree_count < AL_MAX_FILES; i++) {
            char *nm = ents[i].name;
            if (nm[0] == '.' && (!nm[1] || (nm[1] == '.' && !nm[2])))
                continue;   /* . .. */
            int isd = (ents[i].type == 2);   /* DT_DIR per fm.c */
            musr_strncpy(tree_ents[tree_count].name, nm,
                         DIRENT_NAME_MAX - 1);
            tree_ents[tree_count].is_dir = isd;
            tree_count++;
        }
        if (tree_count >= AL_MAX_FILES) break;
    }
    musr_sc_close(fd);
}

static void al_tree_open(void)
{
    if (tree_sel < 0 || tree_sel >= tree_count)
        return;
    struct al_tree_ent *e = &tree_ents[tree_sel];
    char full[192];
    al_path_join(full, sizeof(full), tree_cwd, e->name);
    if (e->is_dir) {
        musr_strncpy(tree_cwd, full, sizeof(tree_cwd) - 1);
        tree_sel = 0;
        al_tree_reload();
        return;
    }
    /* open file in the editor */
    musr_sc_open(full, O_RDONLY);   /* existence check via load below */
    tree_active = 0;
    altr_load(full);
}

/* ══════════════ syntax highlighting ══════════════ */
enum al_syn { SY_NONE, SY_C, SY_ASM, SY_SH };

static const char *const c_kws[] = {
    "int", "char", "void", "return", "if", "else", "while", "for",
    "do", "switch", "case", "break", "continue", "static", "struct",
    "enum", "typedef", "const", "unsigned", "sizeof", "extern",
    "volatile", "goto", "union", "long", "short", "float", "double",
    NULL
};
static const char *const asm_insns[] = {
    "mov", "add", "sub", "mul", "div", "jmp", "call", "ret", "push",
    "pop", "cmp", "je", "jne", "jz", "jnz", "jl", "jg", "inc", "dec",
    "and", "or", "xor", "not", "shl", "shr", "int", "nop", "lea",
    "loop", "stosb", "lodsb", "cli", "sti", "hlt", "iret", "xchg",
    NULL
};
static const char *const asm_regs[] = {
    "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip",
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    NULL
};
static const char *const sh_kws[] = {
    "echo", "if", "then", "else", "fi", "for", "while", "do", "done",
    "case", "esac", "function", "return", "export", "local", "exit",
    NULL
};

static enum al_syn al_syn_of(const char *path)
{
    if (al_endswith(path, ".c") || al_endswith(path, ".h"))
        return SY_C;
    if (al_endswith(path, ".asm") || al_endswith(path, ".S") ||
        al_endswith(path, ".s"))
        return SY_ASM;
    if (al_endswith(path, ".sh"))
        return SY_SH;
    return SY_NONE;
}

static int al_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int al_in_list(const char *w, const char *const *lst)
{
    for (int i = 0; lst[i]; i++)
        if (al_streq(w, lst[i]))
            return 1;
    return 0;
}

/*
 * al_draw_line - render one line with syntax colors.
 * Returns the number of chars consumed from s (<= maxch), stopping
 * early only at end of string.
 */
static void al_draw_line(int x, int y, const char *s, int maxch,
                         enum al_syn syn)
{
    int i = 0;
    while (s[i] && i < maxch) {
        if (syn == SY_C &&
            (s[i] == '/' && s[i + 1] == '/' ||
             s[i] == '/' && s[i + 1] == '*')) {
            al_str_clip(x + i * 6, y, s + i, maxch - i, SY_COL_COM);
            return;
        }
        if (syn == SY_C && s[i] == '"') {
            int j = i + 1;
            while (s[j] && s[j] != '"' && j < maxch) j++;
            if (s[j] == '"') j++;
            al_str_clip(x + i * 6, y, s + i, j - i, SY_COL_STR);
            i = j;
            continue;
        }
        if (syn == SY_SH && s[i] == '#') {
            al_str_clip(x + i * 6, y, s + i, maxch - i, SY_COL_COM);
            return;
        }
        if (syn == SY_ASM && s[i] == ';') {
            al_str_clip(x + i * 6, y, s + i, maxch - i, SY_COL_COM);
            return;
        }
        if (syn == SY_SH && s[i] == '$') {
            int j = i + 1;
            while (al_word_char(s[j]) && j < maxch) j++;
            al_str_clip(x + i * 6, y, s + i, j - i, SY_COL_VAR);
            i = j;
            continue;
        }
        if (al_word_char(s[i])) {
            int j = i;
            while (al_word_char(s[j]) && j < maxch) j++;
            char w[24];
            int wl = j - i;
            if (wl < 24) {
                for (int k = 0; k < wl; k++) w[k] = s[i + k];
                w[wl] = 0;
                uint32_t col = AL_COL_TEXT;
                if (syn == SY_C && al_in_list(w, c_kws))
                    col = SY_COL_KW;
                else if (syn == SY_ASM && al_in_list(w, asm_insns))
                    col = SY_COL_INSN;
                else if (syn == SY_ASM && al_in_list(w, asm_regs))
                    col = SY_COL_REG;
                else if (syn == SY_SH && al_in_list(w, sh_kws))
                    col = SY_COL_KW;
                al_str_clip(x + i * 6, y, w, wl, col);
            }
            i = j;
            continue;
        }
        al_char(x + i * 6, y, s[i], AL_COL_TEXT);
        i++;
    }
}

/* ══════════════ editor buffer ══════════════ */
static int al_line_len(int ln)
{
    if (ln < 0 || ln >= ed_nlines) return 0;
    return al_strlen(ed_lines[ln]);
}

static void al_new_buffer(void)
{
    ed_lines[0][0] = 0;
    ed_nlines = 1;
    cur_line = cur_col = top_line = left_col = 0;
    ed_dirty = 0;
}

void altr_load(const char *path)
{
    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0) {
        /* new file: empty buffer */
        musr_strncpy(ed_path, path, sizeof(ed_path) - 1);
        al_new_buffer();
        return;
    }
    static char fbuf[96 * 1024];
    int n = musr_sc_read(fd, fbuf, sizeof(fbuf) - 1);
    musr_sc_close(fd);
    if (n < 0) n = 0;
    fbuf[n] = 0;

    ed_nlines = 0;
    int col = 0;
    for (int i = 0; i < n && ed_nlines < AL_MAX_LINES; i++) {
        if (fbuf[i] == '\n') {
            ed_lines[ed_nlines][col] = 0;
            ed_nlines++;
            col = 0;
        } else if (fbuf[i] != '\r' && col < AL_MAX_LINELEN - 1) {
            ed_lines[ed_nlines][col++] = fbuf[i];
        }
    }
    if (col > 0 && ed_nlines < AL_MAX_LINES) {
        ed_lines[ed_nlines][col] = 0;
        ed_nlines++;
    }
    if (ed_nlines == 0)
        ed_nlines = 1;
    musr_strncpy(ed_path, path, sizeof(ed_path) - 1);
    cur_line = cur_col = top_line = left_col = 0;
    ed_dirty = 0;
}

static int altr_save(const char *path)
{
    int fd = musr_sc_open(path, O_WRONLY | O_CREAT);
    if (fd < 0)
        return -1;
    for (int i = 0; i < ed_nlines; i++) {
        int l = al_line_len(i);
        if (l > 0)
            musr_sc_write(fd, ed_lines[i], l);
        if (musr_sc_write(fd, "\n", 1) != 1) {
            musr_sc_close(fd);
            return -1;
        }
    }
    musr_sc_close(fd);
    ed_dirty = 0;
    return 0;
}

/* ══════════════ sead-style substitute ══════════════ */
/*
 * sead_subst_line - implement the 's/pat/rep/g' core of sead on one
 * line: literal pattern match (no regex), replacement as text.
 * This is the altr embed of the sead engine core (the full sead
 * stream tool lives in /bin/sead inside m4sh).
 */
static int sead_match_at(const char *line, int pos, const char *pat)
{
    for (int i = 0; pat[i]; i++)
        if (!line[pos + i] || line[pos + i] != pat[i])
            return 0;
    return 1;
}

static int sead_subst_line(char *line, int maxlen, const char *pat,
                           const char *rep, int global)
{
    int plen = al_strlen(pat);
    int rlen = al_strlen(rep);
    int changed = 0;
    int pos = 0;
    if (plen == 0) return 0;
    while (line[pos]) {
        if (sead_match_at(line, pos, pat)) {
            /* build the new tail: rep + rest-after-match */
            char tail[AL_MAX_LINELEN];
            int ti = 0;
            for (int i = pos + plen; line[i] && ti < (int)sizeof(tail) - 1; i++)
                tail[ti++] = line[i];
            tail[ti] = 0;
            int o = pos;
            for (int i = 0; rep[i] && o < maxlen - 1; i++)
                line[o++] = rep[i];
            for (int i = 0; tail[i] && o < maxlen - 1; i++)
                line[o++] = tail[i];
            line[o] = 0;
            changed = 1;
            pos += rlen;
            if (!global) break;
        } else {
            pos++;
        }
    }
    return changed;
}

/* ══════════════ tab completion ══════════════ */
/*
 * al_tab_complete - complete the last path component of buf (a
 * directory path being typed in :e / :w).  On a unique prefix,
 * extend it in-place; on multiple candidates, complete to the
 * longest common prefix.  Directories get a trailing '/'.
 */
static void al_tab_complete(char *buf, int bufsz)
{
    int len = al_strlen(buf);
    int slash = -1;
    for (int i = len - 1; i >= 0; i--)
        if (buf[i] == '/') { slash = i; break; }
    char dir[160], pfx[64];
    if (slash >= 0) {
        if (slash == 0) { dir[0] = '/'; dir[1] = 0; }
        else {
            for (int i = 0; i < slash; i++) dir[i] = buf[i];
            dir[slash] = 0;
        }
        musr_strncpy(pfx, buf + slash + 1, sizeof(pfx) - 1);
        pfx[len - slash - 1] = 0;
    } else {
        musr_strncpy(dir, tree_cwd, sizeof(dir) - 1);
        musr_strncpy(pfx, buf, sizeof(pfx) - 1);
    }

    /* collect matching names */
    static struct dirent ents[16];
    char cand[8][DIRENT_NAME_MAX];
    int cand_dir[8];
    int nc = 0;
    int pfxlen = al_strlen(pfx);
    int fd = musr_sc_open(dir, O_RDONLY);
    if (fd < 0) return;
    for (;;) {
        int n = musr_sc_getdents(fd, ents, 16);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (nc >= 8) break;
            char *nm = ents[i].name;
            if (nm[0] == '.') continue;
            int match = 1;
            for (int k = 0; k < pfxlen; k++)
                if (nm[k] != pfx[k]) { match = 0; break; }
            if (!match) continue;
            musr_strncpy(cand[nc], nm, DIRENT_NAME_MAX - 1);
            cand_dir[nc] = (ents[i].type == 2);
            nc++;
        }
        if (nc >= 8) break;
    }
    musr_sc_close(fd);
    if (nc == 0) return;

    /* longest common prefix */
    int lcp = al_strlen(cand[0]);
    for (int i = 1; i < nc; i++) {
        int l = 0;
        while (cand[i][l] && cand[0][l] && cand[i][l] == cand[0][l])
            l++;
        if (l < lcp) lcp = l;
    }

    /* rewrite the path tail: dir + '/' + cand-prefix */
    int dl = al_strlen(dir);
    int o = 0;
    if (slash >= 0 && slash > 0) {
        for (int i = 0; i <= slash && o < bufsz - 1; i++)
            buf[o++] = buf[i];
    } else if (slash == 0) {
        buf[o++] = '/';
    } else {
        for (int i = 0; dir[i] && o < bufsz - 1; i++)
            buf[o++] = dir[i];
        if (o < bufsz - 1 && buf[o - 1] != '/')
            buf[o++] = '/';
    }
    for (int i = 0; i < lcp && o < bufsz - 1; i++)
        buf[o++] = cand[0][i];
    if (nc == 1 && cand_dir[0] && o < bufsz - 1)
        buf[o++] = '/';
    buf[o] = 0;

    /* replace cmdline tail for display (cmdline mirrors buf) */
    (void)dl;
}

/* ══════════════ search ══════════════ */
static int al_find(int from_line, int from_col, const char *pat,
                   int *mline, int *mcol)
{
    int plen = al_strlen(pat);
    if (plen == 0) return 0;
    for (int ln = from_line; ln < ed_nlines; ln++) {
        int start = (ln == from_line) ? from_col : 0;
        char *s = ed_lines[ln];
        for (int c = start; s[c]; c++) {
            if (sead_match_at(s, c, pat)) {
                *mline = ln;
                *mcol = c;
                return 1;
            }
        }
    }
    return 0;
}

/* ══════════════ command line ══════════════ */
static char status_msg[80] = "";

static void al_status(const char *msg)
{
    musr_strncpy(status_msg, msg, sizeof(status_msg) - 1);
}

static void al_exec_cmd(void)
{
    char *c = cmdline;

    if (cmdline_pfx == '/') {
        /* search: store pattern, jump to first match after cursor */
        musr_strncpy(search_pat, c, sizeof(search_pat) - 1);
        int ml, mc;
        if (al_find(cur_line, cur_col + 1, search_pat, &ml, &mc)) {
            cur_line = ml;
            cur_col = mc;
            al_status("search hit");
        } else if (al_find(0, 0, search_pat, &ml, &mc)) {
            cur_line = ml;
            cur_col = mc;
            al_status("search wrapped");
        } else {
            al_status("pattern not found");
        }
        return;
    }

    /* ':' commands */
    if (al_streq(c, "wq") || al_streq(c, "x")) {
        if (ed_path[0]) {
            if (altr_save(ed_path) == 0) {
                al_status("written");
                m4k_exit(0);
            }
            al_status("write FAILED");
        }
        return;
    }
    if (al_streq(c, "q!")) {
        m4k_exit(0);
        return;
    }
    if (al_streq(c, "q")) {
        if (ed_dirty) al_status("no write since last change (:q!)");
        else m4k_exit(0);
        return;
    }
    if (al_streq(c, "w")) {
        if (!ed_path[0]) { al_status("no file name"); return; }
        if (altr_save(ed_path) == 0) al_status("written");
        else al_status("write FAILED");
        return;
    }
    if (al_starts(c, "w ")) {
        char path[160];
        musr_strncpy(path, c + 2, sizeof(path) - 1);
        if (altr_save(path) == 0) {
            musr_strncpy(ed_path, path, sizeof(ed_path) - 1);
            al_status("written");
        } else al_status("write FAILED");
        return;
    }
    if (al_starts(c, "e ")) {
        char path[160];
        musr_strncpy(path, c + 2, sizeof(path) - 1);
        altr_load(path);
        al_status("loaded");
        return;
    }
    /* :s/pat/rep[/g] — sead substitute on the current line */
    if (al_starts(c, "s/") && al_strlen(c) >= 5) {
        char pat[48], rep[48];
        int g = al_endswith(c, "g");
        int end = al_strlen(c) - (g ? 1 : 0);
        /* parse pat/rep between the slashes */
        int i = 2;
        int pi = 0;
        while (i < end && c[i] != '/' && pi < 47) pat[pi++] = c[i++];
        pat[pi] = 0;
        if (c[i] == '/') i++;
        int ri = 0;
        while (i < end && c[i] != '/' && ri < 47) rep[ri++] = c[i++];
        rep[ri] = 0;
        if (sead_subst_line(ed_lines[cur_line], AL_MAX_LINELEN,
                            pat, rep, g))
            al_status("substituted");
        else
            al_status("no match");
        return;
    }
    /* :%s/pat/rep[/g] — sead substitute over the whole file */
    if (al_starts(c, "%s/") && al_strlen(c) >= 6) {
        char pat[48], rep[48];
        int g = al_endswith(c, "g");
        int end = al_strlen(c) - (g ? 1 : 0);
        int i = 3;
        int pi = 0;
        while (i < end && c[i] != '/' && pi < 47) pat[pi++] = c[i++];
        pat[pi] = 0;
        if (c[i] == '/') i++;
        int ri = 0;
        while (i < end && c[i] != '/' && ri < 47) rep[ri++] = c[i++];
        rep[ri] = 0;
        int nsub = 0;
        for (int ln = 0; ln < ed_nlines; ln++)
            if (sead_subst_line(ed_lines[ln], AL_MAX_LINELEN,
                                pat, rep, g))
                nsub++;
        char msg[48];
        char num[12];
        al_itoa(nsub, num);
        int o = 0;
        const char *t = "sead: ";
        while (t[o] && o < 40) { msg[o] = t[o]; o++; }
        for (int k = 0; num[k] && o < 44; k++) msg[o++] = num[k];
        t = " subst";
        for (int k = 0; t[k] && o < 47; k++) msg[o++] = t[k];
        msg[o] = 0;
        al_status(msg);
        return;
    }
    /* :sead <expr> — run the sead engine over the buffer?  m4sh's
     * full sead is a shell command; here we support the common
     * :sead s/pat/rep/g form by delegating to the :%s path. */
    if (al_starts(c, "sead ") || al_streq(c, "sead")) {
        if (c[4] == ' ') {
            al_memmove(c, c + 4, al_strlen(c + 4) + 1);
            /* now starts with " s/.." → shift left */
            al_memmove(c, c + 1, al_strlen(c + 1) + 1);
            al_exec_cmd();   /* re-dispatch (now :%s or :s form) */
            return;
        }
        al_status("usage: sead s/pat/rep/g");
        return;
    }
    if (al_streq(c, "wq!")) {
        if (ed_path[0]) altr_save(ed_path);
        m4k_exit(0);
        return;
    }
    al_status("unknown command");
}

/* ══════════════ key handling ══════════════ */
static void al_scroll(void)
{
    if (cur_line < top_line) top_line = cur_line;
    if (cur_line >= top_line + AL_ROWS)
        top_line = cur_line - AL_ROWS + 1;
    if (cur_col < left_col) left_col = cur_col;
    if (cur_col >= left_col + AL_COLS)
        left_col = cur_col - AL_COLS + 1;
    if (top_line < 0) top_line = 0;
    if (left_col < 0) left_col = 0;
}

static void al_key_insert(unsigned char ch)
{
    if (ch == 0x1B) {
        mode_insert = 0;
        return;
    }
    char *line = ed_lines[cur_line];
    int len = al_strlen(line);

    if (ch == '\b' || ch == 0x7F) {
        if (cur_col > 0) {
            for (int i = cur_col - 1; i < len; i++)
                line[i] = line[i + 1];
            cur_col--;
            ed_dirty = 1;
        } else if (cur_line > 0) {
            /* join with previous line */
            int plen = al_line_len(cur_line - 1);
            if (plen + len < AL_MAX_LINELEN - 1) {
                char *prev = ed_lines[cur_line - 1];
                for (int i = 0; i <= len; i++)
                    prev[plen + i] = line[i];
                for (int i = cur_line; i < ed_nlines - 1; i++)
                    musr_strncpy(ed_lines[i], ed_lines[i + 1],
                                 AL_MAX_LINELEN - 1);
                ed_nlines--;
                cur_line--;
                cur_col = plen;
                ed_dirty = 1;
            }
        }
        return;
    }
    if (ch == '\r' || ch == '\n') {
        if (ed_nlines >= AL_MAX_LINES) return;
        for (int i = ed_nlines; i > cur_line + 1; i--)
            musr_strncpy(ed_lines[i], ed_lines[i - 1],
                         AL_MAX_LINELEN - 1);
        ed_nlines++;
        char *rest = ed_lines[cur_line + 1];
        for (int i = 0; i <= len - cur_col; i++)
            rest[i] = line[cur_col + i];
        line[cur_col] = 0;
        cur_line++;
        cur_col = 0;
        ed_dirty = 1;
        return;
    }
    if (ch == '\t') {
        for (int k = 0; k < 4 && len < AL_MAX_LINELEN - 1; k++) {
            for (int i = len; i > cur_col; i--)
                line[i] = line[i - 1];
            line[cur_col++] = ' ';
            len++;
        }
        ed_dirty = 1;
        return;
    }
    if (ch >= 0x20 && ch < 0x7F && len < AL_MAX_LINELEN - 1) {
        for (int i = len; i > cur_col; i--)
            line[i] = line[i - 1];
        line[cur_col++] = ch;
        line[len + 1] = 0;
        ed_dirty = 1;
    }
}

static void al_key_normal(unsigned char ch)
{
    /* pending operators (d for dd) */
    static int pend_d = 0;

    if (pend_d) {
        pend_d = 0;
        if (ch == 'd' && ed_nlines > 1) {
            for (int i = cur_line; i < ed_nlines - 1; i++)
                musr_strncpy(ed_lines[i], ed_lines[i + 1],
                             AL_MAX_LINELEN - 1);
            ed_nlines--;
            ed_dirty = 1;
        } else if (ch == 'd') {
            ed_lines[0][0] = 0;
            ed_dirty = 1;
        }
        if (cur_line >= ed_nlines) cur_line = ed_nlines - 1;
        return;
    }

    switch (ch) {
    case 'h':
        if (tree_active) { tree_active = 0; break; }
        if (cur_col > 0) cur_col--;
        break;
    case 'l':
        if (!tree_active && cur_col < al_line_len(cur_line))
            cur_col++;
        break;
    case 'j':
        if (tree_active) {
            if (tree_sel < tree_count - 1) tree_sel++;
        } else if (cur_line < ed_nlines - 1) {
            cur_line++;
            if (cur_col > al_line_len(cur_line))
                cur_col = al_line_len(cur_line);
        }
        break;
    case 'k':
        if (tree_active) {
            if (tree_sel > 0) tree_sel--;
        } else if (cur_line > 0) {
            cur_line--;
            if (cur_col > al_line_len(cur_line))
                cur_col = al_line_len(cur_line);
        }
        break;
    case 'g':
        /* gg → top (second g) */
        {
            static int pend_g = 0;
            if (pend_g) { cur_line = 0; cur_col = 0; pend_g = 0; }
            else pend_g = 1;
        }
        break;
    case 'G':
        cur_line = ed_nlines - 1;
        cur_col = 0;
        break;
    case 'i':
        if (!tree_active) mode_insert = 1;
        break;
    case 'a':
        if (!tree_active && cur_col < al_line_len(cur_line))
            cur_col++;
        mode_insert = 1;
        break;
    case 'o':
    case 'O': {
        if (tree_active) break;
        if (ed_nlines >= AL_MAX_LINES) break;
        int at = (ch == 'o') ? cur_line + 1 : cur_line;
        for (int i = ed_nlines; i > at; i--)
            musr_strncpy(ed_lines[i], ed_lines[i - 1],
                         AL_MAX_LINELEN - 1);
        ed_lines[at][0] = 0;
        ed_nlines++;
        cur_line = at;
        cur_col = 0;
        mode_insert = 1;
        ed_dirty = 1;
        break;
    }
    case 'x': {
        if (tree_active) break;
        char *line = ed_lines[cur_line];
        int len = al_strlen(line);
        if (cur_col < len) {
            for (int i = cur_col; i < len; i++)
                line[i] = line[i + 1];
            ed_dirty = 1;
        }
        break;
    }
    case 'd':
        pend_d = 1;
        break;
    case '0':
        cur_col = 0;
        break;
    case '$':
        cur_col = al_line_len(cur_line);
        if (cur_col > 0) cur_col--;
        break;
    case '\r':
    case '\n':
        if (tree_active)
            al_tree_open();
        break;
    case 'f':
        /* f = focus tree (toggle back with h/l) */
        tree_active = 1;
        break;
    case '/':
    case ':':
        cmdline_on = 1;
        cmdline_pfx = ch;
        cmdline_len = 0;
        cmdline[0] = 0;
        tab_pending = 0;
        break;
    case 'n': {
        int ml, mc;
        if (search_pat[0] &&
            al_find(cur_line, cur_col + 1, search_pat, &ml, &mc)) {
            cur_line = ml; cur_col = mc;
        } else if (search_pat[0] &&
                   al_find(0, 0, search_pat, &ml, &mc)) {
            cur_line = ml; cur_col = mc;
        }
        break;
    }
    case 'N': {
        /* backward search: scan up */
        int plen = al_strlen(search_pat);
        if (!plen) break;
        int found = 0;
        for (int ln = cur_line; ln >= 0 && !found; ln--) {
            int startc = al_line_len(ln) - plen;
            if (ln == cur_line && startc >= cur_col) startc = cur_col - 1;
            if (startc < 0) continue;
            for (int c2 = startc; c2 >= 0; c2--) {
                if (sead_match_at(ed_lines[ln], c2, search_pat)) {
                    cur_line = ln; cur_col = c2;
                    found = 1;
                    break;
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

static void al_key(unsigned char ch)
{
    if (cmdline_on) {
        if (ch == '\r' || ch == '\n') {
            cmdline_on = 0;
            al_exec_cmd();
        } else if (ch == 0x1B) {
            cmdline_on = 0;
        } else if (ch == '\b' || ch == 0x7F) {
            if (cmdline_len > 0)
                cmdline[--cmdline_len] = 0;
        } else if (ch == '\t') {
            if (cmdline_pfx == ':' &&
                (al_starts(cmdline, "e ") || al_starts(cmdline, "w "))) {
                char path[160];
                musr_strncpy(path, cmdline + 2, sizeof(path) - 1);
                al_tab_complete(path, sizeof(path));
                /* rebuild cmdline */
                cmdline[0] = 'e'; cmdline[1] = ' ';
                musr_strncpy(cmdline + 2, path, sizeof(cmdline) - 3);
                cmdline_len = al_strlen(cmdline);
            }
        } else if (ch >= 0x20 && ch < 0x7F &&
                   cmdline_len < (int)sizeof(cmdline) - 1) {
            cmdline[cmdline_len++] = ch;
            cmdline[cmdline_len] = 0;
        }
        return;
    }
    if (mode_insert)
        al_key_insert(ch);
    else
        al_key_normal(ch);
    al_scroll();
}

/* ══════════════ rendering ══════════════ */
static void al_render(struct copland_shm *shm, int slot)
{
    /* title */
    al_rect(0, 0, AL_W, AL_H, AL_COL_BODY);
    al_rect(0, 0, AL_W, AL_TITLE_H, AL_COL_TITLE);
    {
        char t[128];
        int o = 0;
        const char *s = "altr - ";
        while (s[o]) { t[o] = s[o]; o++; }
        const char *p = ed_path[0] ? ed_path : "[No Name]";
        for (int i = 0; p[i] && o < 120; i++) t[o++] = p[i];
        if (ed_dirty && o < 126) t[o++] = '+';
        t[o] = 0;
        al_str(6, 5, t, 0x00FFFFFF);
    }

    /* tree panel */
    al_rect(0, AL_TITLE_H, AL_TREE_W, AL_H - AL_TITLE_H, AL_COL_TREEBG);
    al_str(4, AL_TITLE_H + 4, "FILES", 0x0080A0FF);
    al_rect(0, AL_TITLE_H + 14, AL_TREE_W, 1, 0x00404050);
    int trows = (AL_H - AL_TITLE_H - 22 - 6) / 10;
    for (int i = 0; i < tree_count && i < trows; i++) {
        int y = AL_TITLE_H + 22 + i * 10;
        int sel = (i == tree_sel);
        if (sel)
            al_rect(0, y - 1, AL_TREE_W, 10,
                    tree_active ? AL_COL_SEL : 0x00204070);
        char nm[DIRENT_NAME_MAX + 2];
        int o = 0;
        if (tree_ents[i].is_dir) { nm[o++] = '['; }
        const char *s = tree_ents[i].name;
        for (int k = 0; s[k] && o < DIRENT_NAME_MAX; k++)
            nm[o++] = s[k];
        if (tree_ents[i].is_dir) nm[o++] = ']';
        nm[o] = 0;
        al_str_clip(6, y, nm, (AL_TREE_W - 12) / 6,
                    tree_ents[i].is_dir ? 0x0060C0FF : 0x00C0C0C0);
    }
    al_rect(AL_TREE_W, AL_TITLE_H, 1, AL_H - AL_TITLE_H, 0x00404050);

    /* edit area */
    enum al_syn syn = al_syn_of(ed_path);
    for (int r = 0; r < AL_ROWS; r++) {
        int ln = top_line + r;
        if (ln >= ed_nlines) break;
        int y = AL_TEXT_Y + r * AL_CH_H;
        /* line number gutter */
        char num[8];
        al_itoa(ln + 1, num);
        al_str(AL_TEXT_X, y, num, 0x00606060);
        al_draw_line(AL_TEXT_X + 4 * 6, y,
                     ed_lines[ln] + left_col, AL_COLS - 4, syn);
        /* cursor */
        if (ln == cur_line) {
            int cx = AL_TEXT_X + (cur_col - left_col + 4) * 6;
            if (mode_insert)
                al_rect(cx, y + 8, 5, 1, AL_COL_CURSOR);
            else
                al_rect(cx, y, 5, 7, 0x00FFFFFF);
        }
    }

    /* status + cmdline */
    int sy = AL_H - AL_STATUS_H - AL_CMD_H - 2;
    if (cmdline_on) {
        al_str(4, sy + AL_STATUS_H + 1, cmdline_pfx == ':' ? ":" : "/",
               0x00FFFFFF);
        al_str(4 + 6, sy + AL_STATUS_H + 1, cmdline, 0x00FFFFFF);
    } else if (status_msg[0]) {
        al_str(4, sy + AL_STATUS_H + 1, status_msg, 0x00E0E060);
    }
    char st[96];
    int o = 0;
    const char *m = mode_insert ? "-- INSERT --" : (
        tree_active ? "-- FILES --" : "");
    while (m[o]) { st[o] = m[o]; o++; }
    const char *pos = "  line ";
    for (int i = 0; pos[i] && o < 90; i++) st[o++] = pos[i];
    char num[12];
    al_itoa(cur_line + 1, num);
    for (int i = 0; num[i] && o < 90; i++) st[o++] = num[i];
    st[o] = 0;
    al_str(4, sy, st, AL_COL_STATUS);

    /* publish */
    shm->surfaces[slot].buffer_ptr = (uint32_t)(uintptr_t)al_buf;
    shm->surfaces[slot].dmg_x = shm->surfaces[slot].x;
    shm->surfaces[slot].dmg_y = shm->surfaces[slot].y;
    shm->surfaces[slot].dmg_w = shm->surfaces[slot].w;
    shm->surfaces[slot].dmg_h = shm->surfaces[slot].h;
    shm->dirty = 1;
}

/* ══════════════ main ══════════════ */
void _start(void)
{
    ser_puts("[ALTR] editor starting\n");

    struct copland_shm *shm = copland_shm_get();
    if (!shm || shm->magic != COPLAND_SHM_MAGIC) {
        ser_puts("[ALTR] copland not ready\n");
        m4k_exit(1);
    }

    /* Register our key mailbox */
    al_mb->magic = AL_MAILBOX_MAGIC;
    al_mb->write_idx = 0;
    al_mb->read_idx = 0;

    /* Initial content: scratch buffer */
    al_new_buffer();
    al_tree_reload();

    /* Create our surface */
    if (copland_cmd_push(shm, COPLAND_CMD_CREATE_SURFACE,
                         80, 40, AL_W, AL_H, (int32_t)AL_COL_TITLE,
                         COPLAND_SURF_VISIBLE) != 0) {
        ser_puts("[ALTR] cmd ring full\n");
        m4k_exit(1);
    }

    /* Wait for Copland to allocate our slot (640-wide surface) */
    int guard = 0;
    int my_slot = -1;
    while (guard++ < 200000) {
        m4k_yield();
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (shm->surfaces[i].in_use &&
                !shm->surfaces[i].buffer_ptr &&
                shm->surfaces[i].w == AL_W) {
                my_slot = i;
                break;
            }
        if (my_slot >= 0) break;
    }
    if (my_slot < 0) {
        ser_puts("[ALTR] surface allocation timeout\n");
        m4k_exit(1);
    }
    shm->surfaces[my_slot].buffer_ptr = (uint32_t)(uintptr_t)al_buf;
    ser_puts("[ALTR] surface ready\n");

    int need_render = 1;
    for (;;) {
        if (!(shm->surfaces[my_slot].flags & COPLAND_SURF_VISIBLE))
            m4k_exit(0);

        while (al_mb->read_idx != al_mb->write_idx) {
            unsigned char ch = al_mb->buf[al_mb->read_idx];
            al_mb->read_idx =
                (al_mb->read_idx + 1) % AL_MAILBOX_SIZE;
            al_key(ch);
            need_render = 1;
        }

        if (need_render) {
            al_render(shm, my_slot);
            need_render = 0;
        }
        m4k_yield();
    }
}
