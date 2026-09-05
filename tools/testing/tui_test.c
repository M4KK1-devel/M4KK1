/*
 * M4KK1 4P1 - tui_test.c
 * ncurses test dashboard for tools/testing/test_all.sh (host tool,
 * WSL gcc + libncurses).  C port of tui_test.py.
 *
 * Runs the test script unchanged as a child process and parses its
 * live output (ANSI-colour stripped before matching):
 *   "=== 测试N: ... ==="  → section header
 *   "[PASS] ..." / "[FAIL] ..." → tally
 *   "通过: N" / "失败: N"      → script's own final tally (cross-check)
 *
 * UI: header (running/rc, PASS/FAIL counters, section), scrollable
 * log pane (PASS green / FAIL red), footer hotkeys: f follow, s save
 * report to logs/tui_test_<ts>.log, PgUp/PgDn/UP/DOWN scroll, q quit.
 * Exit code = child exit code.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>

#define LINE_MAX 512
#define RING_CAP 20000
#define MAXI(a, b) ((a) > (b) ? (a) : (b))

/* ── line ring (stores ANSI-stripped lines) ── */
static char (*g_ring)[LINE_MAX];
static int g_count;
static int g_follow = 1;
static int g_view;              /* top visible row when not following */

static void strip_ansi(char *s)
{
	char *r = s, *w = s;
	while (*r) {
		if (*r == '\x1b') {
			r++;
			if (*r == '[') {
				r++;
				while (*r && (*r < 0x40 || *r > 0x7e))
					r++;
				if (*r) r++;
			} else if (*r) {
				r++;
			}
		} else {
			*w++ = *r++;
		}
	}
	*w = 0;
}

static void log_push(const char *line)
{
	if (!g_ring) g_ring = malloc(RING_CAP * LINE_MAX);
	if (!g_ring) return;
	if (g_count == RING_CAP) {
		memmove(g_ring, g_ring + 1, (RING_CAP - 1) * LINE_MAX);
		g_count--;
	}
	snprintf(g_ring[g_count], LINE_MAX, "%s", line);
	g_count++;
	if (g_follow)
		g_view = g_count > 0 ? g_count - 1 : 0;
}

/* ── parsing ── */
static int g_pass, g_fail;         /* our tally */
static int g_spass, g_sfail = -1;  /* script's own tally (-1 unknown) */
static char g_section[LINE_MAX];

static void parse_line(const char *s)
{
	if (strncmp(s, "=== ", 4) == 0) {
		snprintf(g_section, LINE_MAX, "%s", s + 4);
		char *e = strstr(g_section, "===");
		if (e) {
			while (e > g_section && (e[-1] == ' ' || e[-1] == '=' ))
				e--;
			*e = 0;
		}
		return;
	}
	if (strncmp(s, "[PASS]", 6) == 0) { g_pass++; return; }
	if (strncmp(s, "[FAIL]", 6) == 0) { g_fail++; return; }
	int a, b;
	if (sscanf(s, "通过: %d", &a) == 1) g_spass = a;
	if (sscanf(s, "失败: %d", &b) == 1) g_sfail = b;
}

/* ── buffered non-blocking line reader over the child pipe ── */
static int g_fd;
static char g_rbuf[8192];
static int g_rlen, g_rpos;
static char g_pend[LINE_MAX];
static int g_pendlen;

/* returns 1: line ready in *out, 0: no data, -1: EOF */
static int read_line(char *out, int outsz)
{
	while (1) {
		while (g_rpos < g_rlen) {
			char ch = g_rbuf[g_rpos++];
			if (ch == '\n') {
				g_pend[g_pendlen] = 0;
				strip_ansi(g_pend);   /* colour codes wrap [PASS] */
				snprintf(out, outsz, "%s", g_pend);
				g_pendlen = 0;
				return 1;
			}
			if (ch != '\r' && g_pendlen < LINE_MAX - 1)
				g_pend[g_pendlen++] = ch;
		}
		fd_set rf;
		struct timeval tv = { 0, 0 };

		FD_ZERO(&rf);
		FD_SET(g_fd, &rf);
		if (select(g_fd + 1, &rf, NULL, NULL, &tv) <= 0)
			return 0;
		ssize_t r = read(g_fd, g_rbuf, sizeof(g_rbuf));
		if (r <= 0)
			return -1;
		g_rlen = (int)r;
		g_rpos = 0;
	}
}

/* ── UI ── */
static double since(clock_t t0)
{
	return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

static void draw(int running, int rc, double elapsed, const char *saved)
{
	erase();
	int my, mx;

	getmaxyx(stdscr, my, mx);
	int ph = my - 3;
	char head[160];

	if (running)
		snprintf(head, sizeof(head),
			 " M4KK1 make test — running %5.1fs", elapsed);
	else
		snprintf(head, sizeof(head),
			 " M4KK1 make test — %s %5.1fs",
			 rc == 0 ? "ALL PASS rc=0" : "FAILURES", elapsed);
	attron(A_BOLD | COLOR_PAIR(running ? 3 : (rc == 0 ? 4 : 1)));
	mvaddnstr(0, 0, head, mx - 1);
	attroff(A_BOLD | COLOR_PAIR(running ? 3 : (rc == 0 ? 4 : 1)));
	char cnt[80];

	snprintf(cnt, sizeof(cnt), "PASS %d  FAIL %d%s", g_pass, g_fail,
		 g_sfail >= 0 ? "  (script: P/F)" : "");
	mvaddnstr(0, MAXI(0, mx - 1 - (int)strlen(cnt)), cnt, mx - 1);
	if (g_section[0])
		mvaddnstr(1, 0, g_section, mx - 1);

	int top = g_follow ? MAXI(0, g_count - ph) : g_view;
	for (int i = 0; i < ph; i++) {
		int idx = top + i;
		if (idx >= g_count)
			break;
		int attr = COLOR_PAIR(2);
		if (strncmp(g_ring[idx], "[PASS]", 6) == 0)
			attr = COLOR_PAIR(3);
		else if (strncmp(g_ring[idx], "[FAIL]", 6) == 0)
			attr = COLOR_PAIR(1) | A_BOLD;
		attron(attr);
		mvaddnstr(2 + i, 0, g_ring[idx], mx - 1);
		attroff(attr);
	}
	char foot[160];

	snprintf(foot, sizeof(foot),
		 " [f]ollow=%s  [s]ave report  [PgUp/PgDn] scroll  [q]uit %s",
		 g_follow ? "on" : "off", saved);
	attron(A_REVERSE);
	mvaddnstr(my - 1, 0, foot, mx - 1);
	attroff(A_REVERSE);
	refresh();
}

static void save_report(const char **outname)
{
	char path[160];
	char ts[64];
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);
	mkdir("logs", 0777);
	snprintf(path, sizeof(path), "logs/tui_test_%s.log", ts);
	FILE *f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "# make test report %s  PASS=%d FAIL=%d\n",
		ts, g_pass, g_fail);
	for (int i = 0; i < g_count; i++)
		fprintf(f, "%s\n", g_ring[i]);
	fclose(f);
	*outname = strdup(path + 5);
}

static void handle_key(int k, int ph, const char **saved)
{
	if (k == 'f')
		g_follow = !g_follow;
	else if (k == 's')
		save_report(saved);
	else if (k == KEY_PPAGE)
		{ g_follow = 0; g_view = MAXI(0, g_view - ph); }
	else if (k == KEY_NPAGE)
		{ g_follow = 0; g_view += ph; if (g_view >= g_count) g_view = g_count - 1; }
	else if (k == KEY_UP)
		{ g_follow = 0; g_view = MAXI(0, g_view - 1); }
	else if (k == KEY_DOWN)
		{ g_follow = 0; g_view += 1; if (g_view >= g_count) g_view = g_count - 1; }
}

int main(void)
{
	/* spawn: bash tools/testing/test_all.sh, stdout+stderr -> pipe */
	int pfd[2];
	if (pipe(pfd) != 0) { perror("pipe"); return 2; }
	pid_t child = fork();
	if (child < 0) { perror("fork"); return 2; }
	if (child == 0) {
		close(pfd[0]);
		dup2(pfd[1], STDOUT_FILENO);
		dup2(pfd[1], STDERR_FILENO);
		close(pfd[1]);
		execl("/bin/bash", "bash",
		      getenv("TUI_TEST_SCRIPT")
			  ? getenv("TUI_TEST_SCRIPT")
			  : "tools/testing/test_all.sh",
		      (char *)NULL);
		_exit(127);
	}
	close(pfd[1]);
	g_fd = pfd[0];
	int fl = fcntl(g_fd, F_GETFL);
	fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);

	initscr();
	if (has_colors()) {
		start_color();
		use_default_colors();
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_WHITE, -1);
		init_pair(3, COLOR_GREEN, -1);
		init_pair(4, COLOR_CYAN, -1);
	}
	curs_set(0);
	keypad(stdscr, TRUE);
	noecho();
	timeout(100);

	const char *saved = "";
	clock_t t0 = clock();
	int rc = -1;
	char line[LINE_MAX];

	while (1) {
		int eof = 0, drained = 0;
		while (drained < 500) {
			int r = read_line(line, sizeof(line));
			if (r == 1) {
				log_push(line);
				parse_line(g_ring[g_count - 1]);
				drained++;
			} else if (r == -1) {
				eof = 1;
				break;
			} else
				break;
		}
		if (eof) {
			int st = 0;
			waitpid(child, &st, 0);
			close(g_fd);
			rc = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
			break;
		}
		int my, mx;

		getmaxyx(stdscr, my, mx);
		(void)mx;
		draw(1, rc, since(t0), saved);
		int k = getch();
		if (k == 'q' || k == 27) {
			endwin();
			kill(child, SIGTERM);
			int st;
			waitpid(child, &st, 0);
			fprintf(stderr, "killed by user\n");
			return 130;
		}
		handle_key(k, my - 3, &saved);
	}

	/* finished: show result until user leaves */
	while (1) {
		int my, mx;

		getmaxyx(stdscr, my, mx);
		(void)mx;
		draw(0, rc, since(t0), saved);
		int k = getch();
		if (k == 'q' || k == 27)
			break;
		if (k == 's')
			save_report(&saved);
		handle_key(k, my - 3, &saved);
	}
	endwin();
	printf("make test: PASS %d  FAIL %d (script tally %d/%d) rc=%d\n",
	       g_pass, g_fail, g_spass, g_sfail, rc);
	return rc;
}
