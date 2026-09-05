/*
 * M4KK1 4P1 - tui_build.c
 * ncurses build dashboard for tools/build/build_krn.sh (host tool,
 * built with the WSL host gcc against ncurses; NOT part of the
 * freestanding OS image).
 *
 * Spawns the build script unchanged (stdout+stderr -> pipe), streams
 * its output into a scrollable log pane, tracks build stages, live
 * elapsed time.  C port of tools/build/tui_build.py.
 *
 * Hotkeys: f follow · s save log · PgUp/PgDn/↑↓ scroll · q kill+quit
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <ncurses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOG_MAX   20000
#define LOG_KEEP  10000
#define LINE_MAX  512

static const char *STAGES[] = {
	"Building M4SH userspace shell",
	"Building userspace GUI clients",
	"ALTR2",
	"Building kernel",
	"Building ISO image",
	"ISO built",
};
#define NSTAGES ((int)(sizeof(STAGES) / sizeof(STAGES[0])))
#define MAXI(a, b) ((a) > (b) ? (a) : (b))

/* ── ring of log lines (ANSI stripped in place) ── */
static char (*g_lines)[LINE_MAX];
static int g_nlines;
static int g_follow = 1;
static int g_view;

static void log_push(const char *raw)
{
	if (g_nlines >= LOG_MAX) {
		memmove(g_lines, g_lines + (LOG_MAX - LOG_KEEP),
			LOG_KEEP * LINE_MAX);
		g_nlines = LOG_KEEP;
	}
	char *dst = g_lines[g_nlines++];
	int o = 0;
	for (const char *p = raw; *p && o < LINE_MAX - 1; p++) {
		if (*p == '\x1b' && p[1] == '[') {
			p += 2;
			while (*p && !((*p >= 'A' && *p <= 'Z') ||
				       (*p >= 'a' && *p <= 'z')))
				p++;
			continue;
		}
		if (*p == '\n' || *p == '\r')
			continue;
		dst[o++] = *p;
	}
	dst[o] = 0;
	if (g_follow && g_nlines > 0)
		g_view = g_nlines - 1;
}

static void log_scroll(int dy, int ph)
{
	g_follow = 0;
	g_view += dy;
	if (g_view < 0) g_view = 0;
	if (g_view >= g_nlines) g_view = g_nlines - 1;
	(void)ph;
}

static const char *stage_of(const char *line)
{
	for (int i = 0; i < NSTAGES; i++)
		if (strstr(line, STAGES[i]))
			return STAGES[i];
	return NULL;
}

static const char *save_log(void)
{
	static char ret[128];
	char path[256], stamp[32];
	time_t t = time(NULL);

	strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S",
		 localtime(&t));
	mkdir("logs", 0777);	/* best effort */
	snprintf(path, sizeof(path), "logs/tui_buildc_%s.log", stamp);
	FILE *f = fopen(path, "w");
	if (!f) {
		snprintf(ret, sizeof(ret), "save FAILED");
		return ret;
	}
	for (int i = 0; i < g_nlines; i++)
		fprintf(f, "%s\n", g_lines[i]);
	fclose(f);
	snprintf(ret, sizeof(ret), "-> %s", strrchr(path, '/') + 1);
	return ret;
}

/* ── non-blocking buffered line reader over the child pipe ── */
static char g_rbuf[8192];
static int g_rlen, g_rpos;
static char g_pend[LINE_MAX];
static int g_pendlen;

/* returns 1: one line completed (in g_pend, NUL-terminated);
 *         0: no complete line right now;
 *        -1: EOF / error */
static int read_line(int fd)
{
	while (1) {
		while (g_rpos < g_rlen) {
			char ch = g_rbuf[g_rpos++];

			if (ch == '\n') {
				g_pend[g_pendlen] = 0;
				g_pendlen = 0;
				return 1;
			}
			if (g_pendlen < LINE_MAX - 1)
				g_pend[g_pendlen++] = ch;
		}
		fd_set rf;
		struct timeval tv = { 0, 0 };

		FD_ZERO(&rf);
		FD_SET(fd, &rf);
		if (select(fd + 1, &rf, NULL, NULL, &tv) <= 0)
			return 0;
		ssize_t r = read(fd, g_rbuf, sizeof(g_rbuf));
		if (r <= 0)
			return -1;
		g_rlen = (int)r;
		g_rpos = 0;
	}
}

static void draw(const char *stage, int rc, double elapsed,
		 int running, const char *saved)
{
	int my, mx;

	getmaxyx(stdscr, my, mx);
	int ph = my - 3;
	erase();

	/* header */
	char head[160];
	snprintf(head, sizeof(head), " M4KK1 build — %s ",
		 stage ? stage : "starting");
	attron(A_BOLD | COLOR_PAIR(2));
	mvaddnstr(0, 0, head, mx - 1);
	for (int x = strlen(head); x < mx - 1; x++)
		addch(' ');
	attroff(A_BOLD | COLOR_PAIR(2));
	char info[64];
	snprintf(info, sizeof(info), "%s %6.1fs",
		 running ? "running" :
		 (rc == 0 ? "done rc=0" :
		  rc > 0 ? "done rc=ERR" : "killed"), elapsed);
	int ic = rc == 0 && !running ? 4 : (running ? 3 : 1);
	attron(COLOR_PAIR(ic));
	mvaddnstr(0, MAXI(0, mx - 1 - (int)strlen(info)), info, mx - 1);
	attroff(COLOR_PAIR(ic));

	/* log pane */
	int top = g_follow ? (g_nlines - ph > 0 ? g_nlines - ph : 0)
			   : g_view;
	for (int i = 0; i < ph; i++) {
		int idx = top + i;

		if (idx < 0 || idx >= g_nlines)
			break;
		int attr = A_NORMAL;
		if (strstr(g_lines[idx], "error") ||
		    strstr(g_lines[idx], "FAIL") ||
		    strstr(g_lines[idx], "fail"))
			attr = COLOR_PAIR(3);
		attron(attr);
		mvaddnstr(1 + i, 0, g_lines[idx], mx - 1);
		attroff(attr);
	}

	/* progress bar by stage index */
	int frac_i = 0;
	if (stage)
		for (int i = 0; i < NSTAGES; i++)
			if (strcmp(STAGES[i], stage) == 0) {
				frac_i = i + 1;
				break;
			}
	int bar_w = mx - 2 < 50 ? mx - 2 : 50;
	int filled = bar_w * frac_i / NSTAGES;
	attron(COLOR_PAIR(4));
	mvaddch(my - 2, 0, ' ');
	for (int i = 0; i < bar_w; i++)
		addch(i < filled ? '#' : '-');
	attroff(COLOR_PAIR(4));

	/* footer */
	char foot[256];
	snprintf(foot, sizeof(foot),
		 "[f]ollow=%s  [s]ave log  [PgUp/PgDn] scroll  [q]uit %s",
		 g_follow ? "on" : "off", saved);
	attron(A_REVERSE);
	mvaddnstr(my - 1, 0, foot, mx - 1);
	int footlen = (int)strlen(foot);

	for (int x = footlen < mx - 1 ? footlen : mx - 1;
	     x < mx - 1; x++)
		addch(' ');
	attroff(A_REVERSE);
	refresh();
}

static void handle_key(int k, int ph, const char **saved)
{
	if (k == 'f') {
		g_follow = !g_follow;
	} else if (k == 's') {
		*saved = save_log();
	} else if (k == KEY_PPAGE) {
		log_scroll(-(ph - 1), ph);
	} else if (k == KEY_NPAGE) {
		log_scroll(ph - 1, ph);
	} else if (k == KEY_UP) {
		log_scroll(-1, ph);
	} else if (k == KEY_DOWN) {
		log_scroll(1, ph);
	}
}

static double since(struct timespec t0)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - t0.tv_sec)
		+ (now.tv_nsec - t0.tv_nsec) / 1e9;
}

/* Walk up from /proc/self/exe until a Makefile + tools/build exist;
 * returns 0 with cwd at the repo root. */
static int chdir_to_repo_root(void)
{
	char path[512];
	ssize_t r = readlink("/proc/self/exe", path,
			     sizeof(path) - 1);

	if (r <= 0)
		return -1;
	path[r] = 0;
	char *slash = strrchr(path, '/');

	if (slash)
		*slash = 0;
	for (int i = 0; i < 5; i++) {
		struct stat st;

		if (stat("Makefile", &st) == 0 &&
		    stat("tools/build", &st) == 0)
			return 0;
		if (chdir("..") != 0)
			return -1;
	}
	return -1;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr,
			"usage: %s <build-mode>   (e.g. --full-test)\n",
			argv[0]);
		return 2;
	}
	if (chdir_to_repo_root() != 0) {	/* helper below */
		fprintf(stderr, "tui_buildc: cannot locate repo root\n");
		return 2;
	}

	int pfd[2];
	if (pipe(pfd) != 0) {
		perror("pipe");
		return 2;
	}
	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		return 2;
	}
	if (child == 0) {
		close(pfd[0]);
		dup2(pfd[1], STDOUT_FILENO);
		dup2(pfd[1], STDERR_FILENO);
		close(pfd[1]);
		execl("/bin/bash", "bash", "tools/build/build_krn.sh",
		      argv[1], (char *)NULL);
		_exit(127);
	}
	close(pfd[1]);
	int outfd = pfd[0];
	int fl = fcntl(outfd, F_GETFL);
	fcntl(outfd, F_SETFL, fl | O_NONBLOCK);

	g_lines = malloc(LOG_MAX * LINE_MAX);
	if (!g_lines) {
		perror("malloc");
		return 2;
	}

	const char *stage = NULL;
	const char *saved = "";
	int rc = -1;

	initscr();
	start_color();
	use_default_colors();
	init_pair(1, COLOR_WHITE, -1);
	init_pair(2, COLOR_CYAN, -1);
	init_pair(3, COLOR_RED, -1);
	init_pair(4, COLOR_GREEN, -1);
	curs_set(0);
	timeout(100);
	keypad(stdscr, TRUE);
	ESCDELAY = 25;

	struct timespec t0;

	clock_gettime(CLOCK_MONOTONIC, &t0);

	while (1) {
		/* drain all pending lines (batch, avoid pipe backpressure) */
		int drained = 0;
		int eof = 0;

		while (drained < 1000) {
			int got = read_line(outfd);

			if (got == 1) {
				log_push(g_pend);
				const char *s = stage_of(g_pend);

				if (s)
					stage = s;
				drained++;
			} else if (got < 0) {
				eof = 1;
				break;
			} else {
				break;
			}
		}
		if (eof) {
			int st = 0;

			waitpid(child, &st, 0);
			close(outfd);
			rc = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
			break;
		}

		draw(stage, rc, since(t0), 1, saved);
		int k = getch();
		int my, mx;

		getmaxyx(stdscr, my, mx);
		(void)mx;
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

	/* finished: hold the tail until the user leaves */
	while (1) {
		draw(stage, rc, since(t0), 0, saved);
		int k = getch();
		int my, mx;

		getmaxyx(stdscr, my, mx);
		(void)mx;
		if (k == 'q' || k == 27)
			break;
		handle_key(k, my - 3, &saved);
	}
	endwin();
	free(g_lines);
	return rc == 0 ? 0 : (rc < 0 ? 1 : rc);
}
