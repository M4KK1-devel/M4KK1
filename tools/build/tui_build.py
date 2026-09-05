#!/usr/bin/env python3
"""M4KK1 4P1 - tui_build.py
ncurses build dashboard for build_krn.sh.

Runs the (unchanged) build script as a subprocess, streams its output
into a scrollable log pane, tracks stage progress from the script's
own "=== stage ===" banners, and offers hotkeys:

    f  follow tail          s  save log to logs/
    PgUp/PgDn scroll        q  quit (detach; build keeps running if
                                the child still lives — we kill it
                                only on Ctrl-C style hard exit)

Usage:  tui_build.py [--full|--full-test|--cmd-only|--minimal]
                     [--output DIR] [-- passthrough args]
"""
import curses
import glob
import os
import re
import selectors
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "tools", "build", "build_krn.sh")
LOGDIR = os.path.join(ROOT, "logs")

STAGES = [
    "Building M4SH userspace shell",
    "Building userspace GUI clients",
    "ALTR2",
    "Building kernel",
    "Building ISO image",
    "ISO built",
]


def stage_of(line):
    for s in STAGES:
        if s in line:
            return s
    return None


class Log:
    def __init__(self):
        self.lines = []
        self.follow = True
        self.view = 0          # index of top visible line

    def push(self, raw):
        txt = raw.rstrip("\r\n")
        # strip ANSI colour escapes for display
        txt = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", txt)
        self.lines.append(txt)
        if len(self.lines) > 20000:
            self.lines = self.lines[-10000:]
        if self.follow:
            self.view = max(0, len(self.lines) - 1)

    def scroll(self, dy, ph):
        self.follow = False
        self.view = max(0, min(len(self.lines) - 1,
                               self.view + dy))

    def page(self, n, ph):
        self.scroll(n * (ph - 1), ph)


def draw(scr, log, stage, rc, elapsed, running, saved):
    scr.erase()
    my, mx = scr.getmaxyx()
    ph = my - 3

    # header
    title = " M4KK1 build — %s " % (stage or "starting")
    scr.addstr(0, 0, title[:mx - 1][:60].ljust(mx - 1),
               curses.A_BOLD | curses.color_pair(2))
    info = "%s  %5.1fs" % ("running" if running else
                           ("done rc=%d" % rc if rc is not None
                            else "killed"),
                           elapsed)
    try:
        scr.addstr(0, max(0, mx - 1 - len(info)), info[:mx - 1],
                   curses.color_pair(4 if not running else 3))
    except curses.error:
        pass

    # log pane
    top = log.view if not log.follow else max(0, len(log.lines) - ph)
    for i in range(ph):
        idx = top + i
        if idx >= len(log.lines):
            break
        attr = (curses.color_pair(3)
                if "error" in log.lines[idx].lower()
                or "fail" in log.lines[idx].lower()
                else curses.A_NORMAL)
        try:
            scr.addstr(1 + i, 0, log.lines[idx][:mx - 1], attr)
        except curses.error:
            pass

    # progress bar
    frac = 0.0
    if stage:
        for i, s in enumerate(STAGES):
            if s.startswith(stage) or stage.startswith(s):
                frac = (i + 1) / len(STAGES)
                break
    bar_w = min(mx - 2, 50)
    filled = int(frac * bar_w)
    bar = "█" * filled + "░" * (bar_w - filled)
    scr.addstr(my - 2, 1, bar[:bar_w], curses.color_pair(4))

    # footer
    mode = ("[f]ollow=%s  [s]ave log  [PgUp/PgDn] scroll  "
            "[q]uit %s" % (log.follow and "on" or "off", saved))
    scr.addstr(my - 1, 0, mode[:mx - 1], curses.A_REVERSE)
    scr.timeout(100)


def main(scr, argv):
    curses.curs_set(0)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_WHITE, -1)
    curses.init_pair(2, curses.COLOR_CYAN, -1)
    curses.init_pair(3, curses.COLOR_RED, -1)
    curses.init_pair(4, curses.COLOR_GREEN, -1)

    log = Log()
    stage = None
    rc = None
    saved = ""
    t0 = time.time()

    cmd = ["bash", SCRIPT] + argv
    proc = subprocess.Popen(
        cmd, cwd=ROOT, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, bufsize=1)

    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)

    scr.timeout(100)
    while True:
        # drain ALL pending output without blocking the UI — a
        # single readline() per 100ms tick could not keep up with
        # gcc's output rate and back-pressured the build pipe
        drained = 0
        while drained < 500:
            events = sel.select(timeout=0)
            if not events:
                break
            for key, _ in events:
                line = key.fileobj.readline()
                if line:
                    log.push(line)
                    s = stage_of(line)
                    if s:
                        stage = s
                    drained += 1
                else:
                    break
        if proc.poll() is not None:
            # flush any tail
            for line in proc.stdout:
                log.push(line)
            rc = proc.returncode
            break

        draw(scr, log, stage, rc, time.time() - t0,
             proc.poll() is None, saved)
        k = scr.getch()
        ph = scr.getmaxyx()[0] - 3
        if k in (ord('q'), 27):
            if proc.poll() is None:
                proc.terminate()
            return proc.returncode if proc.returncode is not None \
                else 130
        if k == ord('f'):
            log.follow = not log.follow
        elif k == ord('s'):
            os.makedirs(LOGDIR, exist_ok=True)
            path = os.path.join(
                LOGDIR, "tui_build_%s.log"
                % time.strftime("%Y%m%d_%H%M%S"))
            with open(path, "w") as f:
                f.write("\n".join(log.lines))
            saved = "→ %s" % os.path.basename(path)
        elif k == curses.KEY_PPAGE:
            log.page(-1, ph)
        elif k == curses.KEY_NPAGE:
            log.page(1, ph)
        elif k == curses.KEY_UP:
            log.scroll(-1, ph)
        elif k == curses.KEY_DOWN:
            log.scroll(1, ph)

    # finished: show tail until user leaves
    saved = ""
    while True:
        draw(scr, log, stage, rc, time.time() - t0, False, saved)
        k = scr.getch()
        ph = scr.getmaxyx()[0] - 3
        if k in (ord('q'), 27, ord('s')):
            if k == ord('s'):
                os.makedirs(LOGDIR, exist_ok=True)
                path = os.path.join(
                    LOGDIR, "tui_build_%s.log"
                    % time.strftime("%Y%m%d_%H%M%S"))
                with open(path, "w") as f:
                    f.write("\n".join(log.lines))
                saved = "→ %s" % os.path.basename(path)
                continue
            break
        if k == curses.KEY_PPAGE:
            log.page(-1, ph)
        elif k == curses.KEY_NPAGE:
            log.page(1, ph)
        elif k == curses.KEY_UP:
            log.scroll(-1, ph)
        elif k == curses.KEY_DOWN:
            log.scroll(1, ph)
        elif k == ord('f'):
            log.follow = not log.follow
    return rc


def run():
    os.environ.setdefault("ESCDELAY", "25")
    argv = sys.argv[1:]
    try:
        return curses.wrapper(lambda s: main(s, argv))
    except RuntimeError as e:
        print("tui_build: %s (need >= 80x24 terminal)" % e,
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(run())
