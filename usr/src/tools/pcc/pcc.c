/*
 * M4KK1 4P1 - pcc.c
 * Description: M4KK1 self-hosted C compiler (pcc subset, i386 ELF output)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * This is the compiler that runs INSIDE M4KK1 (installed as /bin/pcc
 * and /bin/cc by the kernel at boot).  It compiles a small C subset
 * into a static i386 ELF32 executable for M4KK1, self-linking the
 * program against a tiny embedded runtime (crt0 + putchar/puts/putstr/
 * putdec/puthex via the M4KK1 syscall ABI).
 *
 * Build for M4KK1:   i386-pc-m4kk1-pcc (freestanding + m4k_libc)
 * Build for host:    gcc -DPCC_HOST (Linux test harness: the generated
 *                    ELFs use the int 0x80 Linux ABI and run directly
 *                    on the host, so pcc.c is verifiable without QEMU).
 *
 * Supported C subset:
 *   - types: int, char, void, pointers, arrays (int/char), strings
 *   - qualifiers const/static/unsigned/register/volatile/long/short
 *     are accepted and ignored
 *   - statements: { }, return, if/else, while, for, break, continue
 *   - expressions: full precedence, assignment, calls (<=6 args),
 *     indexing, & and *; no ++/--, struct/union/switch/goto/float
 *   - globals: scalar init, brace lists, "string" inits, bss
 *
 * Codegen model: lvalue expressions leave their ADDRESS in EAX with
 * is_lvalue set; every binary level "coerces" its operands to values
 * (loading lvalues); assign() stores through the lvalue address.
 */

#define PCC_VERSION "1.2.0.DEVEL"
#define PCC_TARGET  "i386-pc-m4kk1"

/* Load address of generated executables (free zone: m4sh at 0x800000,
 * sprach at 0x900000). */
#define PCC_LOAD_BASE 0x00A00000u

#ifdef PCC_HOST
/* ── Host (Linux) test build ── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#else
/* ── M4KK1 build (m4k_libc) ── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#endif

/* System call ABI: BOTH builds use the int 0x80 ABI, which the M4KK1
 * kernel implements for every user process (console I/O on fd 1, full
 * VFS file I/O on real fds).  The same numbers happen to be the Linux
 * ABI, so generated programs also run directly on the host. */
#define PCC_SYS_INT   0x80
#define PCC_SYS_WRITE 4
#define PCC_SYS_READ  3
#define PCC_SYS_EXIT  1
#define PCC_SYS_OPEN  5
#define PCC_SYS_CLOSE 6

#ifndef PCC_HOST
#define PCC_SYS_TIME   0x1E
#define PCC_SYS_UPTIME 0x8C
static unsigned int pcc_mtime(void)
{
    unsigned int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(PCC_SYS_TIME));
    return r;
}
static unsigned int pcc_muptime(void)
{
    unsigned int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(PCC_SYS_UPTIME));
    return r;
}
/* File I/O for the M4KK1 build: the int 0x4D ABI has no open/close
 * handlers, so pcc talks to the VFS through the int 0x80 ABI directly
 * instead of the libc stdio wrappers (which use the 0x4D ABI). */
static int pcc_io(unsigned int n, unsigned int a, unsigned int b,
                  unsigned int c)
{
    unsigned int r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return (int)r;
}
#define PCC_FOPEN_R(path) pcc_io(PCC_SYS_OPEN, (unsigned int)(path), 0x01, 0)
#define PCC_FOPEN_W(path) \
    pcc_io(PCC_SYS_OPEN, (unsigned int)(path), 0x02 | 0x0100 | 0x1000, 0)
#define PCC_FREAD(f, b, n) \
    pcc_io(PCC_SYS_READ, (unsigned int)(f), (unsigned int)(b), (unsigned int)(n))
#define PCC_FWRITE(f, b, n) \
    pcc_io(PCC_SYS_WRITE, (unsigned int)(f), (unsigned int)(b), (unsigned int)(n))
#define PCC_FCLOSE(f) pcc_io(PCC_SYS_CLOSE, (unsigned int)(f), 0, 0)
#define PCC_FBAD(f)   ((f) < 0)
#define PCC_FILE int
#else
#define PCC_FILE FILE *
#define PCC_FOPEN_R(path) fopen(path, "rb")
#define PCC_FOPEN_W(path) fopen(path, "wb")
#define PCC_FREAD(f, b, n)  (int)fread(b, 1, (size_t)(n), f)
#define PCC_FWRITE(f, b, n) (int)fwrite(b, 1, (size_t)(n), f)
#define PCC_FCLOSE(f)       fclose(f)
#define PCC_FBAD(f)         (!(f))
#endif

/* ── Fixed-size working areas (BSS; the M4KK1 tool is stack-light) ── */

#define TOKEN_MAX   65536
#define SYM_MAX     512
#define LABEL_MAX   512
#define FIXUP_MAX   16384
#define TEXT_MAX    (512 * 1024)
#define RODATA_MAX  (128 * 1024)
#define DATA_MAX    (128 * 1024)
#define SRC_MAX     (512 * 1024)
#define OUT_MAX     (768 * 1024)
#define CMD_MAX     2048
#define STRPOOL_MAX (128 * 1024)

/* ── Tokenizer ── */

enum {
    TK_IDENT = 256, TK_NUM, TK_STR, TK_CHAR,
    TK_RETURN, TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_BREAK, TK_CONTINUE,
    TK_INT, TK_TCHAR, TK_VOID,
    TK_SHL, TK_SHR, TK_LE, TK_GE, TK_EQ, TK_NE, TK_LAND, TK_LOR,
    TK_EOF
};

struct token {
    int kind;
    int val;          /* TK_NUM/TK_CHAR value; TK_STR: string pool index */
    int line;
};

static struct token toks[TOKEN_MAX];
static int tok_n;
static char str_pool[STRPOOL_MAX];
static int str_pool_n;
static int cur_line = 1;

static int is_id_start(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int is_id_char(int c)
{
    return is_id_start(c) || (c >= '0' && c <= '9');
}

static void tok_push(int kind, int val)
{
    if (tok_n >= TOKEN_MAX)
        return;
    toks[tok_n].kind = kind;
    toks[tok_n].val = val;
    toks[tok_n].line = cur_line;
    tok_n++;
}

static void tokenize(const char *src)
{
    const char *p = src;
    tok_n = 0;
    cur_line = 1;

    while (*p) {
        if (*p == '\n') { cur_line++; p++; continue; }
        if (*p == ' ' || *p == '\t' || *p == '\r') { p++; continue; }

        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') cur_line++;
                p++;
            }
            if (*p) p += 2;
            continue;
        }

        if (is_id_start(*p)) {
            const char *s = p;
            int id = -1;
            while (is_id_char(*p)) p++;
            if (p - s == 6 && strncmp(s, "return", 6) == 0) id = TK_RETURN;
            else if (p - s == 2 && strncmp(s, "if", 2) == 0) id = TK_IF;
            else if (p - s == 4 && strncmp(s, "else", 4) == 0) id = TK_ELSE;
            else if (p - s == 5 && strncmp(s, "while", 5) == 0) id = TK_WHILE;
            else if (p - s == 3 && strncmp(s, "for", 3) == 0) id = TK_FOR;
            else if (p - s == 5 && strncmp(s, "break", 5) == 0) id = TK_BREAK;
            else if (p - s == 8 && strncmp(s, "continue", 8) == 0) id = TK_CONTINUE;
            else if (p - s == 3 && strncmp(s, "int", 3) == 0) id = TK_INT;
            else if (p - s == 4 && strncmp(s, "char", 4) == 0) id = TK_TCHAR;
            else if (p - s == 4 && strncmp(s, "void", 4) == 0) id = TK_VOID;
            if (id >= 0) {
                tok_push(id, 0);
            } else {
                int off = str_pool_n;
                int len = (int)(p - s);
                if (len > 31) len = 31;
                memcpy(str_pool + off, s, len);
                str_pool[off + len] = '\0';
                str_pool_n = off + len + 1;
                tok_push(TK_IDENT, off);
            }
            continue;
        }

        if (*p >= '0' && *p <= '9') {
            int v = 0;
            if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                while ((*p >= '0' && *p <= '9') ||
                       (*p >= 'a' && *p <= 'f') ||
                       (*p >= 'A' && *p <= 'F')) {
                    int d = *p;
                    if (d >= '0' && d <= '9') d -= '0';
                    else if (d >= 'a' && d <= 'f') d = d - 'a' + 10;
                    else d = d - 'A' + 10;
                    v = v * 16 + d;
                    p++;
                }
            } else {
                while (*p >= '0' && *p <= '9') {
                    v = v * 10 + (*p - '0');
                    p++;
                }
            }
            tok_push(TK_NUM, v);
            continue;
        }

        if (*p == '"') {
            p++;
            int off = str_pool_n;
            while (*p && *p != '"') {
                int c = *p++;
                if (c == '\\' && *p) {
                    c = *p++;
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == '\\') c = '\\';
                    else if (c == '"') c = '"';
                    else if (c == '\'') c = '\'';
                    else if (c == '0') c = 0;
                }
                str_pool[str_pool_n++] = (char)c;
            }
            if (*p == '"') p++;
            str_pool[str_pool_n++] = '\0';
            tok_push(TK_STR, off);
            continue;
        }

        if (*p == '\'') {
            p++;
            int c;
            if (*p == '\\' && p[1]) {
                p++;
                c = *p++;
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c == 'r') c = '\r';
                else if (c == '\\') c = '\\';
                else if (c == '\'') c = '\'';
                else if (c == '0') c = 0;
            } else {
                c = *p;
                p++;
            }
            if (*p == '\'') p++;
            tok_push(TK_CHAR, c);
            continue;
        }

        if (!strncmp(p, "&&", 2)) { tok_push(TK_LAND, 0); p += 2; continue; }
        if (!strncmp(p, "||", 2)) { tok_push(TK_LOR, 0); p += 2; continue; }
        if (!strncmp(p, "<<", 2)) { tok_push(TK_SHL, 0); p += 2; continue; }
        if (!strncmp(p, ">>", 2)) { tok_push(TK_SHR, 0); p += 2; continue; }
        if (!strncmp(p, "<=", 2)) { tok_push(TK_LE, 0); p += 2; continue; }
        if (!strncmp(p, ">=", 2)) { tok_push(TK_GE, 0); p += 2; continue; }
        if (!strncmp(p, "==", 2)) { tok_push(TK_EQ, 0); p += 2; continue; }
        if (!strncmp(p, "!=", 2)) { tok_push(TK_NE, 0); p += 2; continue; }
        if (!strncmp(p, "<<=", 3) || !strncmp(p, ">>=", 3)) { p += 3; continue; }
        if (p[1] == '=' && strchr("+-*/%&|^", *p)) { p += 2; continue; }
        if (strchr("(){}[];,=+-*/%&|^!~<>?", *p)) {
            tok_push((unsigned char)*p, 0);
            p++;
            continue;
        }
        p++;
    }
    tok_push(TK_EOF, 0);
}

/* ── Symbols ── */

enum { TY_INT = 0, TY_CHAR, TY_VOID };

struct sym {
    char name[32];
    int kind;          /* 1=func, 2=global, 3=local/param, 4=runtime stub,
                          5=string literal, 0=unresolved */
    int type;          /* TY_* of the value / pointee base */
    int is_ptr;
    int is_array;
    int arr_len;
    int size;          /* total bytes */
    int defined;
    uint32_t addr;     /* global: data offset or BSS marker 0xFFFFFFFF;
                          local: ebp offset; stub: text offset;
                          string: rodata offset */
    int text_off;      /* function: text offset */
};

static struct sym syms[SYM_MAX];
static int sym_n;

static struct sym *find_sym(const char *name)
{
    int i;
    for (i = sym_n - 1; i >= 0; i--)
        if (strcmp(syms[i].name, name) == 0)
            return &syms[i];
    return 0;
}

static struct sym *add_sym(const char *name)
{
    struct sym *s;
    struct sym *old = find_sym(name);
    if (old && old->kind == 0)
        return old;
    if (sym_n >= SYM_MAX)
        return 0;
    s = &syms[sym_n++];
    strncpy(s->name, name, 31);
    s->name[31] = '\0';
    s->kind = 0;
    s->type = TY_INT;
    s->is_ptr = 0;
    s->is_array = 0;
    s->arr_len = 0;
    s->size = 4;
    s->defined = 0;
    s->addr = 0;
    s->text_off = 0;
    return s;
}

/* ── Code emission ── */

static unsigned char text[TEXT_MAX];
static unsigned char rodata[RODATA_MAX];
static unsigned char data[DATA_MAX];
static int text_n, rodata_n, data_n;

struct fixup {
    int pos;
    int sym;
    int is_call;   /* 1 = E8 rel32, 0 = absolute imm32 */
};
static struct fixup fixups[FIXUP_MAX];
static int fixup_n;

struct labelfix {
    int pos;
    int next;
};
static struct labelfix lfix[LABEL_MAX];
static int lfix_n;
static int label_list[LABEL_MAX];
static int label_pos[LABEL_MAX];    /* -1 until emit_label() runs */
static int label_n;

static int loop_brk[16], loop_cont[16];
static int loop_depth;
static int err_line;

static void emit1(int b)
{
    if (text_n < TEXT_MAX)
        text[text_n++] = (unsigned char)b;
}
static void emit32(unsigned int v)
{
    emit1((int)(v & 0xFF));
    emit1((int)((v >> 8) & 0xFF));
    emit1((int)((v >> 16) & 0xFF));
    emit1((int)((v >> 24) & 0xFF));
}
static void emit_mrm(int m, int r, int rm)
{
    emit1((m << 6) | (r << 3) | rm);
}
static void emit_load_eax(int is_char)
{
    if (is_char) {
        emit1(0x0F); emit1(0xB6);   /* movzx eax, byte [eax] */
        emit_mrm(0, 0, 0);
    } else {
        emit1(0x8B);                /* mov eax, [eax] */
        emit_mrm(0, 0, 0);
    }
}
static void emit_store_edx(int is_char)
{
    if (is_char) {
        emit1(0x88);                /* mov byte [edx], al */
        emit_mrm(0, 0, 2);
    } else {
        emit1(0x89);                /* mov [edx], eax */
        emit_mrm(0, 0, 2);
    }
}
static void emit_cmp_eax_0(void)
{
    emit1(0x83); emit_mrm(3, 7, 0); emit1(0);
}
static void emit_setcc(int cc)
{
    emit1(0x0F); emit1(cc); emit_mrm(3, 0, 0);
    emit1(0x0F); emit1(0xB6); emit_mrm(3, 0, 0);   /* movzx eax, al */
}

static int new_label(void)
{
    if (label_n >= LABEL_MAX)
        return 0;
    label_list[label_n] = -1;
    label_pos[label_n] = -1;
    return label_n++;
}
static void emit_label(int id)
{
    int fx = label_list[id];
    label_pos[id] = text_n;         /* backward refs can target us now */
    while (fx >= 0) {
        int pp = lfix[fx].pos;
        int rel = text_n - (pp + 4);
        int i;
        for (i = 0; i < 4; i++) {
            int idx = pp + i;
            if (idx >= 0 && idx < text_n)
                text[idx] = (unsigned char)((rel >> (8 * i)) & 0xFF);
        }
        fx = lfix[fx].next;
    }
    label_list[id] = -1;
}
static void emit_rel32_label(int id)
{
    if (id >= 0 && id < label_n && label_pos[id] >= 0) {
        /* backward reference: target already emitted */
        emit32((unsigned int)(label_pos[id] - (text_n + 4)));
    } else if (lfix_n < LABEL_MAX) {
        lfix[lfix_n].pos = text_n;
        lfix[lfix_n].next = label_list[id];
        label_list[id] = lfix_n++;
        emit32(0);
    } else {
        emit32(0);
    }
}
static void emit_jmp(int id)
{
    emit1(0xE9);
    emit_rel32_label(id);
}
static void emit_jcc(int opcode, int id)
{
    emit1(0x0F);
    emit1(opcode);
    emit_rel32_label(id);
}
static void emit_fixup(int sym_idx, int is_call)
{
    if (fixup_n < FIXUP_MAX) {
        fixups[fixup_n].pos = text_n;
        fixups[fixup_n].sym = sym_idx;
        fixups[fixup_n].is_call = is_call;
        fixup_n++;
    }
    emit32(0);
}

/* ── Type info for expressions ── */

struct typeinfo {
    int base;        /* TY_* */
    int is_ptr;
    int is_lvalue;   /* address in EAX */
    int is_decay;    /* value == address (array/string) */
};

static int ti_elem_size(const struct typeinfo *t)
{
    if (t->is_ptr)
        return 4;
    return (t->base == TY_CHAR) ? 1 : 4;
}

/* ── Parser / codegen ── */

static int pos;
static int fn_local_off;
static int fn_frame_patch;

static struct token *cur(void) { return &toks[pos]; }
static int at(int kind) { return cur()->kind == kind; }
static int at2(int kind1, int kind2)
{
    return at(kind1) && pos + 1 < tok_n && toks[pos + 1].kind == kind2;
}
static int consume(int kind)
{
    if (at(kind)) { pos++; return 1; }
    return 0;
}
static void expect(int kind)
{
    if (!at(kind))
        err_line = cur()->line;
    pos++;
}

static void stmt(void);
static void expr(struct typeinfo *t);
static void assign(struct typeinfo *t);

/* qualifiers accepted and ignored */
static void skip_qualifiers(void)
{
    for (;;) {
        if (at(TK_IDENT)) {
            const char *n = str_pool + cur()->val;
            if (strcmp(n, "const") == 0 || strcmp(n, "static") == 0 ||
                strcmp(n, "unsigned") == 0 || strcmp(n, "register") == 0 ||
                strcmp(n, "volatile") == 0 || strcmp(n, "long") == 0 ||
                strcmp(n, "short") == 0 || strcmp(n, "signed") == 0 ||
                strcmp(n, "extern") == 0) {
                pos++;
                continue;
            }
        }
        break;
    }
}

/* decl_spec: base type + pointer flags */
static int decl_spec(int *is_ptr)
{
    int type = TY_INT;
    *is_ptr = 0;
    skip_qualifiers();
    if (at(TK_INT) || at(TK_TCHAR) || at(TK_VOID)) {
        type = at(TK_INT) ? TY_INT : at(TK_TCHAR) ? TY_CHAR : TY_VOID;
        pos++;
        skip_qualifiers();
    }
    while (at('*')) {
        *is_ptr = 1;
        pos++;
    }
    return type;
}

static int type_total(int base, int is_ptr, int is_array, int arr_len)
{
    if (is_ptr)
        return 4;
    if (is_array) {
        int esz = (base == TY_CHAR) ? 1 : 4;
        return esz * ((arr_len > 0) ? arr_len : 1);
    }
    return (base == TY_CHAR) ? 1 : 4;
}

/* ── Expression codegen ── */

/* lvalue -> EAX address; used for ident and *e and a[i] via assign()
 * and unary('&').  Sets is_char if the pointee is char. */
static int parse_lvalue_addr(int *is_char)
{
    int scale = 4;
    int elem_char = 0;      /* char element (indexed / *p result) */
    int char_lv = 0;        /* plain char variable (no index) */
    int indexed = 0;
    int base_is_ptr = 0;    /* base is a pointer lvalue (needs a load) */
    if (at(TK_IDENT)) {
        const char *n = str_pool + cur()->val;
        struct sym *s = find_sym(n);
        if (!s)
            s = add_sym(n);
        pos++;
        if (s->kind == 3) {
            emit1(0x8D);            /* lea eax, [ebp+disp32] */
            emit_mrm(2, 0, 5);
            emit32((unsigned int)s->addr);
        } else {
            emit1(0xB8);            /* mov eax, imm32 (absolute) */
            emit_fixup((int)(s - syms), 0);
        }
        base_is_ptr = s->is_ptr;
        elem_char = (s->type == TY_CHAR);
        char_lv = (s->type == TY_CHAR && !s->is_ptr && !s->is_array);
        scale = elem_char ? 1 : 4;
    } else if (at('*')) {
        struct typeinfo pt;
        pos++;
        expr(&pt);                  /* value = address (pointer) */
        elem_char = (pt.base == TY_CHAR);
        char_lv = elem_char;
        scale = elem_char ? 1 : 4;
    } else {
        return 0;
    }
    /* array/pointer indexing suffixes */
    while (at('[')) {
        pos++;
        if (base_is_ptr && !indexed)
            emit_load_eax(0);       /* pointer base: use its value */
        emit1(0x50);                /* push base */
        {
            struct typeinfo it;
            expr(&it);              /* index -> EAX */
        }
        emit1(0x89); emit_mrm(3, 0, 1);   /* mov ecx, eax */
        emit1(0x58);                      /* pop eax (base) */
        if (scale == 1) {
            emit1(0x8D); emit_mrm(0, 0, 4); emit1(0x08);
            /* lea eax, [eax+ecx] */
        } else {
            emit1(0x8D); emit_mrm(0, 0, 4); emit1(0x88);
            /* lea eax, [eax+ecx*4] */
        }
        expect(']');
        indexed = 1;
    }
    *is_char = indexed ? elem_char : char_lv;
    return 1;
}

static void coerce_value(struct typeinfo *t)
{
    if (t->is_lvalue && !t->is_decay) {
        emit_load_eax(ti_elem_size(t) == 1);
        t->is_lvalue = 0;
    }
}

static void lor(struct typeinfo *t);
static void land(struct typeinfo *t);
static void bitor(struct typeinfo *t);
static void bitxor(struct typeinfo *t);
static void bitand(struct typeinfo *t);
static void equality(struct typeinfo *t);
static void relational(struct typeinfo *t);
static void shift(struct typeinfo *t);
static void additive(struct typeinfo *t);
static void multiplicative(struct typeinfo *t);
static void unary(struct typeinfo *t);
static void postfix(struct typeinfo *t);
static void primary(struct typeinfo *t);

static void primary(struct typeinfo *t)
{
    struct token *tk = cur();
    if (tk->kind == TK_NUM || tk->kind == TK_CHAR) {
        emit1(0xB8);
        emit32((unsigned int)tk->val);
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
        pos++;
        return;
    }
    if (tk->kind == TK_STR) {
        struct sym *s = add_sym("\001str");
        s->kind = 5;
        s->addr = (uint32_t)rodata_n;
        {
            const char *sp = str_pool + tk->val;
            while (*sp) {
                if (rodata_n < RODATA_MAX)
                    rodata[rodata_n++] = (unsigned char)*sp++;
                else
                    sp++;
            }
            if (rodata_n < RODATA_MAX)
                rodata[rodata_n++] = 0;
            while (rodata_n & 3)
                rodata[rodata_n++] = 0;
        }
        emit1(0xB8);
        emit_fixup((int)(s - syms), 0);
        t->base = TY_CHAR; t->is_ptr = 1;
        t->is_lvalue = 0; t->is_decay = 1;
        pos++;
        return;
    }
    if (tk->kind == '(') {
        pos++;
        expr(t);
        expect(')');
        return;
    }
    if (tk->kind == TK_IDENT) {
        const char *n = str_pool + tk->val;
        struct sym *s = find_sym(n);
        if (!s)
            s = add_sym(n);
        pos++;

        if (at('(')) {
            /* function call */
            int nargs = 0;
            pos++;
            if (!at(')')) {
                for (;;) {
                    struct typeinfo ati;
                    expr(&ati);
                    emit1(0x50);            /* push eax */
                    nargs++;
                    if (!consume(','))
                        break;
                }
            }
            expect(')');
            emit1(0xE8);
            emit_fixup((int)(s - syms), 1);
            if (nargs > 0) {
                emit1(0x83);
                emit_mrm(3, 0, 4);
                emit1(nargs * 4);           /* add esp, imm8 */
            }
            t->base = (s->type == TY_VOID) ? TY_INT : s->type;
            t->is_ptr = s->is_ptr;
            t->is_lvalue = 0; t->is_decay = 0;
            return;
        }

        /* variable: address in EAX (lvalue) */
        if (s->kind == 3) {
            emit1(0x8D);
            emit_mrm(2, 0, 5);
            emit32((unsigned int)s->addr);
        } else {
            emit1(0xB8);
            emit_fixup((int)(s - syms), 0);
        }
        t->base = s->type;
        t->is_ptr = s->is_ptr;
        t->is_lvalue = 1;
        t->is_decay = s->is_array;
        return;
    }
    /* fallback: skip and emit 0 */
    err_line = tk->line;
    emit1(0xB8); emit32(0);
    t->base = TY_INT; t->is_ptr = 0;
    t->is_lvalue = 0; t->is_decay = 0;
    pos++;
}

static void postfix(struct typeinfo *t)
{
    primary(t);
    for (;;) {
        if (at('[')) {
            /* a[i] / p[i]: address = base + i*scale; still an lvalue.
             * base is the (already computed) address in EAX. */
            int scale = ti_elem_size(t);
            int elem_char = (t->base == TY_CHAR);
            pos++;
            if (t->is_ptr && !t->is_decay)
                emit_load_eax(0);       /* pointer lvalue: load value */
            emit1(0x50);                    /* push base */
            {
                struct typeinfo it;
                expr(&it);                  /* index -> EAX */
            }
            emit1(0x89); emit_mrm(3, 0, 1); /* mov ecx, eax */
            emit1(0x58);                    /* pop eax (base) */
            if (scale == 1) {
                emit1(0x8D); emit_mrm(0, 0, 4); emit1(0x08);
                /* lea eax, [eax+ecx] */
            } else {
                emit1(0x8D); emit_mrm(0, 0, 4); emit1(0x88);
                /* lea eax, [eax+ecx*4] */
            }
            expect(']');
            t->base = elem_char ? TY_CHAR : TY_INT;
            t->is_ptr = 0;
            t->is_lvalue = 1;
            t->is_decay = 0;
            continue;
        }
        break;
    }
}

static void unary(struct typeinfo *t)
{
    struct token *tk = cur();
    if (tk->kind == '-') {
        pos++;
        unary(t);
        coerce_value(t);
        emit1(0xF7); emit_mrm(3, 3, 0);     /* neg eax */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
        return;
    }
    if (tk->kind == '!') {
        pos++;
        unary(t);
        coerce_value(t);
        emit_cmp_eax_0();
        emit_setcc(0x94);                   /* sete */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
        return;
    }
    if (tk->kind == '~') {
        pos++;
        unary(t);
        coerce_value(t);
        emit1(0xF7); emit_mrm(3, 2, 0);     /* not eax */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
        return;
    }
    if (tk->kind == '*') {
        pos++;
        unary(t);                           /* operand: pointer value */
        coerce_value(t);
        t->is_lvalue = 1;                   /* EAX = address of pointee */
        t->is_decay = 0;
        t->is_ptr = 0;                      /* value now points at element */
        return;
    }
    if (tk->kind == '&') {
        int is_char;
        pos++;
        if (parse_lvalue_addr(&is_char)) {
            /* EAX already holds the address */
            (void)is_char;
            t->is_lvalue = 0;
            t->is_decay = 0;
            t->is_ptr = 1;
            return;
        }
        err_line = tk->line;
        emit1(0xB8); emit32(0);
        t->is_lvalue = 0; t->is_decay = 0;
        t->is_ptr = 1;
        return;
    }
    postfix(t);
}

static void multiplicative(struct typeinfo *t)
{
    unary(t);
    for (;;) {
        int op = cur()->kind;
        if (op == '*' || op == '/' || op == '%') {
            struct typeinfo rt;
            pos++;
            coerce_value(t);
            emit1(0x50);                    /* push lhs */
            unary(&rt);
            coerce_value(&rt);
            emit1(0x89); emit_mrm(3, 0, 1); /* mov ecx, eax (rhs) */
            emit1(0x58);                    /* pop eax (lhs) */
            if (op == '*') {
                emit1(0x0F); emit1(0xAF); emit_mrm(3, 0, 1);   /* imul eax, ecx */
            } else {
                emit1(0x99);                      /* cdq */
                emit1(0xF7); emit_mrm(3, 7, 1);   /* idiv ecx */
                if (op == '%') {
                    emit1(0x8B); emit_mrm(3, 0, 2); /* mov eax, edx */
                }
            }
            t->base = TY_INT; t->is_ptr = 0;
            t->is_lvalue = 0; t->is_decay = 0;
        } else {
            break;
        }
    }
}

static void additive(struct typeinfo *t)
{
    multiplicative(t);
    for (;;) {
        int op = cur()->kind;
        if (op == '+' || op == '-') {
            struct typeinfo rt;
            pos++;
            coerce_value(t);
            emit1(0x50);
            multiplicative(&rt);
            coerce_value(&rt);
            emit1(0x89); emit_mrm(3, 0, 1);
            emit1(0x58);
            if (op == '+') {
                emit1(0x01); emit_mrm(3, 1, 0);   /* add eax, ecx */
            } else {
                emit1(0x29); emit_mrm(3, 1, 0);   /* sub eax, ecx */
            }
            t->base = TY_INT; t->is_ptr = 0;
            t->is_lvalue = 0; t->is_decay = 0;
        } else {
            break;
        }
    }
}

static void shift(struct typeinfo *t)
{
    additive(t);
    for (;;) {
        int op = cur()->kind;
        if (op == TK_SHL || op == TK_SHR) {
            struct typeinfo rt;
            pos++;
            coerce_value(t);
            emit1(0x50);
            additive(&rt);
            coerce_value(&rt);
            emit1(0x89); emit_mrm(3, 0, 1);
            emit1(0x58);
            if (op == TK_SHL) {
                emit1(0xD3); emit_mrm(3, 4, 0);   /* shl eax, cl */
            } else {
                emit1(0xD3); emit_mrm(3, 7, 0);   /* sar eax, cl */
            }
            t->base = TY_INT; t->is_ptr = 0;
            t->is_lvalue = 0; t->is_decay = 0;
        } else {
            break;
        }
    }
}

static void relational(struct typeinfo *t)
{
    shift(t);
    for (;;) {
        int op = cur()->kind;
        int cc = -1;
        if (op == '<') cc = 0x9C;        /* setl */
        else if (op == '>') cc = 0x9F;   /* setg */
        else if (op == TK_LE) cc = 0x9E;
        else if (op == TK_GE) cc = 0x9D;
        if (cc < 0)
            break;
        {
            struct typeinfo rt;
            pos++;
            coerce_value(t);
            emit1(0x50);
            shift(&rt);
            coerce_value(&rt);
            emit1(0x89); emit_mrm(3, 0, 1);
            emit1(0x58);
            emit1(0x39); emit_mrm(3, 1, 0);  /* cmp eax, ecx */
            emit_setcc(cc);
        }
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void equality(struct typeinfo *t)
{
    relational(t);
    for (;;) {
        int op = cur()->kind;
        int cc;
        if (op == TK_EQ) cc = 0x94;      /* sete */
        else if (op == TK_NE) cc = 0x95;
        else break;
        {
            struct typeinfo rt;
            pos++;
            coerce_value(t);
            emit1(0x50);
            relational(&rt);
            coerce_value(&rt);
            emit1(0x89); emit_mrm(3, 0, 1);
            emit1(0x58);
            emit1(0x39); emit_mrm(3, 1, 0);
            emit_setcc(cc);
        }
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void bitand(struct typeinfo *t)
{
    equality(t);
    while (at('&')) {
        struct typeinfo rt;
        pos++;
        coerce_value(t);
        emit1(0x50);
        equality(&rt);
        coerce_value(&rt);
        emit1(0x89); emit_mrm(3, 0, 1);
        emit1(0x58);
        emit1(0x21); emit_mrm(3, 1, 0);  /* and eax, ecx */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void bitxor(struct typeinfo *t)
{
    bitand(t);
    while (at('^')) {
        struct typeinfo rt;
        pos++;
        coerce_value(t);
        emit1(0x50);
        bitand(&rt);
        coerce_value(&rt);
        emit1(0x89); emit_mrm(3, 0, 1);
        emit1(0x58);
        emit1(0x31); emit_mrm(3, 1, 0);  /* xor eax, ecx */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void bitor(struct typeinfo *t)
{
    bitxor(t);
    while (at('|')) {
        struct typeinfo rt;
        pos++;
        coerce_value(t);
        emit1(0x50);
        bitxor(&rt);
        coerce_value(&rt);
        emit1(0x89); emit_mrm(3, 0, 1);
        emit1(0x58);
        emit1(0x09); emit_mrm(3, 1, 0);  /* or eax, ecx */
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void land(struct typeinfo *t)
{
    bitor(t);
    while (at(TK_LAND)) {
        struct typeinfo rt;
        int lend = new_label();
        pos++;
        coerce_value(t);
        emit_cmp_eax_0();
        emit_jcc(0x84, lend);            /* je */
        bitor(&rt);
        coerce_value(&rt);
        emit_cmp_eax_0();
        emit_jcc(0x84, lend);
        emit1(0xB8); emit32(1);
        emit_jmp(lend);
        emit_label(lend);
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void lor(struct typeinfo *t)
{
    land(t);
    while (at(TK_LOR)) {
        struct typeinfo rt;
        int ltrue = new_label();
        int lend = new_label();
        pos++;
        coerce_value(t);
        emit_cmp_eax_0();
        emit_jcc(0x85, ltrue);           /* jne */
        land(&rt);
        coerce_value(&rt);
        emit_cmp_eax_0();
        emit_jcc(0x85, ltrue);
        emit1(0xB8); emit32(0);
        emit_jmp(lend);
        emit_label(ltrue);
        emit1(0xB8); emit32(1);
        emit_label(lend);
        t->base = TY_INT; t->is_ptr = 0;
        t->is_lvalue = 0; t->is_decay = 0;
    }
}

static void assign(struct typeinfo *t)
{
    int lv_char;
    struct typeinfo lt;

    /* Try to parse a leading lvalue; if it is followed by '=', do an
     * assignment, otherwise fall back to the full precedence chain
     * (which coerces the lvalue to a value). */
    {
        int save = pos;
        int save_text = text_n;
        int save_fixup = fixup_n;
        if (parse_lvalue_addr(&lv_char)) {
            if (at('=')) {
                pos++;                    /* consume '=' */
                emit1(0x50);              /* push eax (lvalue address) */
                {
                    struct typeinfo rt;
                    expr(&rt);
                }
                emit1(0x5A);              /* pop edx */
                emit_store_edx(lv_char);
                *t = lt;
                t->is_lvalue = 0;
                t->is_decay = 0;
                return;
            }
            /* not an assignment: rewind the tentative emission */
            text_n = save_text;
            fixup_n = save_fixup;
            pos = save;                   /* reparse */
        }
    }

    lor(&lt);
    coerce_value(&lt);
    *t = lt;
}

static void expr(struct typeinfo *t)
{
    assign(t);
}

/* ── Statements ── */

static void emit_epilogue(void)
{
    emit1(0x89); emit_mrm(3, 5, 4);      /* mov esp, ebp */
    emit1(0x5D);                          /* pop ebp */
    emit1(0xC3);                          /* ret */
}

static void stmt(void)
{
    struct token *tk = cur();

    if (tk->kind == '{') {
        pos++;
        while (!at('}') && !at(TK_EOF))
            stmt();
        expect('}');
        return;
    }
    if (tk->kind == TK_RETURN) {
        pos++;
        if (!at(';')) {
            struct typeinfo rt;
            expr(&rt);
        }
        expect(';');
        emit_epilogue();
        return;
    }
    if (tk->kind == TK_IF) {
        int lelse, lend;
        pos++;
        expect('(');
        {
            struct typeinfo ct;
            expr(&ct);
        }
        expect(')');
        emit_cmp_eax_0();
        lelse = new_label();
        lend = new_label();
        emit_jcc(0x84, lelse);           /* je else */
        stmt();
        if (consume(TK_ELSE)) {
            emit_jmp(lend);
            emit_label(lelse);
            stmt();
            emit_label(lend);
        } else {
            emit_label(lelse);
        }
        return;
    }
    if (tk->kind == TK_WHILE) {
        int lcont, lbrk;
        pos++;
        lcont = new_label();
        lbrk = new_label();
        if (loop_depth < 16) {
            loop_brk[loop_depth] = lbrk;
            loop_cont[loop_depth] = lcont;
            loop_depth++;
        }
        emit_label(lcont);
        expect('(');
        {
            struct typeinfo ct;
            expr(&ct);
        }
        expect(')');
        emit_cmp_eax_0();
        emit_jcc(0x84, lbrk);
        stmt();
        emit_jmp(lcont);
        emit_label(lbrk);
        if (loop_depth > 0)
            loop_depth--;
        return;
    }
    if (tk->kind == TK_FOR) {
        int lcont, lbrk, linc = -1, lskip = -1;
        pos++;
        expect('(');
        if (!at(';')) {
            struct typeinfo it;
            expr(&it);
        }
        expect(';');
        lcont = new_label();
        lbrk = new_label();
        if (loop_depth < 16) {
            loop_brk[loop_depth] = lbrk;
            loop_cont[loop_depth] = lcont;
            loop_depth++;
        }
        emit_label(lcont);
        if (!at(';')) {
            struct typeinfo ct;
            expr(&ct);
            emit_cmp_eax_0();
            emit_jcc(0x84, lbrk);
        }
        expect(';');
        if (!at(')')) {
            struct typeinfo it;
            lskip = new_label();
            linc = new_label();
            emit_jmp(lskip);        /* first pass: skip the inc */
            emit_label(linc);       /* inc runs after each body */
            expr(&it);
            emit_jmp(lcont);        /* then loop back to the cond */
            emit_label(lskip);      /* body entry */
        }
        expect(')');
        stmt();
        if (linc >= 0)
            emit_jmp(linc);         /* body done: run the inc */
        emit_label(lbrk);
        if (loop_depth > 0)
            loop_depth--;
        return;
    }
    if (tk->kind == TK_BREAK) {
        pos++;
        expect(';');
        if (loop_depth > 0)
            emit_jmp(loop_brk[loop_depth - 1]);
        return;
    }
    if (tk->kind == TK_CONTINUE) {
        pos++;
        expect(';');
        if (loop_depth > 0)
            emit_jmp(loop_cont[loop_depth - 1]);
        return;
    }
    if (tk->kind == TK_INT || tk->kind == TK_TCHAR || tk->kind == TK_VOID ||
        (tk->kind == TK_IDENT &&
         (strcmp(str_pool + tk->val, "const") == 0 ||
          strcmp(str_pool + tk->val, "static") == 0 ||
          strcmp(str_pool + tk->val, "unsigned") == 0 ||
          strcmp(str_pool + tk->val, "register") == 0 ||
          strcmp(str_pool + tk->val, "volatile") == 0))) {
        /* local declaration */
        int ip;
        int base = decl_spec(&ip);
        for (;;) {
            if (!at(TK_IDENT))
                break;
            {
                const char *n = str_pool + cur()->val;
                struct sym *s = add_sym(n);
                int is_arr = 0, alen = 0;
                s->kind = 3;
                s->type = base;
                s->is_ptr = ip;
                pos++;                      /* consume the name */
                if (at('[')) {
                    pos++;
                    is_arr = 1;
                    if (at(TK_NUM)) {
                        alen = cur()->val;
                        pos++;
                    }
                    expect(']');
                }
                s->is_array = is_arr;
                s->arr_len = alen;
                s->size = type_total(base, ip, is_arr, alen);
                /* allocate frame slot (aligned 4) */
                {
                    int sz = s->size;
                    if (sz < 4) sz = 4;
                    fn_local_off -= sz;
                    s->addr = (uint32_t)fn_local_off;
                }
                if (consume('=')) {
                    /* emit: lea eax,[ebp+off]; push; value; pop edx; store */
                    emit1(0x8D);
                    emit_mrm(2, 0, 5);
                    emit32((unsigned int)s->addr);
                    emit1(0x50);                    /* push eax (lvalue address) */
                    {
                        struct typeinfo vt;
                        expr(&vt);
                    }
                    emit1(0x5A);
                    emit_store_edx((base == TY_CHAR) && !ip && !is_arr);
                }
            }
            if (!consume(','))
                break;
        }
        expect(';');
        return;
    }
    /* expression statement */
    {
        struct typeinfo et;
        expr(&et);
        expect(';');
    }
}

/* ── Top level: declarations ── */

static void fn_def(void);

static void global_var(const char *n, int base, int ip, int is_arr, int alen)
{
    struct sym *s = add_sym(n);
    s->kind = 2;
    s->type = base;
    s->is_ptr = ip;
    s->is_array = is_arr;
    s->arr_len = alen;
    s->size = type_total(base, ip, is_arr, alen);
    s->defined = 1;

    if (at('=')) {
        pos++;
        s->addr = (uint32_t)data_n;
        if (ip) {
            /* pointer init: only string literals supported */
            if (at(TK_STR)) {
                struct sym *st = add_sym("\001str");
                st->kind = 5;
                st->addr = (uint32_t)rodata_n;
                {
                    const char *sp = str_pool + cur()->val;
                    while (*sp) {
                        if (rodata_n < RODATA_MAX)
                            rodata[rodata_n++] = (unsigned char)*sp++;
                        else
                            sp++;
                    }
                    if (rodata_n < RODATA_MAX)
                        rodata[rodata_n++] = 0;
                    while (rodata_n & 3)
                        rodata[rodata_n++] = 0;
                }
                /* data fixup: 4 bytes placeholder */
                if (fixup_n < FIXUP_MAX) {
                    fixups[fixup_n].pos = data_n;
                    fixups[fixup_n].sym = (int)(st - syms);
                    fixups[fixup_n].is_call = 2;   /* data fixup marker */
                    fixup_n++;
                }
                data[data_n++] = 0;
                data[data_n++] = 0;
                data[data_n++] = 0;
                data[data_n++] = 0;
                pos++;
            } else {
                int v = cur()->val;
                pos++;
                data[data_n++] = (unsigned char)(v & 0xFF);
                data[data_n++] = (unsigned char)((v >> 8) & 0xFF);
                data[data_n++] = (unsigned char)((v >> 16) & 0xFF);
                data[data_n++] = (unsigned char)((v >> 24) & 0xFF);
            }
            while (data_n & 3)
                data[data_n++] = 0;
            return;
        }
        if (at(TK_STR)) {
            /* char array initialized from string */
            const char *sp = str_pool + cur()->val;
            int n = 0, sz = s->size;
            if (sz <= 0) sz = (int)strlen(sp) + 1;
            while (*sp && n < sz - 1) {
                data[data_n++] = (unsigned char)*sp++;
                n++;
            }
            data[data_n++] = 0;
            pos++;
            while (n < sz) {
                data[data_n++] = 0;
                n++;
            }
            while (data_n & 3)
                data[data_n++] = 0;
            return;
        }
        if (at('{')) {
            int n = 0, sz = s->size;
            int esz = (base == TY_CHAR) ? 1 : 4;
            pos++;
            while (!at('}')) {
                int v = cur()->val;
                if (at(TK_CHAR))
                    v = cur()->val;
                pos++;
                if (esz == 1) {
                    data[data_n++] = (unsigned char)(v & 0xFF);
                    n++;
                } else {
                    data[data_n++] = (unsigned char)(v & 0xFF);
                    data[data_n++] = (unsigned char)((v >> 8) & 0xFF);
                    data[data_n++] = (unsigned char)((v >> 16) & 0xFF);
                    data[data_n++] = (unsigned char)((v >> 24) & 0xFF);
                    n += 4;
                }
                if (!consume(','))
                    break;
            }
            expect('}');
            while (n < sz) {
                data[data_n++] = 0;
                n++;
            }
            while (data_n & 3)
                data[data_n++] = 0;
            return;
        }
        /* scalar */
        {
            int v = cur()->val;
            if (at(TK_CHAR))
                v = cur()->val;
            pos++;
            if (base == TY_CHAR) {
                data[data_n++] = (unsigned char)(v & 0xFF);
                data[data_n++] = 0;
                data[data_n++] = 0;
                data[data_n++] = 0;
            } else {
                data[data_n++] = (unsigned char)(v & 0xFF);
                data[data_n++] = (unsigned char)((v >> 8) & 0xFF);
                data[data_n++] = (unsigned char)((v >> 16) & 0xFF);
                data[data_n++] = (unsigned char)((v >> 24) & 0xFF);
            }
        }
        return;
    }
    /* no initializer: bss */
    s->addr = 0xFFFFFFFFu;
}

static void top_level(void)
{
    for (;;) {
        int ip;
        int base;
        struct token *tk = cur();
        if (tk->kind == TK_EOF)
            break;

        base = decl_spec(&ip);
        if (!at(TK_IDENT)) {
            err_line = tk->line;
            pos++;
            continue;
        }
        {
            const char *n = str_pool + cur()->val;
            struct sym *s = find_sym(n);
            pos++;                      /* consume the declarator name */
            if (at('(')) {
                /* function: fn_def expects to start at the name */
                pos--;
                fn_def();
            } else {
                int is_arr = 0, alen = 0;
                if (at('[')) {
                    pos++;
                    is_arr = 1;
                    if (at(TK_NUM)) {
                        alen = cur()->val;
                        pos++;
                    }
                    expect(']');
                }
                /* global_var expects pos at '=' or ';' */
                global_var(n, base, ip, is_arr, alen);
                expect(';');
                (void)s;
            }
        }
    }
}

static void fn_def(void)
{
    int ip;
    int base = decl_spec(&ip);
    const char *n;
    struct sym *s;
    int nparams = 0;
    int fn_start;
    int fn_ret_void = 0;

    if (!at(TK_IDENT)) {
        err_line = cur()->line;
        pos++;
        return;
    }
    n = str_pool + cur()->val;
    pos++;
    s = find_sym(n);
    if (!s)
        s = add_sym(n);
    /* Never clobber a runtime stub (_start/_rtsys3/_rtsys1): the runtime
     * source declares `extern int _rtsys3(...)`, which flows through
     * here as a prototype. */
    if (s->kind != 4) {
        s->kind = 1;
        s->type = base;
        s->is_ptr = ip;
        s->defined = 0;
    }
    fn_ret_void = (base == TY_VOID);

    expect('(');
    if (!at(')')) {
        for (;;) {
            int pip;
            int pbase;
            if (at2(TK_VOID, ')')) {
                /* (void): check BEFORE decl_spec consumes the keyword */
                pos++;
                break;
            }
            pbase = decl_spec(&pip);
            if (!at(TK_IDENT)) {
                err_line = cur()->line;
                pos++;
                break;
            }
            {
                const char *pn = str_pool + cur()->val;
                struct sym *ps = add_sym(pn);
                ps->kind = 3;
                ps->type = pbase;
                ps->is_ptr = pip;
                ps->size = 4;
                ps->addr = (uint32_t)(8 + 4 * nparams);
                nparams++;
                pos++;
            }
            if (!consume(','))
                break;
            if (nparams >= 16)
                break;
        }
    }
    expect(')');

    if (at(';')) {
        /* prototype */
        s->defined = 0;
        s->type = base;
        s->is_ptr = ip;
        pos++;
        return;
    }
    expect('{');

    /* emit prologue */
    fn_start = text_n;
    emit1(0x55);                        /* push ebp */
    emit1(0x89); emit_mrm(3, 4, 5);     /* mov ebp, esp */
    emit1(0x81); emit_mrm(3, 5, 4);     /* sub esp, imm32 */
    emit32(0);
    fn_frame_patch = text_n - 4;

    fn_local_off = 0;

    /* Params are addressed directly as [ebp+8+4i] (see param
     * declaration above), so no copy-in code is needed: reads use the
     * argument slot, writes land in the caller's stack slot which is
     * popped right after the call returns. */

    while (!at('}') && !at(TK_EOF))
        stmt();
    expect('}');

    /* patch frame size */
    {
        unsigned int fsize = (unsigned int)(-fn_local_off);
        int i;
        for (i = 0; i < 4; i++) {
            int idx = fn_frame_patch + i;
            if (idx >= 0 && idx < text_n)
                text[idx] = (unsigned char)((fsize >> (8 * i)) & 0xFF);
        }
    }

    emit_epilogue();

    s->text_off = fn_start;
    s->defined = 1;
    (void)fn_ret_void;
}

/* ── Runtime stubs (raw machine code) ── */

static void emit_stubs(void)
{
    struct sym *s_start = add_sym("_start");
    struct sym *s3 = add_sym("_rtsys3");
    struct sym *s1 = add_sym("_rtsys1");

    /* _start: call main; exit(eax) */
    s_start->kind = 4;
    s_start->addr = (uint32_t)text_n;
    emit1(0xE8);
    {
        struct sym *m = add_sym("main");
        emit_fixup((int)(m - syms), 1);
    }
    emit1(0x89); emit_mrm(3, 0, 3);     /* mov ebx, eax */
    emit1(0xB8);
    emit32(PCC_SYS_EXIT);               /* mov eax, EXIT */
    emit1(0xCD); emit1(PCC_SYS_INT);    /* int */
    emit1(0xF4);                        /* hlt */

    /* _rtsys3(n,a,b,c) -> syscall(n,a,b,c).  Args are pushed left to
     * right, so the first arg (n) sits at [esp+16] on entry. */
    s3->kind = 4;
    s3->addr = (uint32_t)text_n;
    emit1(0x8B); emit_mrm(1, 0, 4); emit1(0x24); emit1(0x10);  /* mov eax,[esp+16] */
    emit1(0x8B); emit_mrm(1, 3, 4); emit1(0x24); emit1(0x0C);  /* mov ebx,[esp+12] */
    emit1(0x8B); emit_mrm(1, 1, 4); emit1(0x24); emit1(0x08);  /* mov ecx,[esp+8] */
    emit1(0x8B); emit_mrm(1, 2, 4); emit1(0x24); emit1(0x04);  /* mov edx,[esp+4] */
    emit1(0xCD); emit1(PCC_SYS_INT);
    emit1(0xC2); emit1(0x10); emit1(0x00);                      /* ret 16 */

    /* _rtsys1(n,a) -> syscall(n,a): n at [esp+8], a at [esp+4] */
    s1->kind = 4;
    s1->addr = (uint32_t)text_n;
    emit1(0x8B); emit_mrm(1, 0, 4); emit1(0x24); emit1(0x08);  /* mov eax,[esp+8] */
    emit1(0x8B); emit_mrm(1, 3, 4); emit1(0x24); emit1(0x04);  /* mov ebx,[esp+4] */
    emit1(0xCD); emit1(PCC_SYS_INT);
    emit1(0xC2); emit1(0x08); emit1(0x00);                      /* ret 8 */
}

/* ── Runtime C source (compiled into every generated program) ── */

static void build_runtime_src(char *dst, int max)
{
    /* NOTE: the C subset has no ++/--, so every increment is spelled
     * out as x = x + 1.  puthex prints 8 hex digits, most significant
     * nibble first (shifts 28,24,...,0). */
    static const char tmpl[] =
        "extern int _rtsys3(int a, int b, int c, int d);\n"
        "static int _wr(int fd, char *buf, int n) { "
        "return _rtsys3(%d, fd, buf, n); }\n"
        "static int _wrch(int c) { "
        "char b[2]; b[0] = c; return _wr(1, b, 1); }\n"
        "int putchar(int c) { return _wrch(c); }\n"
        "int putstr(char *s) { "
        "while (*s) { _wrch(*s); s = s + 1; } return 0; }\n"
        "int puts(char *s) { "
        "putstr(s); return _wrch(10); }\n"
        "int putdec(int v) { "
        "char d[16]; int i; int neg;\n"
        "i = 0; neg = 0;\n"
        "if (v < 0) { neg = 1; v = 0 - v; }\n"
        "while (v > 0) { d[i] = 48 + v %% 10; i = i + 1; v = v / 10; }\n"
        "if (i == 0) { d[i] = 48; i = i + 1; }\n"
        "if (neg) { _wrch('-'); }\n"
        "while (i > 0) { i = i - 1; _wrch(d[i]); }\n"
        "return 0; }\n"
        "int puthex(int v) { "
        "int i; int h; int c;\n"
        "for (i = 0; i < 8; i = i + 1) { "
        "h = (v >> (28 - i * 4)) & 15; "
        "if (h < 10) { c = 48 + h; } "
        "else { c = 97 + h - 10; } "
        "_wrch(c); }\n"
        "return 0; }\n";
    snprintf(dst, max, tmpl, PCC_SYS_WRITE);
}

/* ── Link pass ── */

static int link_pass(void)
{
    unsigned int text_base = PCC_LOAD_BASE;
    unsigned int text_sz = (unsigned int)((text_n + 15) & ~15);
    unsigned int ro_base = text_base + text_sz;
    unsigned int ro_sz = (unsigned int)((rodata_n + 15) & ~15);
    unsigned int data_base = ro_base + ro_sz;
    unsigned int data_sz = (unsigned int)((data_n + 15) & ~15);
    unsigned int bss_base = data_base + data_sz;
    unsigned int bss_off = 0;
    int i;

    /* assign addresses */
    for (i = 0; i < sym_n; i++) {
        struct sym *s = &syms[i];
        if (s->kind == 1) {
            s->addr = text_base + (unsigned int)s->text_off;
        } else if (s->kind == 4) {
            s->addr = text_base + s->addr;
        } else if (s->kind == 5) {
            s->addr = ro_base + s->addr;
        } else if (s->kind == 2) {
            if (s->addr == 0xFFFFFFFFu) {
                int sz = s->size;
                if (sz < 4) sz = 4;
                s->addr = bss_base + bss_off;
                bss_off += (unsigned int)((sz + 3) & ~3);
            } else {
                s->addr = data_base + s->addr;
            }
        } else if (s->kind == 0) {
            /* unresolved: look up a defined symbol with the same name */
            int j;
            for (j = 0; j < sym_n; j++) {
                if (j != i && syms[j].kind == 1 &&
                    strcmp(syms[j].name, s->name) == 0) {
                    s->addr = text_base +
                        (unsigned int)syms[j].text_off;
                    s->kind = 1;
                    break;
                }
            }
            if (s->kind == 0) {
                /* undefined symbol */
                return -1;
            }
        }
    }

    /* apply fixups */
    for (i = 0; i < fixup_n; i++) {
        struct fixup *f = &fixups[i];
        unsigned int target;
        int fpos;
        if (f->sym < 0 || f->sym >= sym_n)
            return -1;
        target = syms[f->sym].addr;
        if (f->is_call == 2) {
            /* data fixup: patch 4 bytes in data[] */
            fpos = f->pos;
            if (fpos + 3 < data_n) {
                data[fpos]     = (unsigned char)(target & 0xFF);
                data[fpos + 1] = (unsigned char)((target >> 8) & 0xFF);
                data[fpos + 2] = (unsigned char)((target >> 16) & 0xFF);
                data[fpos + 3] = (unsigned char)((target >> 24) & 0xFF);
            }
            continue;
        }
        fpos = f->pos;
        if (fpos + 3 >= text_n)
            return -1;
        if (f->is_call)
            /* rel32 = target - (address of the instruction AFTER the call) */
            target = target - (text_base + (unsigned int)fpos + 4);
        text[fpos]     = (unsigned char)(target & 0xFF);
        text[fpos + 1] = (unsigned char)((target >> 8) & 0xFF);
        text[fpos + 2] = (unsigned char)((target >> 16) & 0xFF);
        text[fpos + 3] = (unsigned char)((target >> 24) & 0xFF);
    }
    return 0;
}

/* ── ELF writer ── */

static unsigned char out[OUT_MAX];

struct elf32_hdr {
    unsigned char  e_ident[16];
    unsigned short e_type, e_machine;
    unsigned int   e_version;
    unsigned int   e_entry;
    unsigned int   e_phoff;
    unsigned int   e_shoff;
    unsigned int   e_flags;
    unsigned short e_ehsize, e_phentsize, e_phnum;
    unsigned short e_shentsize, e_shnum, e_shstrndx;
};

struct elf32_phdr {
    unsigned int p_type, p_offset, p_vaddr, p_paddr;
    unsigned int p_filesz, p_memsz, p_flags, p_align;
};

struct elf32_shdr {
    unsigned int sh_name, sh_type, sh_flags, sh_addr, sh_offset;
    unsigned int sh_size, sh_link, sh_info, sh_addralign, sh_entsize;
};

static int write_elf(const char *path)
{
    unsigned int text_base = PCC_LOAD_BASE;
    unsigned int text_sz = (unsigned int)((text_n + 15) & ~15);
    unsigned int ro_sz = (unsigned int)((rodata_n + 15) & ~15);
    unsigned int data_sz = (unsigned int)((data_n + 15) & ~15);
    unsigned int bss_sz = 0;
    unsigned int off_text = 0x1000;
    unsigned int off_ro, off_data, off_shstr, off_shdr;
    unsigned int total;
    int i;
    const char *shstr = ".shstrtab\0.text\0.rodata\0.data\0.bss";
    int shstr_len = 35;     /* packed length incl. trailing NUL */
    PCC_FILE fp;

    /* recompute bss size from bss globals: same pass as link_pass so
     * the bss segment and the bss addresses stay in sync */
    {
        unsigned int bb = PCC_LOAD_BASE + text_sz + ro_sz + data_sz;
        unsigned int bo = 0;
        for (i = 0; i < sym_n; i++) {
            struct sym *s = &syms[i];
            if (s->kind == 2 && s->addr == 0xFFFFFFFFu) {
                int sz = s->size;
                if (sz < 4) sz = 4;
                s->addr = bb + bo;
                bo += (unsigned int)((sz + 3) & ~3);
            }
        }
        bss_sz = bo;
    }

    off_ro = off_text + text_sz;
    off_data = off_ro + ro_sz;
    off_shstr = off_data + data_sz;
    {
        int sl = shstr_len;
        if (sl & 3) sl = (sl + 4) & ~3;
        off_shdr = off_shstr + (unsigned int)sl;
    }
    total = off_shdr + 6 * (unsigned int)sizeof(struct elf32_shdr);

    if (total > OUT_MAX)
        return -1;

    memset(out, 0, total);

    {
        struct elf32_hdr *eh = (struct elf32_hdr *)out;
        eh->e_ident[0] = 0x7F; eh->e_ident[1] = 'E';
        eh->e_ident[2] = 'L'; eh->e_ident[3] = 'F';
        eh->e_ident[4] = 1; eh->e_ident[5] = 1; eh->e_ident[6] = 1;
        eh->e_ident[7] = 0;
        eh->e_type = 2;
        eh->e_machine = 3;
        eh->e_version = 1;
        eh->e_entry = text_base;
        eh->e_phoff = (unsigned int)sizeof(struct elf32_hdr);
        eh->e_shoff = off_shdr;
        eh->e_flags = 0;
        eh->e_ehsize = (unsigned short)sizeof(struct elf32_hdr);
        eh->e_phentsize = (unsigned short)sizeof(struct elf32_phdr);
        eh->e_phnum = 2;
        eh->e_shentsize = (unsigned short)sizeof(struct elf32_shdr);
        eh->e_shnum = 6;
        eh->e_shstrndx = 1;
    }
    {
        struct elf32_phdr *p1 = (struct elf32_phdr *)(out +
            sizeof(struct elf32_hdr));
        struct elf32_phdr *p2 = p1 + 1;
        p1->p_type = 1;
        p1->p_offset = off_text;
        p1->p_vaddr = text_base;
        p1->p_paddr = text_base;
        p1->p_filesz = text_sz + ro_sz;
        p1->p_memsz = text_sz + ro_sz;
        p1->p_flags = 5;
        p1->p_align = 0x1000;
        p2->p_type = 1;
        p2->p_offset = off_data;
        p2->p_vaddr = text_base + text_sz + ro_sz;
        p2->p_paddr = p2->p_vaddr;
        p2->p_filesz = data_sz;
        p2->p_memsz = data_sz + bss_sz;
        p2->p_flags = 6;
        p2->p_align = 0x1000;
    }
    {
        struct elf32_shdr *sh = (struct elf32_shdr *)(out + off_shdr);
        const char *names = shstr;
        int n_off = 11;             /* ".text" */
        int r_off = 17;             /* ".rodata" */
        int d_off = 25;             /* ".data" */
        int b_off = 31;             /* ".bss" */
        /* section 0: null */
        sh[0].sh_type = 0;
        /* 1: .shstrtab */
        sh[1].sh_name = 1;
        sh[1].sh_type = 3;
        sh[1].sh_offset = off_shstr;
        sh[1].sh_size = (unsigned int)shstr_len;
        sh[1].sh_addralign = 1;
        /* 2: .text */
        sh[2].sh_name = (unsigned int)n_off;
        sh[2].sh_type = 1;
        sh[2].sh_flags = 6;         /* AX */
        sh[2].sh_addr = text_base;
        sh[2].sh_offset = off_text;
        sh[2].sh_size = text_sz;
        sh[2].sh_addralign = 16;
        /* 3: .rodata */
        sh[3].sh_name = (unsigned int)r_off;
        sh[3].sh_type = 1;
        sh[3].sh_flags = 2;         /* A */
        sh[3].sh_addr = text_base + text_sz;
        sh[3].sh_offset = off_ro;
        sh[3].sh_size = ro_sz;
        sh[3].sh_addralign = 16;
        /* 4: .data */
        sh[4].sh_name = (unsigned int)d_off;
        sh[4].sh_type = 1;
        sh[4].sh_flags = 3;         /* WA */
        sh[4].sh_addr = text_base + text_sz + ro_sz;
        sh[4].sh_offset = off_data;
        sh[4].sh_size = data_sz;
        sh[4].sh_addralign = 16;
        /* 5: .bss */
        sh[5].sh_name = (unsigned int)b_off;
        sh[5].sh_type = 8;          /* NOBITS */
        sh[5].sh_flags = 3;
        sh[5].sh_addr = text_base + text_sz + ro_sz + data_sz;
        sh[5].sh_offset = off_data + data_sz;
        sh[5].sh_size = bss_sz;
        sh[5].sh_addralign = 16;
        /* section name string table */
        memcpy(out + off_shstr, names, shstr_len);
    }
    memcpy(out + off_text, text, (size_t)text_n);
    memcpy(out + off_ro, rodata, (size_t)rodata_n);
    memcpy(out + off_data, data, (size_t)data_n);

    fp = PCC_FOPEN_W(path);
    if (PCC_FBAD(fp))
        return -1;
    if (PCC_FWRITE(fp, out, total) != (int)total) {
        PCC_FCLOSE(fp);
        return -1;
    }
    PCC_FCLOSE(fp);
    return (int)total;
}

/* ── Salt ── */

static unsigned int salt_word(void)
{
#ifdef PCC_HOST
    return (unsigned int)(time(0) ^ (unsigned)getpid() ^
                          (unsigned)(unsigned long)&salt_word);
#else
    return pcc_mtime() * 2654435761u ^
           (pcc_muptime() * 40503u) ^
           (unsigned int)(unsigned long)&salt_word ^ 0x9E3779B9u;
#endif
}

static void salt_hex(char *out6)
{
    static const char hx[] = "0123456789abcdef";
    unsigned int a = salt_word();
    unsigned int b = salt_word();
    int i;
    for (i = 0; i < 8; i++) {
        out6[i] = hx[(a >> (28 - i * 4)) & 15];
        out6[8 + i] = hx[(b >> (28 - i * 4)) & 15];
    }
    out6[16] = '\0';
}

/* ── CLI ── */

static char av[32][64];
static int ac;

#ifdef PCC_HOST
static void collect_args(int argc, char **argv)
{
    int i;
    if (argc > 32) argc = 32;
    for (i = 0; i < argc; i++) {
        strncpy(av[i], argv[i], 63);
        av[i][63] = '\0';
    }
    ac = argc;
}
#else
static void collect_args(void)
{
    /* M4KK1: m4k_spawn carries no argv; the shell writes the command
     * line into /tmp/pcc.cmd before spawning /bin/pcc. */
    PCC_FILE fp = PCC_FOPEN_R("/tmp/pcc.cmd");
    char buf[CMD_MAX];
    int n = 0, i = 0;
    if (PCC_FBAD(fp)) {
        av[0][0] = '\0';
        ac = 0;
        return;
    }
    n = PCC_FREAD(fp, buf, CMD_MAX - 1);
    PCC_FCLOSE(fp);
    buf[n] = '\0';
    ac = 0;
    while (buf[i] && ac < 32) {
        while (buf[i] == ' ' || buf[i] == '\t')
            i++;
        if (!buf[i])
            break;
        {
            int j = 0;
            while (buf[i] && buf[i] != ' ' && buf[i] != '\t' && j < 63)
                av[ac][j++] = buf[i++];
            av[ac][j] = '\0';
            ac++;
        }
    }
}
#endif

/* m4k_libc printf only knows %d/%s/%c, so hex is formatted by hand */
static void print_hex(unsigned int v)
{
    static const char hx[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = hx[(v >> (28 - i * 4)) & 15];
    buf[10] = '\0';
    printf("%s", buf);
}

static void print_version(void)
{
    printf("pcc %s (%s)\n", PCC_VERSION, PCC_TARGET);
    printf("  backend: M4KK1 self-hosted C compiler (C subset)\n");
    printf("  output:  static ELF32 executables at ");
    print_hex(PCC_LOAD_BASE);
    printf("\n");
    printf("Copyright (c) 2026 Yaku Makki\n");
}

static void print_usage(void)
{
    printf("usage: pcc [options] file.c\n");
    printf("  -v          print version information\n");
    printf("  -o <file>   write output to <file> (default: a.out)\n");
    printf("  -h          show this help\n");
}

/* ── Entry ── */

#ifdef PCC_HOST
int main(int argc, char **argv)
{
    collect_args(argc, argv);
#else
int main(void)
{
    collect_args();
#endif
    const char *in_file = 0;
    const char *out_file = "a.out";
    int want_version = 0;
    int i;
    static char src[SRC_MAX];
    static char rt_src[8192];
    char salt[17];
    int r;

    for (i = 1; i < ac; i++) {
        if (av[i][0] == '-' && av[i][1]) {
            if (strcmp(av[i], "-v") == 0 ||
                strcmp(av[i], "--version") == 0) {
                want_version = 1;
            } else if (strcmp(av[i], "-o") == 0 && i + 1 < ac) {
                out_file = av[++i];
            } else if (strcmp(av[i], "-h") == 0 ||
                       strcmp(av[i], "--help") == 0) {
                print_usage();
                return 0;
            } else {
                printf("pcc: unknown option '%s'\n", av[i]);
                print_usage();
                return 1;
            }
        } else {
            in_file = av[i];
        }
    }

    if (want_version)
        print_version();
    if (!in_file) {
        if (!want_version)
            print_usage();
        return want_version ? 0 : 1;
    }

    /* read source */
    {
        PCC_FILE fp = PCC_FOPEN_R(in_file);
        if (PCC_FBAD(fp)) {
            printf("pcc: cannot open '%s'\n", in_file);
            return 1;
        }
        r = PCC_FREAD(fp, src, SRC_MAX - 1);
        PCC_FCLOSE(fp);
        if (r <= 0) {
            printf("pcc: empty source file\n");
            return 1;
        }
        src[r] = '\0';
    }

    salt_hex(salt);

    /* reset compiler state */
    sym_n = 0;
    text_n = 0;
    rodata_n = 0;
    data_n = 0;
    fixup_n = 0;
    lfix_n = 0;
    label_n = 0;
    loop_depth = 0;
    err_line = 0;

    /* embed the build salt into .rodata so every build differs */
    {
        int i2;
        for (i2 = 0; i2 < 9; i2++) {
            const char *tag = "PCCSALT:";
            if (i2 < 8) {
                if (rodata_n < RODATA_MAX)
                    rodata[rodata_n++] = (unsigned char)tag[i2];
            } else {
                if (rodata_n < RODATA_MAX)
                    rodata[rodata_n++] = (unsigned char)salt[i2 - 8];
            }
        }
        for (i2 = 0; i2 < 8; i2++)
            if (rodata_n < RODATA_MAX)
                rodata[rodata_n++] = (unsigned char)salt[i2 + 8];
        while (rodata_n & 3)
            rodata[rodata_n++] = 0;
    }

    /* runtime stubs + runtime C library */
    emit_stubs();
    build_runtime_src(rt_src, (int)sizeof(rt_src));
    tokenize(rt_src);
    pos = 0;
    top_level();
    if (err_line) {
        printf("pcc: runtime compile error (line %d)\n", err_line);
        return 1;
    }

    /* user source */
    tokenize(src);
    pos = 0;
    top_level();
    if (err_line) {
        printf("pcc: %s:%d: parse error\n", in_file, err_line);
        return 1;
    }

    if (link_pass() != 0) {
        printf("pcc: undefined symbol in '%s'\n", in_file);
        return 1;
    }

    r = write_elf(out_file);
    if (r < 0) {
        printf("pcc: cannot write '%s'\n", out_file);
        return 1;
    }

    printf("pcc: %s -> %s (%d bytes, build salt %s)\n",
           in_file, out_file, r, salt);
    return 0;
}
