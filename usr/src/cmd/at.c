/*
 * M4KK1 4P1 - at.c
 * Description: at command - schedule one-time commands
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

#define JOB_MAX 16
#define JOB_CMD_LEN 128

static struct {
    char cmd[JOB_CMD_LEN];
    uint32_t due_tick;
    int active;
} at_jobs[JOB_MAX];
static int at_job_count;
static uint32_t at_tick_counter;

/* 命令注入防护：验证输入是否包含危险字符 */
static int validate_command_input(const char *input)
{
    const char *dangerous = ";|&()`$><\\";
    for (int i = 0; input[i]; i++) {
        for (int j = 0; dangerous[j]; j++) {
            if (input[i] == dangerous[j]) {
                return 0; /* 发现危险字符 */
            }
        }
    }
    return 1; /* 输入安全 */
}

/**
 * musr_at_check_jobs - Check and run scheduled jobs
 *
 * Return: void
 */
void musr_at_check_jobs(void)
{
    at_tick_counter++;
    for (int i = 0; i < JOB_MAX; i++) {
        if (at_jobs[i].active
            && at_tick_counter >= at_jobs[i].due_tick) {
            at_jobs[i].active = 0;
            at_job_count--;
            c_grn();
            out_puts("[scheduler] Running: ");
            c_wht();
            out_puts(at_jobs[i].cmd);
            out_putc('\n');
            char *argv[32];
            char copy[JOB_CMD_LEN];
            musr_strncpy(copy, at_jobs[i].cmd, sizeof(copy)-1);
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
            if (ac > 0) {
                for (int j = 0; musr_cmd_table[j].name; j++) {
                    if (musr_strcmp(argv[0],
                                    musr_cmd_table[j].name) == 0) {
                        musr_cmd_table[j].func(ac, argv);
                        break;
                    }
                }
            }
        }
    }
}

/**
 * musr_cmd_at - Schedule a command for later execution
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_at(int ac, char **av)
{
    if (ac < 3) {
        out_puts("usage: at <time> <command> [args...]\n");
        return;
    }
    
    /* 命令注入防护：验证所有参数 */
    for (int i = 2; i < ac; i++) {
        if (!validate_command_input(av[i])) {
            c_red();
            out_puts("at: rejected - dangerous characters detected\n");
            c_rst();
            return;
        }
    }
    
    if (at_job_count >= JOB_MAX) {
        c_red();
        out_puts("at: job queue full\n");
        c_rst();
        return;
    }
    uint32_t delay = 0;
    if (av[1][0] == '+') {
        char *end;
        for (end = av[1] + 1; *end >= '0' && *end <= '9'; end++)
            ;
        char num[16];
        int k;
        for (k = 0; av[1][k + 1] && k < 14; k++)
            num[k] = av[1][k + 1];
        num[k] = '\0';
        int val = 0;
        for (char *q = num; *q; q++)
            val = val * 10 + (*q - '0');
        if (musr_strpref(av[2], "sec"))
            delay = (uint32_t)val;
        else if (musr_strpref(av[2], "min"))
            delay = (uint32_t)(val * 60);
        else {
            c_red();
            out_puts("at: bad unit (sec/min)\n");
            c_rst();
            return;
        }
    }
    if (delay > 100000)
        delay = 100000;
    int slot = -1;
    for (int i = 0; i < JOB_MAX; i++) {
        if (!at_jobs[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return;
    musr_strncpy(at_jobs[slot].cmd, av[3], sizeof(at_jobs[slot].cmd)-1);
    for (int i = 4; i < ac; i++) {
        int cl = musr_strlen(at_jobs[slot].cmd);
        if (cl + musr_strlen(av[i]) + 1 < JOB_CMD_LEN) {
            at_jobs[slot].cmd[cl] = ' ';
            musr_strncpy(at_jobs[slot].cmd + cl + 1, av[i], sizeof(at_jobs[slot].cmd)-cl-1);
        }
    }
    at_jobs[slot].due_tick = at_tick_counter + delay;
    at_jobs[slot].active = 1;
    at_job_count++;
    c_grn();
    out_puts("at: job scheduled (tick +");
    print_u32(delay);
    out_puts(")\n");
    c_rst();
}
