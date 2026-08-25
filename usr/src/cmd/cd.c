/*
 * M4KK1 4P1 - cd.c
 * Description: cd command - change directory (Zsh-style)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/* Look up an environment variable in the envars[] table.
 * Returns the value pointer or NULL. */
static const char *env_lookup(const char *name)
{
    extern char envars[16][128];
    extern int envar_cnt;

    for (int i = 0; i < envar_cnt; i++) {
        char *eq;
        for (eq = envars[i]; *eq && *eq != '='; eq++)
            ;
        int nlen = (int)(eq - envars[i]);
        int mlen = musr_strlen(name);
        if (nlen == mlen && *eq == '=') {
            int match = 1;
            for (int c = 0; c < nlen; c++)
                if (name[c] != envars[i][c]) {
                    match = 0;
                    break;
                }
            if (match)
                return eq + 1;
        }
    }
    return 0;
}

/* Store/overwrite NAME=VALUE in the envars[] table. */
static void env_store(const char *name, const char *value)
{
    extern char envars[16][128];
    extern int envar_cnt;

    for (int i = 0; i < envar_cnt; i++) {
        char *eq;
        for (eq = envars[i]; *eq && *eq != '='; eq++)
            ;
        int nlen = (int)(eq - envars[i]);
        if (nlen == musr_strlen(name) && *eq == '=') {
            int match = 1;
            for (int c = 0; c < nlen; c++)
                if (name[c] != envars[i][c]) {
                    match = 0;
                    break;
                }
            if (match) {
                int vi = nlen + 1;
                int si = 0;
                while (value[si] && vi < 127)
                    envars[i][vi++] = value[si++];
                envars[i][vi] = '\0';
                return;
            }
        }
    }
    if (envar_cnt >= 16)
        return;
    int pos = 0;
    while (*name && pos < 100)
        envars[envar_cnt][pos++] = *name++;
    envars[envar_cnt][pos++] = '=';
    while (*value && pos < 127)
        envars[envar_cnt][pos++] = *value++;
    envars[envar_cnt][pos] = '\0';
    envar_cnt++;
}

/**
 * musr_cmd_cd - change the current working directory (Zsh style)
 * @ac: argument count
 * @av: argument vector
 *
 *   cd           -> $HOME (fallback /root for uid 0)
 *   cd -         -> $OLDPWD (prints the new dir, Zsh behaviour)
 *   cd ~         -> $HOME
 *   cd ~user     -> home of `user` from /export/cfg/passwd.db
 *   cd dir       -> dir
 */
void musr_cmd_cd(int ac, char **av)
{
    char target[256];
    char old[256];
    const char *arg = (ac > 1) ? av[1] : 0;

    /* remember the current dir for OLDPWD / cd - */
    musr_strncpy(old, cwd, sizeof(old) - 1);
    old[sizeof(old) - 1] = '\0';

    if (!arg || !*arg || musr_strcmp(arg, "~") == 0) {
        /* no argument or ~: home directory */
        const char *home = env_lookup("HOME");
        if (!home || !*home)
            home = (m4k_geteuid() == 0) ? "/root" : "/export/home";
        musr_strncpy(target, home, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    } else if (musr_strcmp(arg, "-") == 0) {
        /* cd -: previous directory ($OLDPWD) */
        const char *oldpw = env_lookup("OLDPWD");
        if (!oldpw || !*oldpw) {
            c_red();
            out_puts("cd: OLDPWD not set\n");
            c_rst();
            last_exit_code = 1;
            return;
        }
        musr_strncpy(target, oldpw, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        /* Zsh prints the resulting directory */
        c_cyn();
        out_puts(target);
        out_puts("\n");
        c_rst();
    } else if (arg[0] == '~' && arg[1]) {
        /* ~user: look up the user's home in the password db */
        char username[64];
        int u = 0;
        const char *p = arg + 1;

        while (*p && *p != '/' && u < 63)
            username[u++] = *p++;
        username[u] = '\0';

        passwd_entry_t pw;
        if (musr_getpwnam(username, &pw) != 0 || !pw.home[0]) {
            c_red();
            out_puts("cd: no such user: ");
            out_puts(username);
            out_puts("\n");
            c_rst();
            last_exit_code = 1;
            return;
        }
        musr_strncpy(target, pw.home, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        int tl = musr_strlen(target);
        if (*p == '/' && tl < (int)sizeof(target) - 1) {
            musr_strncpy(target + tl, p, sizeof(target) - 1 - tl);
        }
    } else {
        musr_strncpy(target, arg, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }

    if (musr_sc_chdir(target) < 0) {
        c_red();
        out_puts("cd: ");
        out_puts(target);
        out_puts(": not found\n");
        c_rst();
        last_exit_code = 1;
        return;
    }
    /* success: publish OLDPWD, refresh the prompt cwd */
    env_store("OLDPWD", old);
    cwd_init();
    last_exit_code = 0;
}
