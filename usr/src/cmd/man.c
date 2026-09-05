/*
 * M4KK1 4P1 - man.c
 * Description: man command - display manual pages from /export/share/man
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/* Section search order: commands first, then libs, then overview pages. */
static const int man_sections[] = {1, 2, 3, 4, 5, 7, 8};

/* Appends a char to an out-line buffer; flushes with newline. */
static void man_emit(const char *s)
{
    out_puts(s);
}

static void man_emit_n(const char *s, int n)
{
    for (int i = 0; i < n; i++)
        musr_sc_write(out_fd, &s[i], 1);
}

/**
 * man_print - print a manual page (roff-lite renderer)
 * @fd: open file descriptor of the page
 *
 * Handles the small roff subset the M4KK1 pages use:
 *   .DD skip   .TH header   .SH heading   .TP indent-newline
 *   .B bold    .I plain     \fB/\fR fonts  \- dash  \& zero-width
 *
 * Return: void
 */
static void man_print(int fd)
{
    static char buf[8192];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        out_puts("man: page is empty\n");
        return;
    }
    buf[n] = '\0';

    int i = 0;
    while (i < n) {
        int ls = i;
        while (i < n && buf[i] != '\n')
            i++;
        int le = i;
        if (i < n)
            i++;

        int j = ls;

        /* request lines */
        if (j < le && buf[j] == '.') {
            /* comment lines (.\") are silently dropped */
            if (le - ls >= 2 && buf[j+1] == '\\')
                continue;
            if (le - ls >= 3 && buf[j+1] == 'D' && buf[j+2] == 'D')
                continue;                       /* .DD */
            /* no-op layout requests */
            if (le - ls >= 2 && buf[j+1] == 'n' && buf[j+2] == 'f')
                continue;                       /* .nf */
            if (le - ls >= 2 && buf[j+1] == 'f' && buf[j+2] == 'i')
                continue;                       /* .fi */
            if (le - ls >= 3 && buf[j+1] == 'P' && buf[j+2] == 'P')
                continue;                       /* .PP */
            if (le - ls >= 3 && buf[j+1] == 'T' && buf[j+2] == 'H') {
                /* .TH NAME sec "title" → header */
                out_puts("\n=== ");
                int k = j + 3, words = 0;
                while (k < le) {
                    while (k < le
                           && (buf[k] == ' ' || buf[k] == '"'))
                        k++;
                    int ws = k;
                    while (k < le && buf[k] != ' ' && buf[k] != '"')
                        k++;
                    if (k > ws) {
                        words++;
                        if (words == 1)
                            man_emit_n(buf + ws, k - ws);
                    }
                }
                man_emit(" (man page) ===\n");
                continue;
            }
            if (le - ls >= 3 && buf[j+1] == 'S' && buf[j+2] == 'H') {
                out_puts("\n");
                c_grn();
                int k = j + 3;
                while (k < le && buf[k] == ' ')
                    k++;
                man_emit_n(buf + k, le - k);
                c_wht();
                out_puts("\n");
                continue;
            }
            if (le - ls >= 3 && buf[j+1] == 'T' && buf[j+2] == 'P') {
                out_puts("\n  ");
                continue;
            }
            if (le - ls >= 2 && (buf[j+1] == 'B' || buf[j+1] == 'I')
                && (j + 2 >= le || buf[j+2] == ' ')) {
                if (buf[j+1] == 'B')
                    c_grn();
                int k = j + 2;
                while (k < le && buf[k] == ' ')
                    k++;
                man_emit_n(buf + k, le - k);
                c_wht();
                out_puts("\n");
                continue;
            }
            /* unknown request: fall through, print verbatim */
        }

        /* plain text line: strip \fB/\fR, \-, \& escapes */
        {
            int k = ls;
            while (k < le) {
                if (buf[k] == '\\' && k + 1 < le) {
                    char nx = buf[k + 1];
                    if (nx == 'f' && k + 2 < le) {
                        if (buf[k + 2] == 'B')
                            c_grn();
                        else
                            c_wht();
                        k += 3;
                        continue;
                    }
                    if (nx == '-') {
                        man_emit("-");
                        k += 2;
                        continue;
                    }
                    if (nx == '&') {
                        k += 2;
                        continue;
                    }
                }
                musr_sc_write(out_fd, &buf[k], 1);
                k++;
            }
            out_puts("\n");
        }
    }
    c_rst();
}

/* /export/share/man/man<sec>/<name>.<sec> */
static int man_open_page(const char *name, int sec)
{
    static char path[300];
    const char *pre = "/export/share/man/man";
    int i = 0;
    while (*pre && i < 299)
        path[i++] = *pre++;
    path[i++] = '0' + (char)sec;
    path[i++] = '/';
    while (*name && i < 299)
        path[i++] = *name++;
    path[i++] = '.';
    path[i++] = '0' + (char)sec;
    path[i] = '\0';
    return musr_sc_open(path, O_RDONLY);
}

/**
 * musr_cmd_man - display a manual page
 * @ac: argument count
 * @av: argument vector ("man" "name" ["section"])
 *
 * Pages live under /export/share/man/<section>/name.<section>.
 * "man -l" lists all installed pages.
 *
 * Return: void
 */
void musr_cmd_man(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: man <name> [section]\n");
        out_puts("       man -l           list all pages\n");
        return;
    }
    if (musr_strcmp(av[1], "-l") == 0) {
        for (int s = 0; s < (int)(sizeof(man_sections) /
                                  sizeof(man_sections[0])); s++) {
            /* YAFS readdir is stateless (every call walks from the
             * directory head — there is no cursor), so a getdents
             * loop would repeat the first entries forever.  One call
             * with a large buffer is the correct shape here; man1 is
             * the biggest section (~70 pages). */
            static struct dirent de[80];
            /* open the section dir itself */
            static char dpath[64];
            const char *pre = "/export/share/man/man";
            int i = 0;
            while (*pre)
                dpath[i++] = *pre++;
            dpath[i++] = '0' + (char)man_sections[s];
            dpath[i] = '\0';
            int dfd = musr_sc_open(dpath, O_RDONLY);
            if (dfd < 0)
                continue;
            /* getdents' count arg and return value are entry counts
             * (see ls.c), not byte counts. */
            int cnt = musr_sc_getdents(dfd, de, 80);
            musr_sc_close(dfd);
            if (cnt <= 0)
                continue;
            out_puts("section ");
            out_putc('0' + (char)man_sections[s]);
            out_puts(":\n");
            for (int d = 0; d < cnt && d < 80; d++) {
                out_puts("  ");
                out_puts(de[d].name);
                out_puts("\n");
            }
        }
        return;
    }

    int want_sec = 0;
    if (ac >= 3)
        want_sec = musr_atoi(av[2]);
    for (int s = 0; s < (int)(sizeof(man_sections) /
                              sizeof(man_sections[0])); s++) {
        int sec = man_sections[s];
        if (want_sec && sec != want_sec)
            continue;
        int fd = man_open_page(av[1], sec);
        if (fd < 0)
            continue;
        man_print(fd);
        musr_sc_close(fd);
        return;
    }
    out_puts("man: no manual page for ");
    out_puts(av[1]);
    out_puts("\n");
}
