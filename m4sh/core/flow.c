/*
 * M4KK1 4P1 - flow.c
 * Description: shell flow-control builtins — eval, shift, trap
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/* ── Positional parameters ($1..$9) ───────────────────────────
 * m4sh is a single-process shell with no argv ABI: the parameters
 * are seeded by musr_set_args() (shell argv, script line fields)
 * and shifted left by `shift`.  expand_vars() resolves $1..$9
 * from this table (see musr_pos_param() below). */

char musr_pos_args[9][128];
static int pos_arg_cnt;

void musr_set_args(int ac, char **av)
{
    int i;

    for (i = 0; i < 9; i++)
        musr_pos_args[i][0] = '\0';
    pos_arg_cnt = 0;
    for (i = 1; i < ac && pos_arg_cnt < 9; i++) {
        musr_strncpy(musr_pos_args[pos_arg_cnt], av[i], 127);
        musr_pos_args[pos_arg_cnt][127] = '\0';
        pos_arg_cnt++;
    }
    /* export the first 9 as environment-visible $1..$9 style */
}

int musr_pos_param(int n, char *out, int osize)
{
    if (n < 1 || n > pos_arg_cnt) {
        out[0] = '\0';
        return 0;
    }
    musr_strncpy(out, musr_pos_args[n - 1], osize - 1);
    out[osize - 1] = '\0';
    return 1;
}

/* ── trap: shell-level signal interception ────────────────────
 * The M4KK1 kernel signals (sys/src/kernel/signal.c) are internal
 * only — there is no user-space handler registration.  trap()
 * therefore works at the shell layer: Ctrl-C (0x03) is dispatched
 * through musr_trap_fire("INT") by the line editor, and the at-job
 * scheduler calls musr_trap_fire("EXIT") on shell exit. */

struct musr_trap {
    char cmd[256];
    int  armed;
};
static struct musr_trap traps[4];   /* INT TERM EXIT ERR */

static int trap_idx(const char *name)
{
    if (musr_strcmp(name, "INT") == 0)  return 0;
    if (musr_strcmp(name, "TERM") == 0) return 1;
    if (musr_strcmp(name, "EXIT") == 0) return 2;
    if (musr_strcmp(name, "ERR") == 0)  return 3;
    return -1;
}

static const char *trap_name(int i)
{
    static const char *names[4] = {"INT", "TERM", "EXIT", "ERR"};
    return names[i];
}

void musr_trap_fire(const char *name)
{
    int i = trap_idx(name);

    if (i < 0 || !traps[i].armed)
        return;
    /* run the trap command through the full command engine */
    musr_exec_line(traps[i].cmd);
}

void musr_trap_on_exit(void)
{
    musr_trap_fire("EXIT");
}

/**
 * musr_cmd_trap - set or list signal traps
 * @ac: argument count
 * @av: argument vector
 *
 *   trap              — list armed traps
 *   trap CMD SIG...   — run CMD when SIG fires
 *   trap - SIG        — clear the trap for SIG
 */
void musr_cmd_trap(int ac, char **av)
{
    /* list */
    if (ac < 2) {
        int any = 0;
        for (int i = 0; i < 4; i++) {
            if (traps[i].armed) {
                any = 1;
                c_cyn();
                out_puts("trap -- '");
                c_wht();
                out_puts(traps[i].cmd);
                c_cyn();
                out_puts("' ");
                out_puts(trap_name(i));
                out_puts("\n");
            }
        }
        if (!any)
            out_puts("trap: no traps set\n");
        c_rst();
        return;
    }

    const char *cmd = av[1];

    /* each remaining arg is a signal name */
    for (int a = 2; a < ac; a++) {
        int i = trap_idx(av[a]);
        if (i < 0) {
            c_red();
            out_puts("trap: unknown signal: ");
            out_puts(av[a]);
            out_puts("\n");
            c_rst();
            last_exit_code = 2;
            return;
        }
        if (musr_strcmp(cmd, "-") == 0) {
            traps[i].armed = 0;
        } else {
            musr_strncpy(traps[i].cmd, cmd, sizeof(traps[i].cmd) - 1);
            traps[i].cmd[sizeof(traps[i].cmd) - 1] = '\0';
            traps[i].armed = 1;
        }
    }
}

/**
 * musr_cmd_shift - shift positional parameters left
 * @ac: argument count
 * @av: argument vector
 *
 *   shift       — drop $1, $2 becomes $1, ...
 *   shift N     — drop the first N parameters
 */
void musr_cmd_shift(int ac, char **av)
{
    int n = 1;

    if (ac > 1)
        n = musr_atoi(av[1]);
    if (n <= 0) {
        c_red();
        out_puts("shift: invalid count\n");
        c_rst();
        last_exit_code = 2;
        return;
    }
    if (n >= pos_arg_cnt) {
        /* everything shifts away */
        for (int i = 0; i < 9; i++)
            musr_pos_args[i][0] = '\0';
        pos_arg_cnt = 0;
        return;
    }
    for (int i = 0; i + n < 9; i++) {
        if (i + n < pos_arg_cnt)
            musr_strncpy(musr_pos_args[i], musr_pos_args[i + n], 127);
        else
            musr_pos_args[i][0] = '\0';
    }
    pos_arg_cnt -= n;
}

/**
 * musr_cmd_eval - concatenate args and execute as a command line
 * @ac: argument count
 * @av: argument vector
 *
 * eval "echo hello" runs echo hello through the full engine
 * (variable expansion, pipes, ;, &&, || all supported).
 */
void musr_cmd_eval(int ac, char **av)
{
    char line[512];
    int i = 1, j = 0;

    if (ac < 2)
        return;
    /* plain char-by-char join (avoids PCC arithmetic quirks) */
    while (i < ac && j < (int)sizeof(line) - 1) {
        int k = 0;
        while (av[i][k] && j < (int)sizeof(line) - 1)
            line[j++] = av[i][k++];
        i++;
        if (i < ac && j < (int)sizeof(line) - 1)
            line[j++] = ' ';
    }
    line[j] = '\0';
    musr_exec_line(line);
}
