/*
 * M4KK1 4P1 - m4sh_main.c
 * Description: M4KK1 shell entry point and main loop
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

int out_fd = 1;
char cwd[256];
char envars[16][128];
int envar_cnt;

musr_cmd_t musr_cmd_table[] = {
    {"help",   musr_cmd_help,   "Show this help"},
    {"cd",     musr_cmd_cd,     "Change directory"},
    {"ls",     musr_cmd_ls,     "List directory"},
    {"cat",    musr_cmd_cat,    "Show file content"},
    {"echo",   musr_cmd_echo,   "Print arguments"},
    {"exit",   musr_cmd_exit,   "Exit shell"},
    {"pwd",    musr_cmd_pwd,    "Print working dir"},
    {"touch",  musr_cmd_touch,  "Create empty file"},
    {"mkdir",  musr_cmd_mkdir,  "Create directory"},
    {"mv",     musr_cmd_mv,     "Move/rename"},
    {"rm",     musr_cmd_rm,     "Remove file"},
    {"rmdir",  musr_cmd_rmdir,  "Remove directory"},
    {"cp",     musr_cmd_cp,     "Copy file"},
    {"ps",     musr_cmd_ps,     "Process list"},
    {"kill",   musr_cmd_kill,   "Send signal"},
    {"nice",   musr_cmd_nice,   "Set priority"},
    {"time",   musr_cmd_time,   "Time command"},
    {"grep",   musr_cmd_grep,   "Search text"},
    {"wc",     musr_cmd_wc,     "Count lines/words"},
    {"export", musr_cmd_export, "Set env var"},
    {"last",   musr_cmd_last,   "Show uptime"},
    {"date",   musr_cmd_date,   "Show date/time"},
    {"clear",  musr_cmd_clear,  "Clear screen"},
    {"uname",  musr_cmd_uname,  "System info"},
    {"free",   musr_cmd_free,   "Memory stats"},
    {"df",     musr_cmd_df,     "Disk usage"},
    {"mount",  musr_cmd_mount,  "Mount filesystem"},
    {"umount", musr_cmd_umount, "Unmount filesystem"},
    {"at",     musr_cmd_at,     "Schedule command"},
    {"batch",  musr_cmd_batch,  "Batch schedule"},
    {"calc",   musr_cmd_calc,   "Full bc-replacement calculator"},
    {"blkid",  musr_cmd_blkid,  "Block device info"},
    {"cal",    musr_cmd_cal,    "Calendar"},
    {"diff",   musr_cmd_diff,   "Compare files"},
    {"sead",   musr_cmd_sead,   "Stream editor (sed/awk replacement)"},
    {"id",     musr_cmd_id,     "Show user identity"},
    {"login",  musr_cmd_login,  "Authenticate as root"},
    {NULL, NULL, NULL}
};

static int valid(const char *n)
{
    for (int i = 0; musr_cmd_table[i].name; i++)
        if (musr_strcmp(n, musr_cmd_table[i].name) == 0)
            return 1;
    return 0;
}

static int expand_vars(char *out, const char *in, int osize)
{
    int oi = 0;
    while (*in && oi < osize - 1) {
        if (*in == '$') {
            in++;
            char vname[128];
            int vi = 0;
            if (*in == '{') {
                in++;
                while (*in && *in != '}' && vi < 127)
                    vname[vi++] = *in++;
                if (*in == '}')
                    in++;
            } else {
                while (*in && ((*in >= 'A' && *in <= 'Z') ||
                               (*in >= 'a' && *in <= 'z') ||
                               (*in >= '0' && *in <= '9') ||
                               *in == '_') && vi < 127)
                    vname[vi++] = *in++;
            }
            vname[vi] = '\0';
            if (vname[0]) {
                int found = 0;
                for (int e = 0; e < envar_cnt; e++) {
                    char *eq;
                    for (eq = envars[e]; *eq && *eq != '='; eq++)
                        ;
                    int nlen = eq - envars[e];
                    if (nlen > 0 && musr_strlen(vname) == nlen) {
                        int match = 1;
                        for (int c = 0; c < nlen; c++)
                            if (vname[c] != envars[e][c]) {
                                match = 0;
                                break;
                            }
                        if (match && *eq == '=') {
                            const char *val = eq + 1;
                            while (*val && oi < osize - 1)
                                out[oi++] = *val++;
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found && oi < osize - 1)
                    out[oi++] = '$';
            }
        } else {
            out[oi++] = *in++;
        }
    }
    out[oi] = '\0';
    return oi;
}

static void exec_one(const char *line)
{
    char expanded[256];
    expand_vars(expanded, line, 256);
    char copy[256], *argv[32];
    musr_strcpy(copy, expanded);
    int ac = 0;
    char *p = copy;
    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p || ac >= 31)
            break;
        argv[ac++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }
    argv[ac] = NULL;
    if (ac == 0)
        return;
    for (int i = 0; musr_cmd_table[i].name; i++)
        if (musr_strcmp(argv[0], musr_cmd_table[i].name) == 0) {
            musr_cmd_table[i].func(ac, argv);
            return;
        }
    c_red();
    out_puts("m4sh: ");
    out_puts(argv[0]);
    out_puts(": not found\n");
    c_rst();
}

static void run_pipeline(char *cmdline)
{
    char *parts[8];
    int np = 0;
    char *p = cmdline;
    while (*p && np < 8) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        parts[np++] = p;
        while (*p) {
            if (*p == '|') {
                *p = '\0';
                p++;
                break;
            }
            p++;
        }
    }
    if (np == 0)
        return;
    if (np == 1) {
        exec_one(parts[0]);
        return;
    }

    int prev_read = -1;
    for (int i = 0; i < np; i++) {
        int fds[2];
        int is_last = (i == np - 1);
        if (!is_last) {
            if (musr_sc_pipe(fds) < 0) {
                c_red();
                out_puts("pipe: failed\n");
                c_rst();
                return;
            }
        }
        if (prev_read >= 0) {
            musr_sc_dup2(prev_read, 0);
            musr_sc_close(prev_read);
        }
        if (!is_last) {
            musr_sc_dup2(fds[1], 1);
            out_fd = fds[1];
        } else {
            out_fd = 1;
        }
        exec_one(parts[i]);
        if (!is_last)
            musr_sc_close(fds[1]);
        if (prev_read >= 0) {
            musr_sc_close(prev_read);
            musr_sc_close(0);
        }
        prev_read = is_last ? -1 : fds[0];
        out_fd = 1;
    }
}

#define TOKEN_MAX 16
static int tokenize(char *s, char **av, int mx)
{
    int ac = 0;
    while (*s) {
        while (*s == ' ')
            s++;
        if (!*s || ac >= mx - 1)
            break;
        av[ac++] = s;
        while (*s && *s != ' ')
            s++;
        if (*s)
            *s++ = '\0';
    }
    av[ac] = NULL;
    return ac;
}

static char cmd_buf[256];
static int cmd_len;
#define HIST_MAX 16
static char history[HIST_MAX][256];
static int hist_count, hist_cur;
static int tab_count;
static const char *tab_matches[16];

static void tab_comp(void)
{
    if (cmd_len == 0)
        return;
    int n = 0;
    for (int i = 0; musr_cmd_table[i].name; i++)
        if (musr_strpref(musr_cmd_table[i].name, cmd_buf)) {
            int dup = 0;
            for (int j = 0; j < tab_count; j++)
                if (musr_strcmp(tab_matches[j],
                                musr_cmd_table[i].name) == 0)
                    dup = 1;
            if (!dup)
                tab_matches[n++] = musr_cmd_table[i].name;
        }
    if (n == 0)
        return;
    if (n == 1) {
        musr_strcpy(cmd_buf, tab_matches[0]);
        cmd_len = musr_strlen(cmd_buf);
        cmd_buf[cmd_len++] = ' ';
        cmd_buf[cmd_len] = '\0';
        tab_count = 0;
    } else {
        tab_count = n;
        ser_puts("\n");
        for (int i = 0; i < n; i++) {
            c_grn();
            ser_puts(tab_matches[i]);
            c_wht();
            ser_puts("  ");
        }
        ser_puts("\n");
    }
}

static void hist_add(const char *c)
{
    if (!c || !*c)
        return;
    if (hist_count > 0 &&
        musr_strcmp(history[hist_count - 1], c) == 0)
        return;
    if (hist_count < HIST_MAX)
        musr_strcpy(history[hist_count++], c);
    else {
        for (int i = 1; i < HIST_MAX; i++)
            musr_strcpy(history[i - 1], history[i]);
        musr_strcpy(history[HIST_MAX - 1], c);
    }
}

static void render_line(void)
{
    ser_puts("\r");
    c_ylw();
    ser_puts("[m4kk1]");
    c_wht();
    ser_puts("@");
    c_cyn();
    ser_puts("m4sh");
    c_wht();
    ser_puts(" ~> ");
    if (cmd_len > 0) {
        int wl = 0;
        while (wl < cmd_len && cmd_buf[wl] != ' ')
            wl++;
        if (wl > 0) {
            char save = cmd_buf[wl];
            cmd_buf[wl] = '\0';
            if (valid(cmd_buf))
                c_grn();
            else
                c_red();
            cmd_buf[wl] = save;
            for (int i = 0; i < wl; i++)
                ser_putc(cmd_buf[i]);
        }
        if (wl < cmd_len) {
            c_wht();
            for (int i = wl; i < cmd_len; i++)
                ser_putc(cmd_buf[i]);
        }
    }
    c_wht();
    ser_puts("\x1B[K");
}

static void execute(void)
{
    ser_puts("\n");
    if (cmd_len == 0)
        return;
    char copy[256];
    musr_strcpy(copy, cmd_buf);
    hist_add(copy);
    hist_cur = hist_count;

    if (musr_strchr(copy, '|')) {
        run_pipeline(copy);
    } else {
        char *argv[TOKEN_MAX];
        int argc = tokenize(copy, argv, TOKEN_MAX);
        if (argc > 0) {
            int found = 0;
            for (int i = 0; musr_cmd_table[i].name; i++)
                if (musr_strcmp(argv[0],
                                musr_cmd_table[i].name) == 0) {
                    musr_cmd_table[i].func(argc, argv);
                    found = 1;
                    break;
                }
            if (!found) {
                c_red();
                ser_puts("m4sh: ");
                ser_puts(argv[0]);
                ser_puts(": not found\n");
                c_rst();
            }
        }
    }
    cmd_len = 0;
    cmd_buf[0] = '\0';
    tab_count = 0;
}

static void handle_key(char ch)
{
    if (ch == '\n' || ch == '\r') {
        execute();
        return;
    }
    if (ch == '\b' || ch == 0x7F) {
        if (cmd_len > 0)
            cmd_buf[--cmd_len] = '\0';
        return;
    }
    if (ch == '\t') {
        tab_comp();
        return;
    }
    if (ch == 0x03) {
        cmd_len = 0;
        cmd_buf[0] = '\0';
        ser_puts("^C\n");
        return;
    }
    if (ch == 0x04) {
        if (cmd_len == 0) {
            ser_puts("exit\n");
            musr_sc0(S_EXIT);
        }
        return;
    }
    if (ch >= 0x20 && ch <= 0x7E && cmd_len < 255) {
        cmd_buf[cmd_len++] = ch;
        cmd_buf[cmd_len] = '\0';
    }
}

static void handle_esc(int sec)
{
    if (sec != '[')
        return;
    int third = -1;
    for (int w = 0; w < 50000; w++) {
        int c = ser_getc();
        if (c > 0) {
            third = c;
            break;
        }
    }
    if (third == 'A' && hist_cur > 0) {
        hist_cur--;
        musr_strcpy(cmd_buf, history[hist_cur]);
        cmd_len = musr_strlen(cmd_buf);
    } else if (third == 'B' && hist_cur < hist_count) {
        hist_cur++;
        if (hist_cur == hist_count)
            cmd_buf[cmd_len = 0] = '\0';
        else {
            musr_strcpy(cmd_buf, history[hist_cur]);
            cmd_len = musr_strlen(cmd_buf);
        }
    }
}

void _start(void)
{
    ser_puts("\n====================================\n");
    c_grn();
    ser_puts("  M4KK1 Userspace Shell (M4SH)\n");
    c_wht();
    ser_puts("  Type 'help' for available commands\n");
    ser_puts("====================================\n");
    c_rst();
    cmd_len = 0;
    cmd_buf[0] = '\0';
    hist_count = hist_cur = 0;
    cwd_init();
    render_line();
    while (1) {
        musr_at_check_jobs();
        int ch = ser_getc();
        if (ch > 0) {
            if (ch == 0x1B) {
                int sec = -1;
                for (int w = 0; w < 50000; w++) {
                    int c = ser_getc();
                    if (c > 0) {
                        sec = c;
                        break;
                    }
                }
                if (sec > 0) {
                    handle_esc(sec);
                    render_line();
                }
            } else {
                handle_key(ch);
                tab_count = 0;
                render_line();
            }
        }
        __asm__ volatile("pause");
    }
}
