/*
 * ==== ALTR2 syntax.c — token scanner & colored line painter ====
 * M4KK1 4P1 - usr/src/cmd/altr/syntax.c
 * Description: VSCode-Dark+ flavored highlighting for .c/.h,
 *              .asm/.S/.s, .sh, .md — keywords, strings, comments,
 *              numbers, asm registers/mnemonics, $VARS, md headers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

static const char *const c_kws[] = {
    "int", "char", "void", "return", "if", "else", "while", "for",
    "do", "switch", "case", "break", "continue", "static", "struct",
    "enum", "typedef", "const", "unsigned", "sizeof", "extern",
    "volatile", "goto", "union", "long", "short", "float", "double",
    NULL
};
static const char *const c_types[] = {
    "uint32_t", "uint8_t", "int32_t", "uint64_t", "int64_t",
    "uint16_t", "int16_t", "uintptr_t", "size_t", "u8", "u16",
    "u32", "u64", "NULL",
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

static int endswith(const char *s, const char *sfx)
{
    int sl = al_strlen(s), fl = al_strlen(sfx);
    if (fl > sl) return 0;
    for (int i = 0; i < fl; i++)
        if (s[sl - fl + i] != sfx[i]) return 0;
    return 1;
}

enum al_syn al_syn_of(const char *path)
{
    if (endswith(path, ".c") || endswith(path, ".h"))
        return SYN_C;
    if (endswith(path, ".asm") || endswith(path, ".S") ||
        endswith(path, ".s"))
        return SYN_ASM;
    if (endswith(path, ".sh"))
        return SYN_SH;
    if (endswith(path, ".md"))
        return SYN_MD;
    return SYN_NONE;
}

static int word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int in_list(const char *w, const char *const *lst)
{
    for (int i = 0; lst[i]; i++) {
        int eq = 1;
        for (int k = 0; ; k++) {
            if (w[k] != lst[i][k]) { eq = 0; break; }
            if (!w[k]) break;
        }
        if (eq) return 1;
    }
    return 0;
}

static int digit(char c)
{
    return c >= '0' && c <= '9';
}

void al_draw_syn_line(int x, int y, const char *s, int maxch,
                      enum al_syn syn)
{
    int i = 0;
    /* md header: whole line accent */
    if (syn == SYN_MD && s[0] == '#') {
        al_str_clip(x, y, s, maxch, C_SYN_KW);
        return;
    }
    while (s[i] && i < maxch) {
        /* C comment // or /* */
        if (syn == SYN_C &&
            ((s[i] == '/' && s[i + 1] == '/') ||
             (s[i] == '/' && s[i + 1] == '*'))) {
            al_str_clip(x + i * 6, y, s + i, maxch - i, C_SYN_COM);
            return;
        }
        /* sh / asm / md comment */
        if ((syn == SYN_SH || syn == SYN_MD) && s[i] == '#') {
            al_str_clip(x + i * 6, y, s + i, maxch - i, C_SYN_COM);
            return;
        }
        if (syn == SYN_ASM && s[i] == ';') {
            al_str_clip(x + i * 6, y, s + i, maxch - i, C_SYN_COM);
            return;
        }
        /* string literal (C + sh) */
        if ((syn == SYN_C || syn == SYN_SH) && s[i] == '"') {
            int j = i + 1;
            while (s[j] && s[j] != '"' && j < maxch) j++;
            if (s[j] == '"') j++;
            al_str_clip(x + i * 6, y, s + i, j - i, C_SYN_STR);
            i = j;
            continue;
        }
        /* $VAR (sh) */
        if (syn == SYN_SH && s[i] == '$') {
            int j = i + 1;
            while (word_char(s[j]) && j < maxch) j++;
            al_str_clip(x + i * 6, y, s + i, j - i, C_SYN_VAR);
            i = j;
            continue;
        }
        /* number */
        if (digit(s[i])) {
            int j = i;
            while ((digit(s[j]) || s[j] == 'x' || s[j] == 'X' ||
                    (s[j] >= 'a' && s[j] <= 'f')) && j < maxch)
                j++;
            al_str_clip(x + i * 6, y, s + i, j - i, C_SYN_NUM);
            i = j;
            continue;
        }
        /* word */
        if (word_char(s[i])) {
            int j = i;
            while (word_char(s[j]) && j < maxch) j++;
            char w[24];
            int wl = j - i;
            uint32_t col = C_FG;
            if (wl < 24) {
                for (int k = 0; k < wl; k++) w[k] = s[i + k];
                w[wl] = 0;
                if (syn == SYN_C)
                    col = in_list(w, c_kws) ? C_SYN_KW
                          : in_list(w, c_types) ? C_SYN_TYPE : C_FG;
                else if (syn == SYN_ASM)
                    col = in_list(w, asm_insns) ? C_SYN_INSN
                          : in_list(w, asm_regs) ? C_SYN_REG : C_FG;
                else if (syn == SYN_SH)
                    col = in_list(w, sh_kws) ? C_SYN_KW : C_FG;
            }
            al_str_clip(x + i * 6, y, s + i, j - i, col);
            i = j;
            continue;
        }
        al_char(x + i * 6, y, s[i], C_FG);
        i++;
    }
}
