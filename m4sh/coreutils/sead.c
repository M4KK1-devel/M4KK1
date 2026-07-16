/*
 * M4KK1 4P1 - sead.c
 * Description: sead command - stream editor (sed/awk replacement)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

#define SEAD_OPS_MAX   64
#define SEAD_PAT_MAX   128
#define SEAD_REPL_MAX  128
#define SEAD_FIELDS_MAX 64
#define SEAD_LINE_MAX  65536
#define SEAD_VARS_MAX  26

enum sead_op_type {
    SEAD_S,       SEAD_D,        SEAD_P,
    SEAD_MATCH_D, SEAD_MATCH_P,
    SEAD_BEGIN,   SEAD_EACH,     SEAD_END,
};

struct sead_addr {
    int has;
    int is_pat;
    char pat[SEAD_PAT_MAX];
    int line;
};

struct sead_op {
    int type;
    char pattern[SEAD_PAT_MAX];
    char replacement[SEAD_REPL_MAX];
    int global;
    struct sead_addr a1, a2;
    int negate;
};

struct sead_expr_ctx {
    const char *src;
    int pos;
    int tok;
    int64_t tokval;
    char tokstr[64];
    int err;
    int (*col_cb)(void *ud, int idx);
    void *col_ud;
};

struct sead_fields {
    char *fields[SEAD_FIELDS_MAX];
    int count;
};

struct sead_ctx {
    struct sead_op each_ops[SEAD_OPS_MAX];
    int each_cnt;
    struct sead_op begin_ops[SEAD_OPS_MAX];
    int begin_cnt;
    struct sead_op end_ops[SEAD_OPS_MAX];
    int end_cnt;
    int suppress;
    int line_no;
    int64_t vars[SEAD_VARS_MAX];
    int is_block_mode;
    int debug;
    int is_csv;
    int in_place;
    char backup_ext[32];
    int range_active[SEAD_OPS_MAX];
    struct sead_fields fields;
    int64_t nf;
    char field_buf[SEAD_LINE_MAX];
};

static int sead_capture;
static char sead_out_buf[SEAD_LINE_MAX * 4];
static int sead_out_len;

static void sead_out(const char *s)
{
    if (sead_capture) {
        while (*s && sead_out_len < (int)sizeof(sead_out_buf) - 2)
            sead_out_buf[sead_out_len++] = *s++;
    } else {
        out_puts(s);
    }
}
static void sead_outc(char c)
{
    if (sead_capture) {
        if (sead_out_len < (int)sizeof(sead_out_buf) - 2)
            sead_out_buf[sead_out_len++] = c;
    } else {
        out_putc(c);
    }
}

enum {
    T_NUM, T_ID, T_STR, T_PLUS, T_MINUS, T_MUL, T_DIV, T_MOD,
    T_LT, T_GT, T_LE, T_GE, T_EQ, T_NE,
    T_AND, T_OR, T_NOT, T_LPAREN, T_RPAREN, T_COMMA, T_EOF, T_ERR
};

static int is_expr_char(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static void next_tok(struct sead_expr_ctx *ec)
{
    while (ec->src[ec->pos] == ' ' || ec->src[ec->pos] == '\t')
        ec->pos++;
    int c = ec->src[ec->pos];
    if (c == '\0' || c == '\n' || c == '\r') {
        ec->tok = T_EOF;
        return;
    }
    if (c >= '0' && c <= '9') {
        ec->tokval = 0;
        while (ec->src[ec->pos] >= '0' && ec->src[ec->pos] <= '9')
            ec->tokval = ec->tokval * 10 + (ec->src[ec->pos++] - '0');
        ec->tok = T_NUM;
        return;
    }
    if (is_expr_char(c)) {
        int p = 0;
        while (is_expr_char(ec->src[ec->pos]) && p < 63)
            ec->tokstr[p++] = ec->src[ec->pos++];
        ec->tokstr[p] = '\0';
        ec->tok = T_ID;
        return;
    }
    if (c == '"') {
        ec->pos++;
        int p = 0;
        while (ec->src[ec->pos] && ec->src[ec->pos] != '"' && p < 63) {
            if (ec->src[ec->pos] == '\\')
                ec->pos++;
            ec->tokstr[p++] = ec->src[ec->pos++];
        }
        ec->tokstr[p] = '\0';
        if (ec->src[ec->pos] == '"')
            ec->pos++;
        ec->tok = T_STR;
        return;
    }
    if (c == '+') {
        ec->pos++;
        ec->tok = T_PLUS;
        return;
    }
    if (c == '-') {
        ec->pos++;
        ec->tok = T_MINUS;
        return;
    }
    if (c == '*') {
        ec->pos++;
        ec->tok = T_MUL;
        return;
    }
    if (c == '/') {
        ec->pos++;
        ec->tok = T_DIV;
        return;
    }
    if (c == '%') {
        ec->pos++;
        ec->tok = T_MOD;
        return;
    }
    if (c == '(') {
        ec->pos++;
        ec->tok = T_LPAREN;
        return;
    }
    if (c == ')') {
        ec->pos++;
        ec->tok = T_RPAREN;
        return;
    }
    if (c == ',') {
        ec->pos++;
        ec->tok = T_COMMA;
        return;
    }
    if (c == '<' && ec->src[ec->pos + 1] == '=') {
        ec->pos += 2;
        ec->tok = T_LE;
        return;
    }
    if (c == '>' && ec->src[ec->pos + 1] == '=') {
        ec->pos += 2;
        ec->tok = T_GE;
        return;
    }
    if (c == '=' && ec->src[ec->pos + 1] == '=') {
        ec->pos += 2;
        ec->tok = T_EQ;
        return;
    }
    if (c == '!' && ec->src[ec->pos + 1] == '=') {
        ec->pos += 2;
        ec->tok = T_NE;
        return;
    }
    if (c == '<') {
        ec->pos++;
        ec->tok = T_LT;
        return;
    }
    if (c == '>') {
        ec->pos++;
        ec->tok = T_GT;
        return;
    }
    if (c == '&' && ec->src[ec->pos + 1] == '&') {
        ec->pos += 2;
        ec->tok = T_AND;
        return;
    }
    if (c == '|' && ec->src[ec->pos + 1] == '|') {
        ec->pos += 2;
        ec->tok = T_OR;
        return;
    }
    if (c == '!') {
        ec->pos++;
        ec->tok = T_NOT;
        return;
    }
    ec->tok = T_ERR;
}

static int64_t expr_parse_or(struct sead_expr_ctx *ec);
static int64_t expr_parse_primary(struct sead_expr_ctx *ec);
static void expr_next(struct sead_expr_ctx *ec)
{
    next_tok(ec);
}

static int64_t expr_parse_unary(struct sead_expr_ctx *ec)
{
    if (ec->tok == T_MINUS) {
        expr_next(ec);
        return -expr_parse_unary(ec);
    }
    if (ec->tok == T_NOT) {
        expr_next(ec);
        return !expr_parse_unary(ec);
    }
    if (ec->tok == T_LPAREN) {
        expr_next(ec);
        int64_t v = expr_parse_or(ec);
        if (ec->tok == T_RPAREN)
            expr_next(ec);
        return v;
    }
    return expr_parse_primary(ec);
}

static int64_t expr_parse_pow(struct sead_expr_ctx *ec)
{
    return expr_parse_unary(ec);
}

static int64_t expr_parse_mul(struct sead_expr_ctx *ec)
{
    int64_t v = expr_parse_pow(ec);
    while (ec->tok == T_MUL || ec->tok == T_DIV || ec->tok == T_MOD) {
        int op = ec->tok;
        expr_next(ec);
        int64_t r = expr_parse_pow(ec);
        if (op == T_MUL)
            v = v * r;
        else if (op == T_DIV) {
            if (r == 0) {
                ec->err = 1;
                return 0;
            }
            v = v / r;
        } else {
            if (r == 0) {
                ec->err = 1;
                return 0;
            }
            v = v % r;
        }
    }
    return v;
}

static int64_t expr_parse_add(struct sead_expr_ctx *ec)
{
    int64_t v = expr_parse_mul(ec);
    while (ec->tok == T_PLUS || ec->tok == T_MINUS) {
        int op = ec->tok;
        expr_next(ec);
        int64_t r = expr_parse_mul(ec);
        if (op == T_PLUS)
            v = v + r;
        else
            v = v - r;
    }
    return v;
}

static int64_t expr_parse_cmp(struct sead_expr_ctx *ec)
{
    int64_t v = expr_parse_add(ec);
    while (ec->tok == T_LT || ec->tok == T_GT || ec->tok == T_LE
           || ec->tok == T_GE || ec->tok == T_EQ || ec->tok == T_NE) {
        int op = ec->tok;
        expr_next(ec);
        int64_t r = expr_parse_add(ec);
        if (op == T_LT)
            v = (v < r);
        else if (op == T_GT)
            v = (v > r);
        else if (op == T_LE)
            v = (v <= r);
        else if (op == T_GE)
            v = (v >= r);
        else if (op == T_EQ)
            v = (v == r);
        else
            v = (v != r);
    }
    return v;
}

static int64_t expr_parse_and(struct sead_expr_ctx *ec)
{
    int64_t v = expr_parse_cmp(ec);
    while (ec->tok == T_AND) {
        expr_next(ec);
        int64_t r = expr_parse_cmp(ec);
        v = (v && r);
    }
    return v;
}

static int64_t expr_parse_or(struct sead_expr_ctx *ec)
{
    int64_t v = expr_parse_and(ec);
    while (ec->tok == T_OR) {
        expr_next(ec);
        int64_t r = expr_parse_and(ec);
        v = (v || r);
    }
    return v;
}

static int64_t expr_parse_primary(struct sead_expr_ctx *ec)
{
    if (ec->tok == T_NUM) {
        int64_t v = ec->tokval;
        expr_next(ec);
        return v;
    }
    if (ec->tok == T_STR) {
        int64_t v = (ec->tokstr[0] != '\0');
        expr_next(ec);
        return v;
    }
    if (ec->tok == T_ID) {
        char name[64];
        int i;
        for (i = 0; ec->tokstr[i]; i++)
            name[i] = ec->tokstr[i];
        name[i] = '\0';
        expr_next(ec);
        if (ec->tok == T_LPAREN) {
            expr_next(ec);
            int64_t arg = expr_parse_or(ec);
            if (ec->tok == T_COMMA) {
                expr_next(ec);
                int64_t arg2 = expr_parse_or(ec);
                (void)arg2;
                if (ec->tok == T_RPAREN)
                    expr_next(ec);
                return 0;
            }
            if (ec->tok == T_RPAREN)
                expr_next(ec);
            if (musr_strcmp(name, "col") == 0) {
                if (ec->col_cb)
                    return ec->col_cb(ec->col_ud, (int)arg);
                return 0;
            }
            if (musr_strcmp(name, "length") == 0)
                return arg;
            if (musr_strcmp(name, "abs") == 0)
                return arg < 0 ? -arg : arg;
            return 0;
        }
        if (name[0] >= 'a' && name[0] <= 'z' && name[1] == '\0')
            return ((struct sead_ctx *)ec->col_ud)->vars[name[0] - 'a'];
        return 0;
    }
    ec->err = 1;
    return 0;
}

static int64_t sead_eval_expr(struct sead_expr_ctx *ec, struct sead_ctx *ctx,
                              int (*col_cb)(void *, int), const char *s)
{
    ec->src = s;
    ec->pos = 0;
    ec->err = 0;
    ec->col_cb = col_cb;
    ec->col_ud = ctx;
    expr_next(ec);
    int64_t v = expr_parse_or(ec);
    return v;
}

static int sead_col_get(void *ud, int idx)
{
    struct sead_ctx *ctx = (struct sead_ctx *)ud;
    if (idx < 0 || idx >= ctx->fields.count)
        return 0;
    const char *f = ctx->fields.fields[idx];
    int neg = 0, v = 0;
    if (*f == '-') {
        neg = 1;
        f++;
    }
    while (*f >= '0' && *f <= '9') {
        v = v * 10 + (*f - '0');
        f++;
    }
    return neg ? -v : v;
}

struct sead_parts {
    const char *data[SEAD_OPS_MAX];
    int count;
};

static void sead_split_pipes(const char *s, struct sead_parts *pts)
{
    pts->count = 0;
    int start = 0, pos = 0;
    int in_s_delim = 0;
    char s_delim = 0;
    int s_need = 0;
    int in_brace = 0;

    while (s[pos]) {
        if (pos == start && s[pos] == 's' && (s[pos + 1] == '/'
            || s[pos + 1] == '#' || s[pos + 1] == '@'
            || s[pos + 1] == '|')) {
            s_delim = s[pos + 1];
            in_s_delim = 1;
            s_need = 2;
        }
        if (in_s_delim) {
            if (s[pos] == s_delim)
                s_need--;
            if (s_need <= 0)
                in_s_delim = 0;
        }
        if (s[pos] == '{' && !in_s_delim)
            in_brace++;
        if (s[pos] == '}' && !in_s_delim)
            in_brace--;
        if (!in_s_delim && in_brace <= 0 && s[pos] == '|'
            && s[pos + 1] == '>') {
            pts->data[pts->count++] = s + start;
            pos += 2;
            while (s[pos] == ' ')
                pos++;
            start = pos;
            continue;
        }
        pos++;
    }
    if (s[start])
        pts->data[pts->count++] = s + start;
}

static int sead_parse_addr(const char *s, int *pp, struct sead_addr *a)
{
    int p = *pp;
    while (s[p] == ' ')
        p++;
    if (s[p] == '/') {
        a->has = 1;
        a->is_pat = 1;
        p++;
        int w = 0;
        while (s[p] && s[p] != '/' && w < SEAD_PAT_MAX - 1) {
            if (s[p] == '\\' && s[p + 1] == '/') {
                a->pat[w++] = '/';
                p += 2;
            } else
                a->pat[w++] = s[p++];
        }
        a->pat[w] = '\0';
        if (s[p] == '/')
            p++;
        *pp = p;
        return 0;
    }
    if (s[p] >= '0' && s[p] <= '9') {
        a->has = 1;
        a->is_pat = 0;
        a->line = 0;
        while (s[p] >= '0' && s[p] <= '9') {
            a->line = a->line * 10 + (s[p] - '0');
            p++;
        }
        *pp = p;
        return 0;
    }
    a->has = 0;
    *pp = p;
    return 0;
}

static int sead_test_addr(struct sead_addr *a, struct sead_ctx *ctx,
                          int *range_active, int range_idx)
{
    (void)range_active;
    (void)range_idx;
    if (!a->has)
        return 1;
    if (a->is_pat) {
        int llen = musr_strlen(ctx->field_buf);
        int plen = musr_strlen(a->pat);
        for (int i = 0; i <= llen - plen; i++) {
            int m = 1;
            for (int j = 0; j < plen; j++) {
                if (a->pat[j] == '.' && ctx->field_buf[i + j] != '\n')
                    continue;
                if (ctx->field_buf[i + j] != a->pat[j]) {
                    m = 0;
                    break;
                }
            }
            if (m)
                return 1;
        }
        return 0;
    }
    return (ctx->line_no == a->line);
}

static int sead_parse_subst(const char *s, struct sead_op *op)
{
    if (s[0] != 's')
        return -1;
    if (!s[1] || s[1] == ' ' || s[1] == '\t')
        return -1;
    char delim = s[1];
    if ((delim >= 'a' && delim <= 'z') || (delim >= '0' && delim <= '9')
        || delim == ' ' || delim == '\t')
        return -1;
    int p = 2, pw = 0;
    while (s[p] && s[p] != delim && pw < SEAD_PAT_MAX - 1) {
        if (s[p] == '\\' && s[p + 1] == delim) {
            op->pattern[pw++] = delim;
            p += 2;
        } else if (s[p] == '\\' && s[p + 1] == '\\') {
            op->pattern[pw++] = '\\';
            p += 2;
        } else
            op->pattern[pw++] = s[p++];
    }
    if (s[p] != delim)
        return -1;
    op->pattern[pw] = '\0';
    p++;
    int rw = 0;
    while (s[p] && s[p] != delim && rw < SEAD_REPL_MAX - 1) {
        if (s[p] == '\\' && s[p + 1] == delim) {
            op->replacement[rw++] = delim;
            p += 2;
        } else if (s[p] == '\\' && s[p + 1] == '\\') {
            op->replacement[rw++] = '\\';
            p += 2;
        } else
            op->replacement[rw++] = s[p++];
    }
    if (s[p] == delim)
        p++;
    op->replacement[rw] = '\0';
    op->global = 0;
    while (s[p]) {
        if (s[p] == 'g')
            op->global = 1;
        p++;
    }
    op->type = SEAD_S;
    return 0;
}

static int sead_parse_op(const char *part, struct sead_op *op)
{
    int p = 0;
    while (part[p] == ' ')
        p++;
    if (!part[p] || part[p] == '#')
        return 0;

    op->a1.has = 0;
    op->a2.has = 0;
    op->negate = 0;

    struct sead_addr tmp;
    int save = p;
    if (sead_parse_addr(part, &p, &tmp) == 0 && tmp.has) {
        op->a1 = tmp;
        while (part[p] == ' ')
            p++;
        if (part[p] == ',') {
            p++;
            sead_parse_addr(part, &p, &op->a2);
            while (part[p] == ' ')
                p++;
        }
    } else {
        p = save;
    }

    while (part[p] == ' ')
        p++;

    if (!part[p] || part[p] == '#')
        return 0;

    if (part[p] == 'd' && (part[p + 1] == '\0' || part[p + 1] == ' ')) {
        op->type = SEAD_D;
        return 1;
    }
    if (part[p] == 'p' && (part[p + 1] == '\0' || part[p + 1] == ' ')) {
        op->type = SEAD_P;
        return 1;
    }
    if (part[p] == 's') {
        op->type = SEAD_S;
        return (sead_parse_subst(part + p, op) == 0) ? 1 : -1;
    }

    return -1;
}

static int sead_parse_line(const char *line, struct sead_ctx *ctx)
{
    const char *s = line;
    while (*s == ' ')
        s++;
    if (!*s || *s == '#')
        return 0;

    if (musr_strcmp(s, "BEGIN") == 0 || musr_strpref(s, "BEGIN ")) {
        const char *brace = s + 5;
        while (*brace == ' ')
            brace++;
        if (*brace != '{')
            return -1;
        const char *end = brace + 1;
        int depth = 1;
        while (*end && depth > 0) {
            if (*end == '{')
                depth++;
            if (*end == '}')
                depth--;
            if (depth > 0)
                end++;
        }
        if (depth != 0)
            return -1;
        char inner[512];
        int ip = 0;
        const char *c = brace + 1;
        while (c < end && ip < 510)
            inner[ip++] = *c++;
        inner[ip] = '\0';
        struct sead_ctx tmp_ctx;
        tmp_ctx.each_cnt = 0;
        if (sead_parse_line(inner, &tmp_ctx) != 0)
            return -1;
        for (int i = 0;
             i < tmp_ctx.each_cnt && ctx->begin_cnt < SEAD_OPS_MAX; i++) {
            ctx->begin_ops[ctx->begin_cnt++] = tmp_ctx.each_ops[i];
        }
        ctx->is_block_mode = 1;
        return 0;
    }
    if (musr_strpref(s, "EACH") && (s[4] == ' ' || s[4] == '{')) {
        const char *brace = s + 4;
        while (*brace == ' ')
            brace++;
        if (*brace != '{')
            return -1;
        const char *end = brace + 1;
        int depth = 1;
        while (*end && depth > 0) {
            if (*end == '{')
                depth++;
            if (*end == '}')
                depth--;
            if (depth > 0)
                end++;
        }
        if (depth != 0)
            return -1;
        char inner[512];
        int ip = 0;
        const char *c = brace + 1;
        while (c < end && ip < 510)
            inner[ip++] = *c++;
        inner[ip] = '\0';
        struct sead_ctx tmp_ctx;
        tmp_ctx.each_cnt = 0;
        if (sead_parse_line(inner, &tmp_ctx) != 0)
            return -1;
        for (int i = 0;
             i < tmp_ctx.each_cnt && ctx->each_cnt < SEAD_OPS_MAX; i++) {
            ctx->each_ops[ctx->each_cnt++] = tmp_ctx.each_ops[i];
        }
        ctx->is_block_mode = 1;
        return 0;
    }
    if (musr_strpref(s, "END") && (s[3] == ' ' || s[3] == '{')) {
        const char *brace = s + 3;
        while (*brace == ' ')
            brace++;
        if (*brace != '{')
            return -1;
        const char *end = brace + 1;
        int depth = 1;
        while (*end && depth > 0) {
            if (*end == '{')
                depth++;
            if (*end == '}')
                depth--;
            if (depth > 0)
                end++;
        }
        if (depth != 0)
            return -1;
        char inner[512];
        int ip = 0;
        const char *c = brace + 1;
        while (c < end && ip < 510)
            inner[ip++] = *c++;
        inner[ip] = '\0';
        struct sead_ctx tmp_ctx;
        tmp_ctx.each_cnt = 0;
        if (sead_parse_line(inner, &tmp_ctx) != 0)
            return -1;
        for (int i = 0;
             i < tmp_ctx.each_cnt && ctx->end_cnt < SEAD_OPS_MAX; i++) {
            ctx->end_ops[ctx->end_cnt++] = tmp_ctx.each_ops[i];
        }
        ctx->is_block_mode = 1;
        return 0;
    }

    struct sead_parts pts;
    sead_split_pipes(s, &pts);
    for (int pi = 0; pi < pts.count; pi++) {
        if (ctx->each_cnt >= SEAD_OPS_MAX) {
            out_puts("sead: too many ops\n");
            return -1;
        }
        struct sead_op *op = &ctx->each_ops[ctx->each_cnt];
        int r = sead_parse_op(pts.data[pi], op);
        if (r < 0) {
            out_puts("sead: bad op: ");
            out_puts(pts.data[pi]);
            out_puts("\n");
            return -1;
        }
        if (r > 0)
            ctx->each_cnt++;
    }
    return 0;
}

static int sead_match(const char *line, const char *pat)
{
    int llen = musr_strlen(line), plen = musr_strlen(pat);
    for (int i = 0; i <= llen - plen; i++) {
        int m = 1;
        for (int j = 0; j < plen; j++) {
            if (pat[j] == '.' && line[i + j] != '\n')
                continue;
            if (line[i + j] != pat[j]) {
                m = 0;
                break;
            }
        }
        if (m)
            return 1;
    }
    return 0;
}

static void sead_subst(char *line, int maxlen, const struct sead_op *op)
{
    int llen = musr_strlen(line), plen = musr_strlen(op->pattern);
    if (plen == 0)
        return;
    int start = 0;
    char result[SEAD_LINE_MAX];
    int rplen = musr_strlen(op->replacement);

    while (start <= llen - plen) {
        int m = 1;
        for (int j = 0; j < plen; j++) {
            if (op->pattern[j] == '.' && line[start + j] != '\n')
                continue;
            if (line[start + j] != op->pattern[j]) {
                m = 0;
                break;
            }
        }
        if (m) {
            int rp = 0;
            for (int i = 0; i < start && rp < maxlen - 1; i++)
                result[rp++] = line[i];
            const char *rps = op->replacement;
            while (*rps && rp < maxlen - 1)
                result[rp++] = *rps++;
            for (int i = start + plen; line[i] && rp < maxlen - 1; i++)
                result[rp++] = line[i];
            result[rp] = '\0';
            musr_strcpy(line, result);
            llen = musr_strlen(line);
            if (rplen > 0)
                start = start + rplen;
            else
                start = start + 1;
            if (!op->global)
                return;
        } else {
            start++;
        }
    }
}

static void sead_split_csv(struct sead_fields *f, char *line)
{
    f->count = 0;
    char *p = line;
    int inq = 0;
    f->fields[f->count++] = p;
    while (*p && f->count < SEAD_FIELDS_MAX) {
        if (*p == '"') {
            inq = !inq;
        } else if (*p == ',' && !inq) {
            *p = '\0';
            p++;
            f->fields[f->count++] = p;
            continue;
        }
        p++;
    }
}

static void sead_split_ws(struct sead_fields *f, char *line)
{
    f->count = 0;
    char *p = line;
    while (*p && f->count < SEAD_FIELDS_MAX) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        f->fields[f->count++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
}

static int sead_apply_op(char *line, int maxlen, struct sead_op *op,
                          struct sead_ctx *ctx, int op_idx)
{
    int range_ok = 0;
    if (op->a2.has) {
        if (ctx->range_active[op_idx]) {
            if (sead_test_addr(&op->a2, ctx, NULL, 0)) {
                ctx->range_active[op_idx] = 0;
            }
            range_ok = 1;
        } else {
            if (sead_test_addr(&op->a1, ctx, NULL, 0)) {
                ctx->range_active[op_idx] = 1;
                range_ok = 1;
                if (op->a2.is_pat && op->a2.pat[0]) {
                    if (sead_test_addr(&op->a2, ctx, NULL, 0)) {
                        ctx->range_active[op_idx] = 0;
                    }
                }
            }
        }
    } else if (op->a1.has) {
        range_ok = sead_test_addr(&op->a1, ctx, NULL, 0);
    } else {
        range_ok = 1;
    }
    if (!range_ok)
        return 0;

    switch (op->type) {
    case SEAD_S:
        sead_subst(line, maxlen, op);
        return 0;
    case SEAD_D:
        return 1;
    case SEAD_P:
        return 2;
    case SEAD_MATCH_D:
    case SEAD_MATCH_P:
        return 0;
    default:
        return 0;
    }
}

static void sead_process_line(char *line, int maxlen, struct sead_ctx *ctx)
{
    int deleted = 0;
    int printed = 0;

    if (ctx->is_csv)
        sead_split_csv(&ctx->fields, line);
    else
        sead_split_ws(&ctx->fields, line);
    ctx->nf = ctx->fields.count;

    for (int i = 0; i < ctx->each_cnt && !deleted; i++) {
        int r = sead_apply_op(line, maxlen, &ctx->each_ops[i], ctx, i);
        if (r == 1)
            deleted = 1;
        if (r == 2)
            printed = 1;
    }

    if (deleted)
        return;
    if (printed) {
        sead_out(line);
        sead_outc('\n');
        return;
    }
    if (!ctx->suppress) {
        sead_out(line);
        sead_outc('\n');
    }
}

static void sead_help(void)
{
    out_puts("sead - Script Editor Advanced\n");
    out_puts("Usage: sead [options] 'script' [file...]\n");
    out_puts("Options:\n");
    out_puts("  -e 'script'    Execute script (or pass as first arg)\n");
    out_puts("  -f 'file'      Read script from file\n");
    out_puts("  -n             Suppress automatic printing\n");
    out_puts("  -i[SUFFIX]     Edit files in-place (opt backup)\n");
    out_puts("  --csv          Interpret input as CSV\n");
    out_puts("  --debug        Show per-line debug info\n");
    out_puts("  --help         Show this help\n");
    out_puts("Operations:\n");
    out_puts("  s/old/new/g    Substitute (any delim)\n");
    out_puts("  d              Delete line\n");
    out_puts("  p              Print line\n");
    out_puts("  /pat/ d|p [!]  Delete/print matching lines\n");
    out_puts("  addr1[,addr2] action  Address ranges\n");
    out_puts("  op1 |> op2     Pipeline chains\n");
    out_puts("  BEGIN { ... }  Run before input\n");
    out_puts("  EACH { ... }   Run per line\n");
    out_puts("  END { ... }    Run after input\n");
    out_puts("Expressions: col(n), NR, NF, + - * / %% > < ==\n");
    out_puts("Examples:\n");
    out_puts("  sead 's/foo/bar/g' file.txt\n");
    out_puts("  sead '1,10 d' file.txt\n");
    out_puts("  sead '/ERROR/,/END/ d' log.txt\n");
    out_puts("  sead -i.bak 's/foo/bar/' file.txt\n");
    out_puts("  sead 'EACH { print col(1) }' --csv data.csv\n");
}

static char *sead_read_file(const char *path, int *out_len)
{
    int fd = musr_sc_open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    char buf[512];
    static char file_buf[65536];
    int len = 0, n;
    while ((n = musr_sc_read(fd, buf, 512)) > 0 && len < 65500) {
        for (int i = 0; i < n && len < 65500; i++)
            file_buf[len++] = buf[i];
    }
    musr_sc_close(fd);
    file_buf[len] = '\0';
    *out_len = len;
    return file_buf;
}

static int sead_write_file(const char *path, const char *data, int len)
{
    int fd = musr_sc_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
        return -1;
    int written = 0;
    while (written < len) {
        int chunk = len - written;
        if (chunk > 512)
            chunk = 512;
        int n = musr_sc_write(fd, data + written, chunk);
        if (n <= 0)
            break;
        written += n;
    }
    musr_sc_close(fd);
    return written;
}

static void sead_process_stream(struct sead_ctx *ctx, const char *path)
{
    char line[SEAD_LINE_MAX];
    char buf[1024];
    int is_stdin = (path == NULL);
    int fd = -1;
    char abs_path[256];

    if (!is_stdin) {
        cwd_to_abs(path, abs_path, 256);
        fd = musr_sc_open(abs_path, O_RDONLY);
        if (fd < 0) {
            c_red();
            out_puts("sead: ");
            out_puts(path);
            out_puts(": not found\n");
            c_rst();
            return;
        }
    }

    if (ctx->in_place && !is_stdin) {
        sead_capture = 1;
        sead_out_len = 0;
    }

    int li = 0, n;
    while (is_stdin
           ? ((n = musr_sc_read(0, buf, 1024)) > 0)
           : ((n = musr_sc_read(fd, buf, 1024)) > 0)) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n' || li >= SEAD_LINE_MAX - 2) {
                line[li] = '\0';
                musr_strcpy(ctx->field_buf, line);
                ctx->line_no++;
                sead_process_line(line, SEAD_LINE_MAX, ctx);
                li = 0;
            } else {
                line[li++] = buf[i];
            }
        }
    }
    if (li > 0) {
        line[li] = '\0';
        musr_strcpy(ctx->field_buf, line);
        ctx->line_no++;
        sead_process_line(line, SEAD_LINE_MAX, ctx);
    }

    if (!is_stdin)
        musr_sc_close(fd);

    if (ctx->in_place && !is_stdin && sead_out_len > 0) {
        if (ctx->backup_ext[0]) {
            char bak[512];
            int bp;
            for (bp = 0; abs_path[bp]; bp++)
                bak[bp] = abs_path[bp];
            bak[bp] = '\0';
            for (int j = 0; ctx->backup_ext[j] && bp < 510; j++)
                bak[bp++] = ctx->backup_ext[j];
            bak[bp] = '\0';
            musr_sc_rename(abs_path, bak);
        }
        sead_write_file(abs_path, sead_out_buf, sead_out_len);
    }
    sead_capture = 0;
}

/**
 * musr_cmd_sead - Stream editor (sed/awk replacement)
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_sead(int ac, char **av)
{
    struct sead_ctx ctx;
    int i;
    for (i = 0; i < SEAD_VARS_MAX; i++)
        ctx.vars[i] = 0;
    ctx.each_cnt = ctx.begin_cnt = ctx.end_cnt = 0;
    ctx.suppress = 0;
    ctx.line_no = 0;
    ctx.is_block_mode = 0;
    ctx.debug = 0;
    ctx.is_csv = 0;
    ctx.in_place = 0;
    ctx.backup_ext[0] = '\0';

    const char *script = NULL;
    const char *files[32];
    int nfiles = 0;
    int script_is_parse = 0;

    for (i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "--help") == 0) {
            sead_help();
            return;
        } else if (musr_strcmp(av[i], "-n") == 0) {
            ctx.suppress = 1;
        } else if (musr_strcmp(av[i], "--csv") == 0) {
            ctx.is_csv = 1;
        } else if (musr_strcmp(av[i], "--debug") == 0) {
            ctx.debug = 1;
        } else if (musr_strcmp(av[i], "-e") == 0) {
            if (i + 1 < ac) {
                script = av[++i];
                script_is_parse = 1;
            } else {
                out_puts("sead: -e needs arg\n");
                return;
            }
        } else if (musr_strpref(av[i], "-i")) {
            ctx.in_place = 1;
            if (av[i][2])
                musr_strcpy(ctx.backup_ext, av[i] + 2);
        } else if (musr_strpref(av[i], "-f")) {
            const char *fpath;
            if (av[i][2])
                fpath = av[i] + 2;
            else if (i + 1 < ac)
                fpath = av[++i];
            else {
                out_puts("sead: -f needs arg\n");
                return;
            }
            int len;
            char *s = sead_read_file(fpath, &len);
            if (!s) {
                out_puts("sead: can't read ");
                out_puts(fpath);
                out_puts("\n");
                return;
            }
            script = s;
            script_is_parse = 1;
        } else if (av[i][0] == '-' && av[i][1] != '\0') {
            out_puts("sead: unknown option: ");
            out_puts(av[i]);
            out_puts("\n");
            return;
        } else {
            if (!script) {
                script = av[i];
                script_is_parse = 1;
            } else {
                if (nfiles < 32)
                    files[nfiles++] = av[i];
            }
        }
    }

    if (!script && script_is_parse) {
        sead_help();
        return;
    }
    if (!script) {
        sead_help();
        return;
    }

    if (sead_parse_line(script, &ctx) != 0) {
        out_puts("sead: parse error\n");
        return;
    }

    int total_ops = ctx.each_cnt + ctx.begin_cnt + ctx.end_cnt;
    if (total_ops == 0) {
        out_puts("sead: no ops\n");
        return;
    }

    if (ctx.debug) {
        out_puts("sead: ");
        print_u32(total_ops);
        out_puts(" op(s)");
        if (ctx.begin_cnt) {
            out_puts(", ");
            print_u32(ctx.begin_cnt);
            out_puts(" BEGIN");
        }
        if (ctx.each_cnt) {
            out_puts(", ");
            print_u32(ctx.each_cnt);
            out_puts(" EACH");
        }
        if (ctx.end_cnt) {
            out_puts(", ");
            print_u32(ctx.end_cnt);
            out_puts(" END");
        }
        out_puts("\n");
    }

    sead_capture = 0;
    sead_out_len = 0;

    if (nfiles == 0) {
        sead_process_stream(&ctx, NULL);
    } else {
        for (int fi = 0; fi < nfiles; fi++) {
            ctx.line_no = 0;
            for (int ri = 0; ri < SEAD_OPS_MAX; ri++)
                ctx.range_active[ri] = 0;
            sead_process_stream(&ctx, files[fi]);
        }
    }
}
