#!/usr/bin/env python3
"""info-proc-list probe: boot the fresh full-test ISO, wait for the
m4sh prompt, spawn /bin/info, then drive it through QMP sendkey
(PS/2 path -> Sprach mailbox): Tab into the process page, N to page
forward, P back, Tab home.  Verifies the serial [INFO] PROC dumps at
every step and cross-checks the table against `ps` output.

Robust against interleaved output: instead of byte-offset cut points
(ANSI escapes split across reads corrupt them), we count occurrences
of marker lines and diff counts across steps."""
import subprocess, re, sys, time, socket, glob, os, json

cands = sorted(glob.glob("output/m4kk1_*-full-test.iso"), key=os.path.getmtime)
ISO = cands[-1].replace("\r", "")
print("ISO:", ISO, flush=True)

PORT = 4483
SERV_SOCK = "/tmp/info_probe_ser.sock"

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none",
     "-net", "none", "-no-reboot",
     "-chardev", f"socket,id=ser0,path={SERV_SOCK},server=on,wait=off",
     "-serial", "chardev:ser0",
     "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

buf = b""
t0 = time.time()
ser = None

def ser_connect():
    global ser
    for _ in range(30):
        try:
            ser = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            ser.connect(SERV_SOCK)
            return True
        except OSError:
            time.sleep(1)
    return False

def readserial():
    global buf
    quiet = 0
    while quiet < 2:
        try:
            ser.settimeout(0.5)
            c = ser.recv(65536)
            if c:
                buf += c
                quiet = 0
            else:
                quiet += 1
        except socket.timeout:
            quiet += 1

def plain():
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", buf)

s = None

def qmp(obj):
    s.sendall((json.dumps(obj) + "\r\n").encode())
    time.sleep(0.05)
    try:
        s.settimeout(0.5)
        while True:
            l = s.recv(65536)
            if not l:
                break
    except socket.timeout:
        pass

def sendkey(name, wait=3.0):
    qmp({"execute": "send-key", "arguments": {
        "keys": [{"type": "qcode", "data": name}]}})
    time.sleep(wait)

def shcmd(line, wait=3.0):
    try:
        ser.sendall(line.encode() + b"\n")
    except Exception as e:
        print("serial err", e, flush=True)
    time.sleep(wait)

checks = []

def check(name, ok, detail=""):
    checks.append((name, ok))
    print(("PASS" if ok else "FAIL") + ": " + name +
          ("  " + detail if detail else ""), flush=True)

try:
    if not ser_connect():
        raise RuntimeError("no serial socket")
    prompt = False
    while time.time() - t0 < 150:
        time.sleep(2)
        readserial()
        if b"m4sh ~>" in plain():
            prompt = True
            break
    check("m4sh prompt seen", prompt)
    if not prompt:
        raise RuntimeError("no prompt")

    time.sleep(2)

    # baseline process table from ps
    shcmd("ps", 4.0)
    readserial()
    tbls = re.findall(rb"PID STATE  NAME(.*?)Total:", plain(), re.S)
    ps_rows = []
    if tbls:
        for ln in tbls[-1].split(b"\n")[1:]:
            ln = re.sub(rb"\x1b\[[0-9;]*m", b"", ln).strip()
            mm = re.match(rb"(\d+)\s+(\w+)\s+(\S+)", ln)
            if mm:
                ps_rows.append((int(mm.group(1)), mm.group(3).decode()))
    check("ps table parsed", len(ps_rows) >= 3, f"{len(ps_rows)} rows")

    # Multi-page coverage: top the table past one page (8 rows/page)
    # by spawning three extra GUI apps first.  With the base 6 procs
    # (copland/cptest/info/m4sht/mdm/sprach) + clock + logview + calcg
    # the table reaches 9 rows -> 2 pages, so N/P actually flip pages
    # instead of exercising the clamp path only.
    # Wait for each app's "surface ready" banner BEFORE the next spawn:
    # surface creation is async and a late-comer (calc's surface landed
    # after info's in an earlier run) stacks ABOVE info and steals the
    # QMP keystrokes.  Serializing on the banners keeps info top-most.
    banners = {"/bin/clock": b"[CLOCK] surface ready",
               "/bin/logview": b"[LOGVIEW] surface ready",
               "/bin/calcg": b"[CALC] surface ready"}
    for app, banner in banners.items():
        mark = len(buf)
        shcmd(f"spawn {app}", 0.5)
        up = False
        t0 = time.time()
        while time.time() - t0 < 20:
            readserial()
            if banner in re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", buf[mark:]):
                up = True
                break
            time.sleep(1.0)
        check(f"{app} up", up)
        time.sleep(1.0)

    # spawn info (graphics: banner on serial) — wait for the banner so
    # the later checks never race a slow execve.
    mark = len(buf)
    shcmd("spawn /bin/info", 0.5)
    info_up = False
    t0 = time.time()
    while time.time() - t0 < 20:
        readserial()
        if b"[INFO] surface ready" in re.sub(
                rb"\x1b\[[0-9;]*[A-Za-z]", b"", buf[mark:]):
            info_up = True
            break
        time.sleep(1.0)
    check("info surface ready", info_up)

    # connect QMP for sendkey
    for _ in range(20):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
            break
        except OSError:
            time.sleep(1)
    if s is None:
        raise RuntimeError("no QMP")
    s.settimeout(2.0)
    try:
        s.recv(65536)
    except Exception:
        pass
    qmp({"execute": "qmp_capabilities"})

    # Tab -> process page.  Before the key the app must be on the sys
    # page (no PROC dumps); after it, dumps appear.
    readserial()
    before = plain().count(b"[INFO] PROC p=")
    sendkey("tab", 3.0)
    readserial()
    after = plain().count(b"[INFO] PROC p=")
    check("proc page serial dump", after > before,
          f"{before} -> {after}")

    blk = plain()
    m = re.search(rb"\[INFO\] PROC p=(\d+)/(\d+) n=(\d+)", blk)
    check("proc dump header parsed", m is not None)
    if m:
        pg, pgs, n = int(m.group(1)), int(m.group(2)), int(m.group(3))
        check("page 0 first", pg == 0)
        # ps ran BEFORE the extra apps and info were spawned; those add
        # +1 each (clock/logview/calcg/info) to the ps baseline.
        check("total matches ps+4", n == len(ps_rows) + 4,
              f"info n={n} ps={len(ps_rows)}")
        # PROC page auto-refreshes (dumps every ~2s); the accumulated
        # buffer holds several dumps of the same rows.  Parse ONE block
        # (text after the first header up to the next header) instead.
        blkall = plain()
        hdrs = [mm.start() for mm in
                re.finditer(rb"\[INFO\] PROC p=", blkall)]
        first_blk = blkall[hdrs[0]:(hdrs[1] if len(hdrs) > 1
                                    else len(blkall))] if hdrs else b""
        rows = re.findall(rb"^ *(\d+) (\S+) +(\w+) (\d+)K$",
                          first_blk, re.M)
        check("visible rows parsed", len(rows) == min(8, n),
              f"{len(rows)} rows")
        info_pids = {int(r[0]) for r in rows}
        ps_pids = {pid for pid, _ in ps_rows}
        check("page-0 pids subset of ps+spawned",
              info_pids <= ps_pids or len(info_pids & ps_pids) >=
              min(len(ps_pids), 4),
              f"info={sorted(info_pids)} ps={sorted(ps_pids)}")
        mems = [int(r[3]) for r in rows]
        check("mem_kb nonzero", all(v > 0 for v in mems), str(mems))
        names = {r[1].decode() for r in rows}
        check("self listed", "info" in names, str(sorted(names)))

    # N -> next page.  Multi-page case: page index must advance and
    # page-1 rows must be disjoint from page-0 rows (real flip, not a
    # re-dump of the same block).
    sendkey("n", 3.0)
    readserial()
    # search in the TAIL: re.search would return the FIRST (stale
    # page-0) header from the periodic dumps, not the post-N state.
    hdrs_all = list(re.finditer(
        rb"\[INFO\] PROC p=(\d+)/(\d+) n=(\d+)", plain()))
    m2 = hdrs_all[-1] if hdrs_all else None
    if m2 and m and int(m.group(3)) > 8:
        check("N advanced page", int(m2.group(1)) == 1,
              m2.group(0).decode())
        blkall = plain()
        hdrs = [mm.start() for mm in
                re.finditer(rb"\[INFO\] PROC p=1/", blkall)]
        p1_blk = blkall[hdrs[0]:(hdrs[1] if len(hdrs) > 1
                                 else len(blkall))] if hdrs else b""
        p1_rows = re.findall(rb"^ *(\d+) (\S+) +(\w+) (\d+)K$",
                             p1_blk, re.M)
        expect_p1 = n - 8 if m else 0
        check("page-1 row count", len(p1_rows) == expect_p1,
              f"{len(p1_rows)} rows, expect {expect_p1}")
        p1_pids = {int(r[0]) for r in p1_rows}
        check("page-1 disjoint from page-0", not (p1_pids & info_pids),
              f"p0={sorted(info_pids)} p1={sorted(p1_pids)}")
        total_pages = int(m2.group(2))
        check("page count", total_pages == (n + 7) // 8 - 1,
              f"maxpage={total_pages} n={n}")
    else:
        check("single page: N clamped", m2 is not None and
              int(m2.group(1)) == 0, m2.group(0).decode() if m2 else "")

    # P -> back (tail search, same rationale as the N check)
    sendkey("p", 3.0)
    readserial()
    hdrs_all = list(re.finditer(
        rb"\[INFO\] PROC p=(\d+)/(\d+) n=(\d+)", plain()))
    m3 = hdrs_all[-1] if hdrs_all else None
    check("P went back", m3 is not None and int(m3.group(1)) == 0,
          m3.group(0).decode() if m3 else "")

    # Tab -> back to sys page.  The auto-refresh dump races with the
    # page switch (a periodic dump due around the key event still
    # lands), so a fixed window can false-FAIL.  Deterministic check:
    # wait for the dump stream to go quiet for >=3s (>1 refresh
    # period), then Tab again and verify dumps RESUME (page cycles).
    readserial()
    sendkey("tab", 1.0)
    quiet_ok = False
    last = plain().count(b"[INFO] PROC p=")
    stable = 0
    for _ in range(10):
        time.sleep(1.0)
        readserial()
        c = plain().count(b"[INFO] PROC p=")
        if c == last:
            stable += 1
            if stable >= 3:
                quiet_ok = True
                break
        else:
            stable = 0
            last = c
    sendkey("tab", 2.0)
    readserial()
    resumed = plain().count(b"[INFO] PROC p=") > last
    check("Tab back to sys page", quiet_ok and resumed,
          f"quiet={quiet_ok} resumed={resumed} last={last}")

    check("no PANIC", b"PANIC" not in plain())
    check("no EXC", b"[EXC]" not in plain())
finally:
    qemu.kill()

print("\n=== RESULTS ===", flush=True)
ok = sum(1 for _, o in checks if o)
for name, o in checks:
    print(f"{'PASS' if o else 'FAIL'}  {name}", flush=True)
print(f"{ok}/{len(checks)} checks passed", flush=True)
os.makedirs("logs", exist_ok=True)
with open("logs/info_probe_serial.log", "wb") as f:
    f.write(buf)
sys.exit(0 if ok == len(checks) else 1)
