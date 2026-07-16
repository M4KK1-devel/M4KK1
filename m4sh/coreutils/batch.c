/*
 * M4KK1 4P1 - batch.c
 * Description: batch command - schedule commands
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

#define BATCH_MAX 16
#define BATCH_CMD_LEN 128

static struct {
    char cmd[BATCH_CMD_LEN];
    int active;
} batch_jobs[BATCH_MAX];
static int batch_job_count;

static void batch_run_all(void)
{
    for (int i = 0; i < BATCH_MAX; i++) {
        if (!batch_jobs[i].active)
            continue;
        batch_jobs[i].active = 0;
        batch_job_count--;
        c_grn();
        out_puts("[batch] Running: ");
        c_wht();
        out_puts(batch_jobs[i].cmd);
        out_putc('\n');
        char *argv[32];
        char copy[BATCH_CMD_LEN];
        musr_strcpy(copy, batch_jobs[i].cmd);
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

/**
 * musr_cmd_batch - Queue and run batch commands
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_batch(int ac, char **av)
{
    if (ac < 2) {
        out_puts("usage: batch <command> [args]\n");
        return;
    }
    if (batch_job_count >= BATCH_MAX) {
        c_red();
        out_puts("batch: queue full\n");
        c_rst();
        return;
    }
    int slot = -1;
    for (int i = 0; i < BATCH_MAX; i++) {
        if (!batch_jobs[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return;
    musr_strcpy(batch_jobs[slot].cmd, av[1]);
    for (int i = 2; i < ac; i++) {
        int cl = musr_strlen(batch_jobs[slot].cmd);
        if (cl + musr_strlen(av[i]) + 1 < BATCH_CMD_LEN) {
            batch_jobs[slot].cmd[cl] = ' ';
            musr_strcpy(batch_jobs[slot].cmd + cl + 1, av[i]);
        }
    }
    batch_jobs[slot].active = 1;
    batch_job_count++;
    c_grn();
    out_puts("batch: job queued (");
    print_u32(batch_job_count);
    out_puts(" pending)\n");
    c_rst();
    batch_run_all();
}
