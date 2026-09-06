/*
 * M4KK1 4P1 - am_sched.h
 * Description: shared helpers for the automission registry
 *              (/etc/automission).  Used by backup -s HH:MM to
 *              register a daily mission and by the automission
 *              command to list entries with a live countdown to
 *              the next scheduled run.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4KK1_AM_SCHED_H_
#define _M4KK1_AM_SCHED_H_

/* NOTE: m4sh.h has no include guard — include it exactly once in
 * the .c file BEFORE this header (all types/helpers used here are
 * declared there). */

#define AM_SCHED_PATH   "/etc/automission"
#define AM_NAME_MAX     16
#define AM_MAX_ENTRIES  8

struct am_sched {
	char name[AM_NAME_MAX];
	unsigned char hh;
	unsigned char mm;
};

/**
 * am_parse_hhmm - parse a strict "HH:MM" string
 * @s: input, must be exactly 5 chars digits:digits
 *
 * Return: minutes since midnight, or -1 on malformed input
 */
static int am_parse_hhmm(const char *s)
{
	if (!s)
		return -1;
	if (!(s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9'))
		return -1;
	if (s[2] != ':')
		return -1;
	if (!(s[3] >= '0' && s[3] <= '9' && s[4] >= '0' && s[4] <= '9'))
		return -1;
	if (s[5] != 0)
		return -1;
	int h = (s[0] - '0') * 10 + (s[1] - '0');
	int m = (s[3] - '0') * 10 + (s[4] - '0');
	if (h > 23 || m > 59)
		return -1;
	return h * 60 + m;
}

/**
 * am_sched_write - persist one "backup HH:MM" mission line
 * @hhmm: minutes since midnight
 *
 * The registry holds one entry per mission name; writing the
 * backup entry truncates and rewrites the file.
 */
static void am_sched_write(int hhmm)
{
	int fd = musr_sc_open((char *)AM_SCHED_PATH,
			      O_CREAT | O_WRONLY | O_TRUNC);
	if (fd < 0)
		return;
	char line[32];
	int o = 0;
	const char *p = "backup ";
	while (*p)
		line[o++] = *p++;
	line[o++] = '0' + hhmm / 600;
	line[o++] = '0' + (hhmm / 60) % 10;
	line[o++] = ':';
	line[o++] = '0' + (hhmm % 60) / 10;
	line[o++] = '0' + hhmm % 10 % 10;
	line[o++] = '\n';
	musr_sc_write(fd, line, o);
	musr_sc_close(fd);
}

/**
 * am_sched_load - read registered missions from /etc/automission
 * @out: caller array of entries
 * @max: array capacity
 *
 * Return: number of entries loaded (0 if file absent/unreadable)
 */
static int am_sched_load(struct am_sched *out, int max)
{
	char buf[256];
	int fd = musr_sc_open((char *)AM_SCHED_PATH, O_RDONLY);
	if (fd < 0)
		return 0;
	int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
	musr_sc_close(fd);
	if (n <= 0)
		return 0;
	buf[n] = 0;
	int cnt = 0;
	for (int i = 0; i < n && cnt < max;) {
		/* skip to line start */
		while (i < n && (buf[i] == '\n' || buf[i] == '\r'))
			i++;
		if (i >= n)
			break;
		int ls = i;
		while (i < n && buf[i] != '\n')
			i++;
		int le = i;
		if (le - ls < 7)
			continue;
		/* parse "<name> HH:MM" */
		int sp = ls;
		while (sp < le && buf[sp] != ' ')
			sp++;
		int nlen = sp - ls;
		if (nlen <= 0 || nlen >= AM_NAME_MAX || sp + 6 != le)
			continue;
		char hhmm[6];
		for (int k = 0; k < 5; k++)
			hhmm[k] = buf[sp + 1 + k];
		hhmm[5] = 0;
		int v = am_parse_hhmm(hhmm);
		if (v < 0)
			continue;
		for (int k = 0; k < nlen; k++)
			out[cnt].name[k] = buf[ls + k];
		out[cnt].name[nlen] = 0;
		out[cnt].hh = (unsigned char)(v / 60);
		out[cnt].mm = (unsigned char)(v % 60);
		cnt++;
	}
	return cnt;
}

/**
 * am_countdown_secs - seconds from now until the next HH:MM run
 * @hh: schedule hour
 * @mm: schedule minute
 *
 * Uses the RTC-backed wall clock (S_TIME).  If the slot already
 * passed today the countdown wraps to tomorrow.
 */
static uint32_t am_countdown_secs(unsigned char hh, unsigned char mm)
{
	uint32_t nows = (uint32_t)musr_sc_time() % 86400u;
	uint32_t sched = ((uint32_t)hh * 60u + mm) * 60u;
	return (sched + 86400u - nows) % 86400u;
}

/**
 * am_print_hhmmss - print a duration as zero-padded HH:MM:SS
 * @t: duration in seconds (< 24h)
 */
static void am_print_hhmmss(uint32_t t)
{
	uint32_t h = t / 3600u;
	uint32_t m = (t / 60u) % 60u;
	uint32_t s = t % 60u;
	if (h < 10)
		out_putc('0');
	print_u32(h);
	out_putc(':');
	if (m < 10)
		out_putc('0');
	print_u32(m);
	out_putc(':');
	if (s < 10)
		out_putc('0');
	print_u32(s);
}

#endif /* _M4KK1_AM_SCHED_H_ */
