/*
 * M4KK1 4P1 - automission.c
 * Description: automission command - list registered automissions
 *              (periodic tasks) and their next scheduled run.
 *              CLI builtin (m4sh command table), replacing the
 *              earlier GUI panel draft.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

struct am_entry {
    const char *name;
    const char *sched;
    const char *desc;
};

/* The automission registry: tasks the system can re-run on a
 * schedule.  Kernel-side scheduling is not wired yet, so entries
 * show their configured interval and last-run stamp from
 * /var/log/automission.log when present. */
static const struct am_entry am_tasks[] = {
    { "build-check", "every 5h",  "rebuild kernel + smoke boot" },
    { "sync-upstream", "daily 22:00", "push ahead branch, rotate main" },
    { "log-rotate",  "weekly Thu", "rotate /var/log/messages" },
};

/**
 * musr_cmd_automission - list automissions
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_automission(int ac, char **av)
{
    (void)ac;
    (void)av;
    c_ylw();
    out_puts("mission          schedule         description\n");
    c_wht();
    for (unsigned i = 0;
         i < sizeof(am_tasks) / sizeof(am_tasks[0]); i++) {
        out_puts(am_tasks[i].name);
        out_puts("  ");
        out_puts(am_tasks[i].sched);
        out_puts("  ");
        out_puts(am_tasks[i].desc);
        out_puts("\n");
    }
}
