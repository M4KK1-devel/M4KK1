/*
 * M4KK1 4P1 - m4sh_main.c
 * Description: M4KK1 shell with &&, ||, ;, |, quotes, and glob support
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

int out_fd = 1;
char cwd[256];
char envars[16][128];
int envar_cnt;
int musr_login_ok = 0;

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
    {"quell",  musr_cmd_quell,  "Execute command as root (prime group)"},
    {"passwd", musr_cmd_passwd, "Change password"},
    {"who",    musr_cmd_who,    "Show active sessions"},
    {"usermod",musr_cmd_usermod,"Modify user account"},
    {"groupmod",musr_cmd_groupmod,"Modify group"},
    {"cu",     musr_cmd_cu,     "Change user (switch account)"},
    {"userlog",musr_cmd_userlog,"Show login history"},
    {"gfx_test",musr_cmd_gfx_test,"Draw graphics test pattern"},
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
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    while (*in && oi < osize - 1) {
        if (*in == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            in++;
            continue;
        }
        if (*in == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            in++;
            continue;
        }
        if (*in == '$' && !in_single_quote) {
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

static int tokenize_quoted(char *s, char **av, int mx)
{
    int ac = 0;
    char *p = s;
    
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p || ac >= mx - 1)
            break;
        
        av[ac++] = p;
        char *out = p;
        
        while (*p && *p != ' ' && *p != '\t') {
            if (*p == '\'') {
                p++;
                while (*p && *p != '\'') {
                    *out++ = *p++;
                }
                if (*p == '\'')
                    p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$')) {
                        p++;
                    }
                    *out++ = *p++;
                }
                if (*p == '"')
                    p++;
            } else if (*p == '\\' && p[1]) {
                p++;
                *out++ = *p++;
            } else {
                *out++ = *p++;
            }
        }
        if (*p)
            *p++ = '\0';
        *out = '\0';
    }
    av[ac] = NULL;
    return ac;
}

static int last_exit_code = 0;

static void exec_one(const char *line)
{
    char expanded[512];
    expand_vars(expanded, line, 512);
    char copy[512], *argv[32];
    musr_strcpy(copy, expanded);
    int ac = tokenize_quoted(copy, argv, 32);
    
    if (ac == 0)
        return;
    
    int cmd_idx = -1;
    for (int i = 0; musr_cmd_table[i].name; i++)
        if (musr_strcmp(argv[0], musr_cmd_table[i].name) == 0) {
            cmd_idx = i;
            break;
        }
    
    if (cmd_idx < 0) {
        c_red();
        out_puts("m4sh: ");
        out_puts(argv[0]);
        out_puts(": not found\n");
        c_rst();
        last_exit_code = 127;
        return;
    }
    
    int saved_in = -1, saved_out = -1;
    int restore_in = 0, restore_out = 0;
    int redirect_fd = -1;
    
    for (int i = 0; i < ac; i++) {
        int is_append = 0;
        if (musr_strcmp(argv[i], ">") == 0)
            is_append = 0;
        else if (musr_strcmp(argv[i], ">>") == 0)
            is_append = 1;
        else if (musr_strcmp(argv[i], "<") == 0) {
            if (i + 1 >= ac) {
                c_red();
                out_puts("m4sh: syntax error: < missing filename\n");
                c_rst();
                last_exit_code = 2;
                return;
            }
            char path[256];
            cwd_to_abs(argv[i + 1], path, 256);
            int fd = musr_sc_open(path, O_RDONLY);
            if (fd < 0) {
                c_red();
                out_puts("m4sh: ");
                out_puts(argv[i + 1]);
                out_puts(": no such file\n");
                c_rst();
                last_exit_code = 1;
                return;
            }
            saved_in = musr_sc_dup2(0, 11);
            musr_sc_dup2(fd, 0);
            musr_sc_close(fd);
            restore_in = 1;
            int rem = ac - i - 2;
            for (int j = 0; j < rem; j++)
                argv[i + j] = argv[i + 2 + j];
            ac -= 2;
            argv[ac] = NULL;
            i--;
            continue;
        } else {
            continue;
        }
        
        if (i + 1 >= ac) {
            c_red();
            out_puts("m4sh: syntax error: > missing filename\n");
            c_rst();
            last_exit_code = 2;
            return;
        }
        
        char path[256];
        cwd_to_abs(argv[i + 1], path, 256);
        int fd;
        
        if (is_append) {
            int old_fd = musr_sc_open(path, O_RDONLY);
            char old_buf[4096];
            int old_len = 0;
            if (old_fd >= 0) {
                old_len = musr_sc_read(old_fd, old_buf, 4096);
                musr_sc_close(old_fd);
            }
            fd = musr_sc_open(path, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd >= 0 && old_len > 0)
                musr_sc_write(fd, old_buf, old_len);
        } else {
            fd = musr_sc_open(path, O_WRONLY | O_CREAT | O_TRUNC);
        }
        
        if (fd < 0) {
            c_red();
            out_puts("m4sh: ");
            out_puts(argv[i + 1]);
            out_puts(": cannot create\n");
            c_rst();
            last_exit_code = 1;
            return;
        }
        
        saved_out = musr_sc_dup2(1, 10);
        musr_sc_dup2(fd, 1);
        out_fd = fd;
        redirect_fd = fd;
        restore_out = 1;
        
        int rem = ac - i - 2;
        for (int j = 0; j < rem; j++)
            argv[i + j] = argv[i + 2 + j];
        ac -= 2;
        argv[ac] = NULL;
        i--;
    }
    
    musr_cmd_table[cmd_idx].func(ac, argv);
    last_exit_code = 0;
    
    if (restore_out) {
        musr_sc_dup2(saved_out, 1);
        musr_sc_close(saved_out);
        if (redirect_fd >= 0)
            musr_sc_close(redirect_fd);
        out_fd = 1;
    }
    if (restore_in) {
        musr_sc_dup2(saved_in, 0);
        musr_sc_close(saved_in);
    }
}

static void run_pipeline(char *cmdline)
{
    char *parts[8];
    int np = 0;
    char *p = cmdline;
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    while (*p && np < 8) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        parts[np++] = p;
        while (*p) {
            if (*p == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            } else if (*p == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            } else if (*p == '|' && !in_single_quote && !in_double_quote) {
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

static void execute_command_list(char *line)
{
    char *commands[32];
    int operators[32];
    int ncmds = 0;
    
    char *p = line;
    commands[ncmds++] = p;
    
    int in_single_quote = 0;
    int in_double_quote = 0;
    
    while (*p && ncmds < 32) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        } else if (!in_single_quote && !in_double_quote) {
            if (*p == ';') {
                *p = '\0';
                p++;
                operators[ncmds - 1] = 0;
                commands[ncmds++] = p;
                continue;
            } else if (*p == '&' && p[1] == '&') {
                *p = '\0';
                p[1] = '\0';
                p += 2;
                operators[ncmds - 1] = 1;
                commands[ncmds++] = p;
                continue;
            } else if (*p == '|' && p[1] == '|') {
                *p = '\0';
                p[1] = '\0';
                p += 2;
                operators[ncmds - 1] = 2;
                commands[ncmds++] = p;
                continue;
            }
        }
        p++;
    }
    
    for (int i = 0; i < ncmds; i++) {
        if (i > 0) {
            int op = operators[i - 1];
            if (op == 1 && last_exit_code != 0)
                break;
            if (op == 2 && last_exit_code == 0)
                break;
        }
        
        char *cmd = commands[i];
        while (*cmd == ' ')
            cmd++;
        if (!*cmd)
            continue;
        
        if (musr_strchr(cmd, '|')) {
            run_pipeline(cmd);
        } else {
            exec_one(cmd);
        }
    }
}

#define TOKEN_MAX 16
static int tokenize(char *s, char **av, int mx)
{
    return tokenize_quoted(s, av, mx);
}

static char cmd_buf[512];
static int cmd_len;
#define HIST_MAX 16
static char history[HIST_MAX][512];
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
    char copy[512];
    musr_strcpy(copy, cmd_buf);
    hist_add(copy);
    hist_cur = hist_count;

    execute_command_list(copy);
    
    cmd_len = 0;
    cmd_buf[0] = '\0';
    tab_count = 0;
}

static void handle_key(char ch)
{
    if (ch == '\n' || ch == '\r') {
        ser_puts("\n");
        execute();
        render_line();
        return;
    }
    if (ch == '\b' || ch == 0x7F) {
        if (cmd_len > 0) {
            cmd_buf[--cmd_len] = '\0';
            ser_puts("\b \b");
        }
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
    if (ch >= 0x20 && ch <= 0x7E && cmd_len < 511) {
        cmd_buf[cmd_len++] = ch;
        cmd_buf[cmd_len] = '\0';
        ser_putc(ch);
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
    if (m4k_geteuid() == 0) {
        musr_boot_setup();
        if (musr_sc_getpid() == 1) {
            musr_login_ok = 1;
        }
        while (!musr_login_ok) {
            char *login_argv[] = {"login", NULL};
            musr_cmd_login(1, login_argv);
        }
    }
    musr_setup_env();
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
                int was_tab = (ch == '\t');
                handle_key(ch);
                tab_count = 0;
                if (was_tab)
                    render_line();
            }
        }
        __asm__ volatile("pause");
    }
}
