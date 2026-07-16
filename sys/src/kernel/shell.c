/*
 * M4KK1 4P1 - shell.c
 * Description: Kernel rescue shell with command-line interface.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include "console.h"
#include "kernel.h"
#include "process.h"
#include "memory.h"

void mkrn_yafs_test(void);

#define COM1_DATA     0x3F8
#define COM1_LSR      0x3FD
#define LSR_DATA_READY 0x01
#define HIST_SIZE     16
#define CMD_BUF_SIZE  256
#define PROMPT_W      18

static inline void outb(u16 p, u8 v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
}

static inline u8 inb(u16 p)
{
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(p));
    return r;
}

static int serial_read(void)
{
    if (inb(COM1_LSR) & LSR_DATA_READY)
        return inb(COM1_DATA);
    return -1;
}

#define C_WHT 0x0F
#define C_GRN 0x0A
#define C_RED 0x0C
#define C_YLW 0x0E
#define C_CYN 0x0B
#define C_GRY 0x08

static void col(u8 f)
{
    mkrn_console_set_color(f);
}

typedef void (*cmd_f)(int, char **);
typedef struct {
    const char *name;
    cmd_f func;
    const char *desc;
} cmd_t;

static void cmd_help(int, char **);
static void cmd_ps(int, char **);
static void cmd_free(int, char **);
static void cmd_reboot(int, char **);
static void cmd_clear(int, char **);
static void cmd_echo(int, char **);
static void cmd_yafstest(int, char **);

static cmd_t cmd_table[] = {
    {"help",     cmd_help,     "Show this help message"},
    {"ps",       cmd_ps,       "Show running processes"},
    {"free",     cmd_free,     "Show memory usage"},
    {"reboot",   cmd_reboot,   "Reboot the system"},
    {"clear",    cmd_clear,    "Clear the screen"},
    {"echo",     cmd_echo,     "Echo arguments"},
    {"yafstest", cmd_yafstest, "Run YAFS B+Tree filesystem test"},
    {NULL, NULL, NULL}
};

static int is_cmd(const char *n)
{
    for (int i = 0; cmd_table[i].name; i++)
        if (mkrn_strcmp(n, cmd_table[i].name) == 0)
            return 1;
    return 0;
}

static int str_pref(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return 0;
    return 1;
}

static void cmd_help(int ac, char **av)
{
    (void)ac;
    (void)av;
    mkrn_console_write("M4KK1 built-in commands:\n");
    for (int i = 0; cmd_table[i].name; i++) {
        col(C_GRN);
        mkrn_console_write("  ");
        mkrn_console_write(cmd_table[i].name);
        col(C_WHT);
        mkrn_console_write("  - ");
        mkrn_console_write(cmd_table[i].desc);
        mkrn_console_write("\n");
    }
}

static void cmd_ps(int ac, char **av)
{
    (void)ac;
    (void)av;
    mkrn_process_t *cur = mkrn_process_get_current();
    mkrn_console_write("PID  STATE  NAME\n");
    if (cur) {
        col(C_CYN);
        mkrn_console_write_dec(cur->pid);
        col(C_WHT);
        mkrn_console_write("  ");
        switch (cur->state) {
            case 0:
                col(C_GRN);
                mkrn_console_write("RUN ");
                break;
            case 1:
                col(C_YLW);
                mkrn_console_write("RDY ");
                break;
            case 2:
                col(C_GRY);
                mkrn_console_write("BLK ");
                break;
            case 3:
                col(C_RED);
                mkrn_console_write("TER ");
                break;
            default:
                mkrn_console_write("??? ");
                break;
        }
        col(C_WHT);
        mkrn_console_write(" ");
        mkrn_console_write(cur->name);
        mkrn_console_write("\n");
    }
    mkrn_console_write("Total: ");
    mkrn_console_write_dec(mkrn_process_get_count());
    mkrn_console_write(" process(es)\n");
}

static void cmd_free(int ac, char **av)
{
    (void)ac;
    (void)av;
    u32 t = mkrn_memory_get_total();
    u32 f = mkrn_memory_get_free();
    col(C_YLW);
    mkrn_console_write("Memory Usage:\n");
    col(C_WHT);
    mkrn_console_write("  Total:  ");
    mkrn_console_write_dec(t / 1024);
    mkrn_console_write(" KB\n");
    col(C_GRN);
    mkrn_console_write("  Free:   ");
    mkrn_console_write_dec(f / 1024);
    mkrn_console_write(" KB\n");
    col(C_RED);
    mkrn_console_write("  Used:   ");
    mkrn_console_write_dec((t - f) / 1024);
    mkrn_console_write(" KB\n");
    col(C_WHT);
}

static void cmd_reboot(int ac, char **av)
{
    (void)ac;
    (void)av;
    mkrn_console_write("Rebooting...\n");
    __asm__ volatile("cli");
    outb(0x64, 0xFE);
    __asm__ volatile("hlt");
}

static void cmd_clear(int ac, char **av)
{
    (void)ac;
    (void)av;
    mkrn_console_clear();
}

static void cmd_echo(int ac, char **av)
{
    for (int i = 1; i < ac; i++) {
        if (i > 1)
            mkrn_console_write(" ");
        mkrn_console_write(av[i]);
    }
    mkrn_console_write("\n");
}

static void cmd_yafstest(int ac, char **av)
{
    (void)ac;
    (void)av;
    mkrn_yafs_test();
}

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

static char cmd_buf[CMD_BUF_SIZE];
static int cmd_len;
static char history[HIST_SIZE][CMD_BUF_SIZE];
static int hist_count, hist_cur;

static int tab_count;
static const char *tab_ms[16];

static void tab_comp(void)
{
    if (cmd_len == 0)
        return;
    int n = 0;
    for (int i = 0; cmd_table[i].name; i++)
        if (str_pref(cmd_table[i].name, cmd_buf)) {
            int dup = 0;
            for (int j = 0; j < tab_count; j++)
                if (mkrn_strcmp(
                        tab_ms[j],
                        cmd_table[i].name) == 0)
                    dup = 1;
            if (!dup)
                tab_ms[n++] = cmd_table[i].name;
        }
    if (n == 0)
        return;
    if (n == 1) {
        mkrn_strcpy(cmd_buf, tab_ms[0]);
        cmd_len = mkrn_strlen(cmd_buf);
        cmd_buf[cmd_len++] = ' ';
        cmd_buf[cmd_len] = '\0';
        tab_count = 0;
    } else {
        tab_count = n;
        mkrn_console_write("\n");
        for (int i = 0; i < n; i++) {
            col(C_GRN);
            mkrn_console_write(tab_ms[i]);
            col(C_WHT);
            mkrn_console_write("  ");
        }
        mkrn_console_write("\n");
    }
}

static void hist_add(const char *c)
{
    if (!c || !*c)
        return;
    if (hist_count > 0
        && mkrn_strcmp(
               history[hist_count - 1], c) == 0)
        return;
    if (hist_count < HIST_SIZE) {
        mkrn_strcpy(history[hist_count++], c);
    } else {
        for (int i = 1; i < HIST_SIZE; i++)
            mkrn_strcpy(history[i - 1], history[i]);
        mkrn_strcpy(history[HIST_SIZE - 1], c);
    }
}

static void render_line(void)
{
    u8 cx, cy;
    mkrn_console_get_cursor(&cx, &cy);

    mkrn_console_put_char('\r');

    col(C_YLW);
    mkrn_console_write("[m4kk1]");
    col(C_WHT);
    mkrn_console_write("@");
    col(C_CYN);
    mkrn_console_write("kernel");
    col(C_WHT);
    mkrn_console_write(" ~> ");

    if (cmd_len > 0) {
        int wl = 0;
        while (wl < cmd_len && cmd_buf[wl] != ' ')
            wl++;

        if (wl > 0) {
            char save = cmd_buf[wl];
            cmd_buf[wl] = '\0';
            int valid = is_cmd(cmd_buf);
            cmd_buf[wl] = save;

            col(valid ? C_GRN : C_RED);
            for (int i = 0; i < wl; i++)
                mkrn_console_put_char(cmd_buf[i]);
        }
        if (wl < cmd_len) {
            col(C_WHT);
            for (int i = wl; i < cmd_len; i++)
                mkrn_console_put_char(cmd_buf[i]);
        }
    }

    col(C_WHT);
    int total = PROMPT_W + cmd_len;
    for (int i_ = total;
         i_ < M4K_VGA_WIDTH;
         i_++)
        mkrn_console_put_char(' ');
    mkrn_console_set_cursor(
        total % M4K_VGA_WIDTH,
        cy + total / M4K_VGA_WIDTH);
}

static void execute(void)
{
    mkrn_console_write("\n");
    if (cmd_len == 0)
        return;

    char copy[CMD_BUF_SIZE];
    mkrn_strcpy(copy, cmd_buf);
    hist_add(copy);
    hist_cur = hist_count;

    char *argv[16];
    int argc = tokenize(copy, argv, 16);

    if (argc > 0) {
        int found = 0;
        for (int i = 0; cmd_table[i].name; i++)
            if (mkrn_strcmp(
                    argv[0],
                    cmd_table[i].name) == 0) {
                cmd_table[i].func(argc, argv);
                found = 1;
                break;
            }
        if (!found) {
            col(C_RED);
            mkrn_console_write(
                "m4kk1: command not found: ");
            mkrn_console_write(argv[0]);
            mkrn_console_write("\n");
            col(C_WHT);
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
    if (ch >= 0x20 && ch <= 0x7E
        && cmd_len < CMD_BUF_SIZE - 1) {
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
        int c = serial_read();
        if (c > 0) {
            third = c;
            break;
        }
    }
    if (third == 'A' && hist_cur > 0) {
        hist_cur--;
        mkrn_strcpy(cmd_buf, history[hist_cur]);
        cmd_len = mkrn_strlen(cmd_buf);
    } else if (third == 'B'
               && hist_cur < hist_count) {
        hist_cur++;
        if (hist_cur == hist_count) {
            cmd_buf[cmd_len = 0] = '\0';
        } else {
            mkrn_strcpy(cmd_buf, history[hist_cur]);
            cmd_len = mkrn_strlen(cmd_buf);
        }
    }
}

void mkrn_shell_main(void)
{
    mkrn_console_write("\n");
    mkrn_console_write("====================================\n");
    col(C_GRN);
    mkrn_console_write(
        "  M4KK1 Kernel Microshell v1.0\n");
    col(C_WHT);
    mkrn_console_write(
        "  Type 'help' for available commands\n");
    mkrn_console_write("====================================\n");

    cmd_len = 0;
    cmd_buf[0] = '\0';
    hist_count = hist_cur = 0;

    render_line();

    while (1) {
        int ch = serial_read();
        if (ch > 0) {
            if (ch == 0x1B) {
                int sec = -1;
                for (int w = 0; w < 50000; w++) {
                    int c = serial_read();
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
