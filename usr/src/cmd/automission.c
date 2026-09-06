/*
 * M4KK1 4P1 - automission.c
 * Description: automission command - list registered automissions
 *              (periodic tasks) from /etc/automission with a live
 *              countdown to the next scheduled run, plus the
 *              built-in periodic missions.  Entries are registered
 *              via e.g. "backup -s HH:MM".
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"
#include "am_sched.h"

struct am_entry {
	const char *name;
	const char *sched;
	const char *desc;
};

/* Built-in periodic missions; kernel-side execution is not wired
 * yet, so these show their configured interval. */
static const struct am_entry am_tasks[] = {
	{ "build-check", "every 5h",  "rebuild kernel + smoke boot" },
	{ "sync-upstream", "daily 22:00", "push ahead branch, rotate main" },
	{ "log-rotate",  "weekly Thu", "rotate /var/log/messages" },
};

/**
 * musr_cmd_automission - list automissions with countdowns
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_automission(int ac, char **av)
{
	(void)ac;
	(void)av;
	struct am_sched reg[AM_MAX_ENTRIES];
	int nreg = am_sched_load(reg, AM_MAX_ENTRIES);

	c_ylw();
	out_puts("mission          schedule         next run in\n");
	c_wht();

	/* registered missions: live countdown from the RTC clock */
	for (int i = 0; i < nreg; i++) {
		out_puts(reg[i].name);
		out_puts("  daily ");
		if (reg[i].hh < 10)
			out_putc('0');
		print_u32(reg[i].hh);
		out_putc(':');
		if (reg[i].mm < 10)
			out_putc('0');
		print_u32(reg[i].mm);
		out_puts("  in ");
		am_print_hhmmss(am_countdown_secs(reg[i].hh,
						 reg[i].mm));
		out_puts("\n");
	}

	/* built-in missions */
	for (unsigned i = 0;
	     i < sizeof(am_tasks) / sizeof(am_tasks[0]); i++) {
		out_puts(am_tasks[i].name);
		out_puts("  ");
		out_puts(am_tasks[i].sched);
		out_puts("  ");
		out_puts(am_tasks[i].desc);
		out_puts("\n");
	}

	if (nreg == 0) {
		c_ylw();
		out_puts("(no scheduled missions; use: backup -s HH:MM)\n");
		c_wht();
	}
}
