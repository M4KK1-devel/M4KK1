/*
 * M4KK1 4P1 - calc.c
 * Description: calc command - full bc-replacement calculator
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"
#include <stdint.h>

static int64_t __divdi3(int64_t a, int64_t b)
{
    if (b == 0)
        return 0;
    int na = 0, nb = 0;
    uint64_t ua, ub;
    if (a < 0) {
        na = 1;
        ua = (uint64_t)(-(a + 1)) + 1;
    } else
        ua = (uint64_t)a;
    if (b < 0) {
        nb = 1;
        ub = (uint64_t)(-(b + 1)) + 1;
    } else
        ub = (uint64_t)b;
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((ua >> i) & 1);
        if (r >= ub) {
            r -= ub;
            q |= (1ULL << i);
        }
    }
    return (na ^ nb) ? -(int64_t)q : (int64_t)q;
}
static int64_t __moddi3(int64_t a, int64_t b)
{
    if (b == 0)
        return 0;
    int na = 0;
    uint64_t ua, ub;
    if (a < 0) {
        na = 1;
        ua = (uint64_t)(-(a + 1)) + 1;
    } else
        ua = (uint64_t)a;
    if (b < 0) {
        ub = (uint64_t)(-(b + 1)) + 1;
    } else
        ub = (uint64_t)b;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((ua >> i) & 1);
        if (r >= ub)
            r -= ub;
    }
    return na ? -(int64_t)r : (int64_t)r;
}
static uint64_t __udivdi3(uint64_t a, uint64_t b)
{
    if (b == 0)
        return 0;
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
            q |= (1ULL << i);
        }
    }
    return q;
}
static uint64_t __umoddi3(uint64_t a, uint64_t b)
{
    if (b == 0)
        return 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b)
            r -= b;
    }
    return r;
}

static int calc_scale = 0;
static int64_t vars[27];
static int var_scale[27];

static int64_t pow10i(int n)
{
    int64_t r = 1;
    for (int i = 0; i < n; i++) {
        int64_t nr = r * 10;
        if (nr / 10 != r)
            return r;
        r = nr;
    }
    return r;
}

static void print_s64(int64_t v)
{
    if (v == 0) {
        out_putc('0');
        return;
    }
    if (v < 0) {
        out_putc('-');
        v = -v;
    }
    char b[24];
    int p = 0;
    while (v > 0) {
        b[p++] = '0' + (char)(v % 10);
        v /= 10;
    }
    while (p > 0)
        out_putc(b[--p]);
}

static void print_num_scale(int64_t v, int s)
{
    if (s <= 0) {
        print_s64(v);
        return;
    }
    int sign = 0;
    if (v < 0) {
        sign = 1;
        v = -v;
    }
    int64_t d = pow10i(s);
    if (d == 0) {
        print_s64(sign ? -v : v);
        return;
    }
    int64_t ip = v / d;
    int64_t fp = v % d;
    if (sign)
        out_putc('-');
    print_s64(ip);
    out_putc('.');
    int nd = 0;
    int64_t t = fp;
    do {
        nd++;
        t /= 10;
    } while (t > 0);
    for (int i = nd; i < s; i++)
        out_putc('0');
    if (fp != 0)
        print_s64(fp);
    else
        for (int i = 0; i < s; i++)
            out_putc('0');
}

static uint32_t f32_as_u32(float f)
{
    uint32_t u;
    char *fp = (char *)&f, *up = (char *)&u;
    for (int i = 0; i < 4; i++)
        up[i] = fp[i];
    return u;
}
static float u32_as_f32(uint32_t u)
{
    float f;
    char *fp = (char *)&f, *up = (char *)&u;
    for (int i = 0; i < 4; i++)
        fp[i] = up[i];
    return f;
}
static float Q_rsqrt(float number)
{
    float x2 = number * 0.5f;
    float y = number;
    uint32_t i = f32_as_u32(y);
    i = 0x5f3759df - (i >> 1);
    y = u32_as_f32(i);
    y = y * (1.5f - (x2 * y * y));
    return y;
}
static float Q_sqrt(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    return 1.0f / Q_rsqrt(x);
}
static float pow10f(int n)
{
    float r = 1.0f;
    for (int i = 0; i < n; i++)
        r *= 10.0f;
    return r;
}

static int64_t f2s(float f, int s)
{
    return (int64_t)(f * pow10f(s) + 0.5f);
}
static float s2f(int64_t v, int s)
{
    return (float)v / pow10f(s);
}

static int64_t calc_sin(int64_t deg, int s)
{
    int64_t p = pow10i(s);
    int64_t full = 360 * p;
    deg = deg % full;
    if (deg < 0)
        deg += full;
    int neg = 0;
    if (deg > 180 * p) {
        neg = 1;
        deg -= 180 * p;
    }
    if (deg > 90 * p)
        deg = 180 * p - deg;
    float rad = s2f(deg, s) * 3.141592653589793f / 180.0f;
    float term = rad, result = rad;
    for (int k = 1; k < 8; k++) {
        term = -term * rad * rad / ((float)(2 * k) * (float)(2 * k + 1));
        result += term;
        float abs_term = term < 0 ? -term : term;
        if (abs_term < 0.5f / pow10f(s))
            break;
    }
    int64_t r = f2s(result, s);
    return neg ? -r : r;
}
static int64_t calc_cos(int64_t deg, int s)
{
    int64_t p = pow10i(s);
    return calc_sin(deg + 90 * p, s);
}
static int64_t calc_exp(int64_t x, int s)
{
    float fx = s2f(x, s);
    float term = 1.0f, result = 1.0f;
    for (int k = 1; k < 15; k++) {
        term = term * fx / (float)k;
        result += term;
        if (term < 0) {
            if (-term < 0.5f / pow10f(s))
                break;
        } else {
            if (term < 0.5f / pow10f(s))
                break;
        }
    }
    return f2s(result, s);
}
static int64_t calc_log(int64_t x, int s)
{
    if (x <= 0)
        return 0;
    float fx = s2f(x, s);
    float z = (fx - 1.0f) / (fx + 1.0f);
    float term = z, result = z;
    for (int k = 1; k < 20; k++) {
        term = term * z * z * (float)(2 * k - 1) / (float)(2 * k + 1);
        result += term;
        float abs_term = term < 0 ? -term : term;
        if (abs_term < 0.5f / pow10f(s))
            break;
    }
    return f2s(result * 2.0f, s);
}
static int64_t calc_powi(int64_t x, int64_t y, int s)
{
    if (y < 0 || y > 100)
        return 0;
    if (y == 0)
        return pow10i(s);
    int64_t p = pow10i(s), r = p;
    for (int64_t i = 0; i < y; i++)
        r = (r * x) / p;
    return r;
}

static int safe_mul_overflow(int64_t a, int64_t b)
{
    if (a == 0 || b == 0)
        return 0;
    if (a > 0) {
        if (b > 0)
            return a > INT64_MAX / b;
        return b < (-INT64_MAX - 1) / a;
    }
    if (b > 0)
        return a < (-INT64_MAX - 1) / b;
    return a < INT64_MAX / b;
}

enum {
    T_EOF, T_NUM, T_VAR, T_PLUS, T_MINUS, T_MUL, T_DIV, T_MOD, T_POW,
    T_LPAREN, T_RPAREN, T_EQ, T_SQRT, T_ABS, T_SIN, T_COS, T_EXP, T_LOG,
    T_ANS, T_SEMI, T_COMMA
};

typedef struct {
    int type;
    int64_t nval;
    int vidx;
} token_t;

static int is_ws(int c)
{
    return c == ' ' || c == '\t';
}

static void next_token(const char *s, int *pos, token_t *t)
{
    while (is_ws(s[*pos]))
        (*pos)++;
    if (s[*pos] == '#' || s[*pos] == '\0' || s[*pos] == '\n'
        || s[*pos] == '\r') {
        t->type = T_EOF;
        return;
    }
    if (s[*pos] >= '0' && s[*pos] <= '9') {
        int64_t iv = 0;
        while (s[*pos] >= '0' && s[*pos] <= '9') {
            iv = iv * 10 + (s[*pos] - '0');
            (*pos)++;
        }
        if (s[*pos] == '.') {
            (*pos)++;
            int64_t fv = 0;
            int fd = 0;
            while (s[*pos] >= '0' && s[*pos] <= '9') {
                fv = fv * 10 + (s[*pos] - '0');
                (*pos)++;
                fd++;
            }
            int64_t p = pow10i(calc_scale);
            int64_t fp = pow10i(fd);
            t->nval = iv * p + (fv * p) / fp;
        } else {
            t->nval = iv * pow10i(calc_scale);
        }
        t->type = T_NUM;
        return;
    }
    if (s[*pos] == '.') {
        int p0 = *pos;
        (*pos)++;
        int64_t fv = 0;
        int fd = 0;
        while (s[*pos] >= '0' && s[*pos] <= '9') {
            fv = fv * 10 + (s[*pos] - '0');
            (*pos)++;
            fd++;
        }
        if (fd > 0) {
            int64_t p = pow10i(calc_scale);
            int64_t fp = pow10i(fd);
            t->nval = (fv * p) / fp;
            t->type = T_NUM;
            return;
        }
        *pos = p0;
        t->type = T_EOF;
        return;
    }
    if (s[*pos] >= 'a' && s[*pos] <= 'z') {
        int start = *pos;
        while ((s[*pos] >= 'a' && s[*pos] <= 'z'))
            (*pos)++;
        int len = *pos - start;
        if (len == 4 && s[start] == 's' && s[start + 1] == 'q'
            && s[start + 2] == 'r' && s[start + 3] == 't') {
            t->type = T_SQRT;
            return;
        }
        if (len == 3 && s[start] == 'a' && s[start + 1] == 'b'
            && s[start + 2] == 's') {
            t->type = T_ABS;
            return;
        }
        if (len == 3 && s[start] == 's' && s[start + 1] == 'i'
            && s[start + 2] == 'n') {
            t->type = T_SIN;
            return;
        }
        if (len == 3 && s[start] == 'c' && s[start + 1] == 'o'
            && s[start + 2] == 's') {
            t->type = T_COS;
            return;
        }
        if (len == 3 && s[start] == 'e' && s[start + 1] == 'x'
            && s[start + 2] == 'p') {
            t->type = T_EXP;
            return;
        }
        if (len == 3 && s[start] == 'l' && s[start + 1] == 'o'
            && s[start + 2] == 'g') {
            t->type = T_LOG;
            return;
        }
        if (len == 3 && s[start] == 'a' && s[start + 1] == 'n'
            && s[start + 2] == 's') {
            t->type = T_ANS;
            return;
        }
        t->type = T_VAR;
        t->vidx = s[start] - 'a';
        return;
    }
    if (s[*pos] == '+') {
        t->type = T_PLUS;
        (*pos)++;
        return;
    }
    if (s[*pos] == '-') {
        t->type = T_MINUS;
        (*pos)++;
        return;
    }
    if (s[*pos] == '*') {
        t->type = T_MUL;
        (*pos)++;
        return;
    }
    if (s[*pos] == '/') {
        t->type = T_DIV;
        (*pos)++;
        return;
    }
    if (s[*pos] == '%') {
        t->type = T_MOD;
        (*pos)++;
        return;
    }
    if (s[*pos] == '^') {
        t->type = T_POW;
        (*pos)++;
        return;
    }
    if (s[*pos] == '(') {
        t->type = T_LPAREN;
        (*pos)++;
        return;
    }
    if (s[*pos] == ')') {
        t->type = T_RPAREN;
        (*pos)++;
        return;
    }
    if (s[*pos] == '=') {
        t->type = T_EQ;
        (*pos)++;
        return;
    }
    if (s[*pos] == ';') {
        t->type = T_SEMI;
        (*pos)++;
        return;
    }
    if (s[*pos] == ',') {
        t->type = T_COMMA;
        (*pos)++;
        return;
    }
    t->type = T_EOF;
}

static token_t cur;
static int cpos;
static const char *cstr;
static int eval_error;

static void advance(void)
{
    next_token(cstr, &cpos, &cur);
}

static void syntax_err(void)
{
    out_puts("calc: syntax error near '");
    for (int i = 0; cstr[cpos + i] && cstr[cpos + i] != '\n' && i < 20;
         i++)
        out_putc(cstr[cpos + i]);
    out_puts("'\n");
    eval_error = 1;
}

static int64_t parse_expr(void);
static int64_t parse_power(void);
static int64_t parse_unary(void);
static int64_t parse_primary(void);

static int64_t parse_term(void)
{
    int64_t v = parse_power();
    while (cur.type == T_MUL || cur.type == T_DIV || cur.type == T_MOD) {
        int op = cur.type;
        advance();
        int64_t r = parse_power();
        if (op == T_MUL) {
            int64_t p = pow10i(calc_scale);
            if (safe_mul_overflow(v, r)) {
                out_puts("calc: overflow\n");
                eval_error = 1;
                v = 0;
                break;
            }
            v = (v * r) / p;
        } else if (op == T_DIV) {
            if (r == 0) {
                out_puts("calc: division by zero\n");
                eval_error = 1;
                v = 0;
            } else {
                int64_t p = pow10i(calc_scale);
                v = (v * p) / r;
            }
        } else {
            if (r == 0) {
                out_puts("calc: mod by zero\n");
                eval_error = 1;
                v = 0;
            } else
                v = v % r;
        }
    }
    return v;
}

static int64_t parse_expr(void)
{
    int64_t v = parse_term();
    while (cur.type == T_PLUS || cur.type == T_MINUS) {
        int op = cur.type;
        advance();
        int64_t r = parse_term();
        if (op == T_PLUS)
            v = v + r;
        else
            v = v - r;
    }
    return v;
}

static int64_t parse_power(void)
{
    int64_t v = parse_unary();
    if (cur.type == T_POW) {
        advance();
        int64_t e = parse_unary();
        int s = calc_scale;
        v = calc_powi(v, e / pow10i(s), s);
    }
    return v;
}

static int64_t parse_unary(void)
{
    if (cur.type == T_MINUS) {
        advance();
        return -parse_unary();
    }
    if (cur.type == T_PLUS) {
        advance();
        return parse_unary();
    }
    return parse_primary();
}

static int64_t isqrt64(int64_t n)
{
    if (n <= 1)
        return n;
    int64_t x = n, y;
    do {
        y = x;
        x = (x + n / x) / 2;
    } while (x < y);
    return y;
}

static int64_t parse_primary(void)
{
    if (cur.type == T_NUM) {
        int64_t v = cur.nval;
        advance();
        return v;
    }
    if (cur.type == T_ANS) {
        advance();
        return vars[26];
    }
    if (cur.type == T_VAR) {
        int idx = cur.vidx;
        advance();
        if (cur.type == T_EQ) {
            advance();
            int64_t v = parse_expr();
            vars[idx] = v;
            var_scale[idx] = calc_scale;
            return v;
        }
        return vars[idx];
    }
    if (cur.type == T_LPAREN) {
        advance();
        int64_t v = parse_expr();
        if (cur.type != T_RPAREN) {
            out_puts("calc: missing ')'\n");
            eval_error = 1;
        } else
            advance();
        return v;
    }
    if (cur.type == T_SQRT || cur.type == T_ABS || cur.type == T_SIN
        || cur.type == T_COS || cur.type == T_EXP || cur.type == T_LOG) {
        int fn = cur.type;
        advance();
        int got_paren = 0;
        if (cur.type == T_LPAREN) {
            advance();
            got_paren = 1;
        }
        int64_t v = parse_expr();
        if (got_paren && cur.type == T_RPAREN)
            advance();
        int s = calc_scale;
        switch (fn) {
        case T_SQRT: {
            float fv = s2f(v, s);
            float fr = Q_sqrt(fv);
            return f2s(fr, s);
        }
        case T_ABS:
            return v < 0 ? -v : v;
        case T_SIN:
            return calc_sin(v, s);
        case T_COS:
            return calc_cos(v, s);
        case T_EXP:
            return calc_exp(v, s);
        case T_LOG:
            return calc_log(v, s);
        }
    }
    syntax_err();
    return 0;
}

static int64_t eval_expr(const char *s)
{
    cstr = s;
    cpos = 0;
    eval_error = 0;
    advance();
    int64_t v = parse_expr();
    return v;
}

static int64_t eval_line(const char *s)
{
    int64_t v = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s || *s == '#')
            break;
        v = eval_expr(s);
        while (*s && *s != ';' && *s != '\n')
            s++;
        if (*s == ';')
            s++;
    }
    return v;
}

static int read_line(char *buf, int max)
{
    int pos = 0;
    while (pos < max - 1) {
        int c = ser_getc();
        if (c < 0)
            continue;
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            ser_puts("\r\n");
            return pos;
        }
        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                ser_puts("\b \b");
            }
            continue;
        }
        buf[pos++] = (char)c;
        ser_putc((char)c);
    }
    buf[pos] = '\0';
    return pos;
}

static int read_file(const char *path, char *buf, int max)
{
    char ap[256];
    cwd_to_abs(path, ap, 256);
    int fd = musr_sc_open(ap, O_RDONLY);
    if (fd < 0)
        return -1;
    int len = 0, n;
    char tmp[64];
    while ((n = musr_sc_read(fd, tmp, 64)) > 0 && len < max - 1) {
        for (int i = 0; i < n && len < max - 1; i++)
            buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    musr_sc_close(fd);
    return len;
}

static void print_help(void)
{
    out_puts("calc - M4KK1 Calculator (bc superset)\r\n");
    out_puts("Usage:  calc [expression]\r\n");
    out_puts("        calc              (interactive mode)\r\n");
    out_puts("        calc -f file      (run script from file)\r\n");
    out_puts("\r\n");
    out_puts("Expressions:\r\n");
    out_puts("  + - * / %% ^    Arithmetic\r\n");
    out_puts("  -expr          Unary negation\r\n");
    out_puts("  (expr)         Grouping\r\n");
    out_puts("  expr; expr     Multiple expressions (prints last)\r\n");
    out_puts("  .5  3.14  5.   Decimal input\r\n");
    out_puts("\r\n");
    out_puts("Variables:\r\n");
    out_puts("  a-z = expr     Assign variable\r\n");
    out_puts("  ans            Last result\r\n");
    out_puts("  scale = N      Set decimal precision (0-10, default 0)\r\n");
    out_puts("\r\n");
    out_puts("Functions:\r\n");
    out_puts("  sqrt(x)        Square root (Carmack Q_rsqrt)\r\n");
    out_puts("  sin(x) cos(x)  Trigonometry (degrees)\r\n");
    out_puts("  exp(x)         e^x\r\n");
    out_puts("  log(x)         Natural logarithm\r\n");
    out_puts("  abs(x)         Absolute value\r\n");
    out_puts("\r\n");
    out_puts("Interactive commands:\r\n");
    out_puts("  help           Show this help\r\n");
    out_puts("  quit           Exit calculator\r\n");
    out_puts("Comments start with #\r\n");
}

/**
 * musr_cmd_calc - Calculator expression evaluator
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_calc(int ac, char **av)
{
    calc_scale = 0;
    for (int i = 0; i < 27; i++) {
        vars[i] = 0;
        var_scale[i] = 0;
    }
    int file_mode = 0;
    const char *file_path = NULL;
    int64_t ans_val = 0;

    for (int i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "-f") == 0) {
            if (i + 1 < ac) {
                file_path = av[++i];
                file_mode = 1;
            } else {
                out_puts("calc: -f needs arg\n");
                return;
            }
        }
    }

    if (file_mode) {
        char script[4096];
        int len = read_file(file_path, script, 4096);
        if (len < 0) {
            out_puts("calc: can't read ");
            out_puts(file_path);
            out_puts("\n");
            return;
        }
        int pos = 0;
        while (pos < len) {
            while (pos < len
                   && (script[pos] == '\n' || script[pos] == '\r'))
                pos++;
            if (pos >= len || script[pos] == '#') {
                while (pos < len && script[pos] != '\n')
                    pos++;
                continue;
            }
            char line[256];
            int lp = 0;
            while (pos < len && script[pos] != '\n'
                   && script[pos] != '\r' && lp < 255)
                line[lp++] = script[pos++];
            line[lp] = '\0';
            if (line[0] == 's' && line[1] == 'c' && line[2] == 'a'
                && line[3] == 'l' && line[4] == 'e') {
                int p = 5;
                while (is_ws(line[p]))
                    p++;
                if (line[p] == '=') {
                    p++;
                    while (is_ws(line[p]))
                        p++;
                    int sv = 0;
                    while (line[p] >= '0' && line[p] <= '9') {
                        sv = sv * 10 + (line[p] - '0');
                        p++;
                    }
                    if (sv >= 0 && sv <= 10)
                        calc_scale = sv;
                }
                continue;
            }
            if (line[0] >= 'a' && line[0] <= 'z' && line[1] == '='
                && lp > 2) {
                int idx = line[0] - 'a';
                int64_t v = eval_line(line + 2);
                if (!eval_error) {
                    vars[idx] = v;
                    var_scale[idx] = calc_scale;
                    ans_val = v;
                }
                continue;
            }
            int64_t v = eval_line(line);
            if (!eval_error) {
                ans_val = v;
                print_num_scale(v, calc_scale);
                out_puts("\n");
            }
        }
        if (eval_error)
            return;
        return;
    }

    if (ac >= 2 && !file_mode) {
        char buf[1024];
        int bp = 0;
        for (int i = 1; i < ac; i++) {
            const char *a = av[i];
            while (*a && bp < 1023)
                buf[bp++] = *a++;
            if (i < ac - 1 && bp < 1023)
                buf[bp++] = ' ';
        }
        buf[bp] = '\0';

        if (buf[0] >= 'a' && buf[0] <= 'z' && buf[1] == '\0') {
            print_num_scale(vars[buf[0] - 'a'],
                            var_scale[buf[0] - 'a']);
            out_puts("\n");
            return;
        }
        if (musr_strcmp(buf, "scale") == 0) {
            print_s64(calc_scale);
            out_puts("\n");
            return;
        }
        if (musr_strcmp(buf, "ans") == 0) {
            print_num_scale(vars[26], var_scale[26]);
            out_puts("\n");
            return;
        }

        if (buf[0] == 's' && buf[1] == 'c' && buf[2] == 'a'
            && buf[3] == 'l' && buf[4] == 'e' && buf[5] == '=') {
            int sv = 0;
            int p = 6;
            while (buf[p] >= '0' && buf[p] <= '9') {
                sv = sv * 10 + (buf[p] - '0');
                p++;
            }
            if (sv >= 0 && sv <= 10)
                calc_scale = sv;
            return;
        }

        int64_t v = eval_line(buf);
        if (eval_error)
            return;
        vars[26] = v;
        var_scale[26] = calc_scale;
        print_num_scale(v, calc_scale);
        out_puts("\n");
        return;
    }

    char line[256];
    out_puts("M4KK1 Calculator v2.0  (type 'help' for commands)\r\n");
    for (;;) {
        out_puts("calc> ");
        int n = read_line(line, 256);
        if (n == 0)
            continue;
        if (musr_strcmp(line, "quit") == 0
            || musr_strcmp(line, "exit") == 0)
            break;
        if (musr_strcmp(line, "help") == 0) {
            print_help();
            continue;
        }

        if (musr_strcmp(line, "scale") == 0) {
            print_s64(calc_scale);
            out_puts("\r\n");
            continue;
        }

        if (line[0] >= 'a' && line[0] <= 'z' && line[1] == '\0') {
            int idx = line[0] - 'a';
            print_num_scale(vars[idx], var_scale[idx]);
            out_puts("\r\n");
            continue;
        }
        if (musr_strcmp(line, "ans") == 0) {
            print_num_scale(vars[26], var_scale[26]);
            out_puts("\r\n");
            continue;
        }

        if (line[0] == 's' && line[1] == 'c' && line[2] == 'a'
            && line[3] == 'l' && line[4] == 'e') {
            int p = 5;
            while (is_ws(line[p]))
                p++;
            if (line[p] == '=') {
                p++;
                while (is_ws(line[p]))
                    p++;
                int sv = 0;
                while (line[p] >= '0' && line[p] <= '9') {
                    sv = sv * 10 + (line[p] - '0');
                    p++;
                }
                if (sv >= 0 && sv <= 10)
                    calc_scale = sv;
                else
                    out_puts("calc: scale 0-10\r\n");
                continue;
            }
        }

        int64_t v = eval_line(line);
        if (eval_error)
            continue;
        vars[26] = v;
        var_scale[26] = calc_scale;
        print_num_scale(v, calc_scale);
        out_puts("\r\n");
    }
}
