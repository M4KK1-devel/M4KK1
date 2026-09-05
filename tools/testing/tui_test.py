#!/usr/bin/env python3
"""M4KK1 4P1 - tui_test.py
ncurses dashboard for `make test` (tools/testing/test_all.sh).

Runs the test script unchanged as a subprocess and parses its
output live:
  "=== 测试N: <title> ==="  -> section
  "[PASS] <msg>"            -> green pass entry (count)
  "[FAIL] <msg>"            -> red fail entry (count + list)
  "[INFO] <msg>"            -> info line (log only)
  "通过: N" / "失败: N"      -> script's own tally (cross-check)

Hotkeys: f follow toggle · s save report · PgUp/PgDn/↑↓ scroll ·
q quit.  Exit code mirrors the script's (0 all pass).
"""
import curses, os, re, selectors, subprocess, sys, time

ROOT = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LOGDIR = os.path.join(ROOT, "logs")

RE_SEC = re.compile(r"^===\s*(.+?)\s*===$")
RE_PASS = re.compile(r"^\[PASS\]\s*(.*)$")
RE_FAIL = re.compile(r"^\[FAIL\]\s*(.*)$")
RE_INFO = re.compile(r"^\[INFO\]\s*(.*)$")
RE_TALLY = re.compile(r"^通过:\s*(\d+)")
RE_TALLY_F = re.compile(r"^失败:\s*(\d+)")


class Log:
    def __init__(self):
        self.lines = []
        self.follow = True
        self.view = 0

    def push(self, raw):
        txt = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "",
                     raw.rstrip("\r\n"))
        self.lines.append(txt)
        if len(self.lines) > 20000:
            self.lines = self.lines[-10000:]
        if self.follow:
            self.view = max(0, len(self.lines) - 1)

    def scroll(self, dy, ph):
        self.follow = False
        self.view = max(0, min(len(self.lines) - 1,
                               self.view + dy))

    def page(self, dp, ph):
        self.scroll(dp * max(1, ph - 1), ph)


def stage_of(line):
    """live counters are kept by the caller; this returns the new
    section title when the line opens one, else None."""
    m = RE_SEC.match(line)
    return m.group(1) if m else None


def draw(scr, log, section, npass, nfail, rc, elapsed,
         running, saved):
    scr.erase()
    my, mx = scr.getmaxyx()
    ph = my - 3

    # header: title + live tally
    title = " M4KK1 make test — %s " % (section or "starting")
    try:
        scr.addstr(0, 0, title[:mx - 1].ljust(mx - 1),
                   curses.A_BOLD | curses.color_pair(2))
    except curses.error:
        pass
    tally = "PASS %d  FAIL %d  %s %5.1fs" % (
        npass, nfail,
        "running" if running else
        ("done rc=%s" % rc if rc is not None else "killed"),
        elapsed)
    try:
        scr.addstr(0, max(0, mx - 1 - len(tally)), tally[:mx - 1],
                   curses.color_pair(
                       4 if not running and nfail == 0 else
                       (3 if nfail else 4)))
    except curses.error:
        pass

    # log pane (pass/fail lines highlighted)
    top = log.view if not log.follow else max(
        0, len(log.lines) - ph)
    for i in range(ph):
        idx = top + i
        if idx >= len(log.lines):
            break
        l = log.lines[idx]
        attr = (curses.color_pair(4) if RE_PASS.match(l)
                else curses.A_BOLD | curses.color_pair(3)
                if RE_FAIL.match(l) else curses.A_NORMAL)
        try:
            scr.addstr(1 + i, 0, l[:mx - 1], attr)
        except curses.error:
            pass

    # footer
    foot = ("[f]ollow=%s  [s]ave report  "
            "[PgUp/PgDn] scroll  [q]uit %s"
            % (log.follow and "on" or "off", saved))
    try:
        scr.addstr(my - 1, 0, foot[:mx - 1].ljust(mx - 1),
                   curses.A_REVERSE)
    except curses.error:
        pass
    scr.timeout(100)


def save_report(log, npass, nfail):
    os.makedirs(LOGDIR, exist_ok=True)
    path = os.path.join(
        LOGDIR, "tui_test_%s.log"
        % time.strftime("%Y%m%d_%H%M%S"))
    with open(path, "w") as f:
        f.write("# make test report %s  PASS=%d FAIL=%d\n"
                % (time.strftime("%F %T"), npass, nfail))
        f.write("\n".join(log.lines))
    return "→ %s" % os.path.basename(path)


def main(scr, argv):
    curses.curs_set(0)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_WHITE, -1)
    curses.init_pair(2, curses.COLOR_CYAN, -1)
    curses.init_pair(3, curses.COLOR_RED, -1)
    curses.init_pair(4, curses.COLOR_GREEN, -1)

    script = os.path.join(ROOT, "tools", "testing",
                          "test_all.sh")
    cmd = ["bash", script]
    proc = subprocess.Popen(
        cmd, cwd=ROOT, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, bufsize=1)

    log = Log()
    section = None
    npass = nfail = 0
    script_pass = script_fail = None
    saved = ""
    rc = None
    t0 = time.time()

    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)

    scr.timeout(100)
    while True:
        drained = 0
        while drained < 500:
            events = sel.select(timeout=0)
            if not events:
                break
            for key, _ in events:
                line = key.fileobj.readline()
                if not line:
                    break
                log.push(line)
                # parse on the STRIPPED line: the script wraps
                # [PASS]/[FAIL] in ANSI colour codes, so the raw
                # line never matches the regexes
                clean = log.lines[-1]
                s = stage_of(clean)
                if s:
                    section = s
                if RE_PASS.match(clean):
                    npass += 1
                elif RE_FAIL.match(clean):
                    nfail += 1
                m = RE_TALLY.match(clean)
                if m:
                    script_pass = int(m.group(1))
                m = RE_TALLY_F.match(clean)
                if m:
                    script_fail = int(m.group(1))
                drained += 1
        if proc.poll() is not None:
            for line in proc.stdout:
                log.push(line)
                clean = log.lines[-1]
                if RE_PASS.match(clean):
                    npass += 1
                elif RE_FAIL.match(clean):
                    nfail += 1
            rc = proc.returncode
            break

        draw(scr, log, section, npass, nfail, rc,
             time.time() - t0, True, saved)
        k = scr.getch()
        ph = scr.getmaxyx()[0] - 3
        if k in (ord('q'), 27):
            if proc.poll() is None:
                proc.terminate()
            return 130
        elif k == ord('f'):
            log.follow = not log.follow
        elif k == ord('s'):
            saved = save_report(log, npass, nfail)
        elif k == curses.KEY_PPAGE:
            log.page(-1, ph)
        elif k == curses.KEY_NPAGE:
            log.page(1, ph)
        elif k == curses.KEY_UP:
            log.scroll(-1, ph)
        elif k == curses.KEY_DOWN:
            log.scroll(1, ph)

    # finished: show tail until user leaves
    while True:
        draw(scr, log, section, npass, nfail, rc,
             time.time() - t0, False, saved)
        k = scr.getch()
        ph = scr.getmaxyx()[0] - 3
        if k in (ord('q'), 27):
            break
        elif k == ord('s'):
            saved = save_report(log, npass, nfail)
        elif k == curses.KEY_PPAGE:
            log.page(-1, ph)
        elif k == curses.KEY_NPAGE:
            log.page(1, ph)
        elif k == curses.KEY_UP:
            log.scroll(-1, ph)
        elif k == curses.KEY_DOWN:
            log.scroll(1, ph)

    # cross-check our tally with the script's own summary
    if (script_pass is not None
            and script_pass != npass):
        log.lines.append(
            "tui_test: tally mismatch — parsed PASS %d "
            "vs script %s" % (npass, script_pass))
    return rc if rc is not None else 1


def run():
    os.environ.setdefault("ESCDELAY", "25")
    try:
        return curses.wrapper(
            lambda s: main(s, sys.argv[1:]))
    except RuntimeError as e:
        print("tui_test: %s (need >= 80x24 terminal)" % e,
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(run())
