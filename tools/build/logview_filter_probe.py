#!/usr/bin/env python3
"""logview_filter_probe.py — verify logview '/' filter mode end-to-end.

Skeleton copied from clock_alarm_probe.py (proven):
serial = unix socket (m4sht shell), monitor = HMP unix socket.

Steps:
  1. spawn /bin/logview, wait "[LOGVIEW] surface ready" (+ loaded N).
  2. minimize/close the desktop terminal via HMP click on its close
     box so keystrokes reach the top-most GUI app (ga width dispatch).
  3. sendkey '/' , "panic", ret -> expect serial
     `[LOGVIEW] FILTER "panic" <m>/<n> lines` with 0 < m < n.
  4. sendkey 'f' -> expect `[LOGVIEW] JUMP #<k> line <store>`.
  5. sendkey esc -> expect `[LOGVIEW] FILTER off`.
  6. no kernel panic / GPF in serial.
"""
import os, socket, subprocess, time, re, sys

os.chdir("/mnt/f/M4KK1")
isos = [f for f in sorted(os.listdir("output"))
        if f.endswith("full-test.iso")]
if not isos:
    print("[lvfilter] no full-test ISO")
    sys.exit(1)
iso = os.path.join("output", isos[-1])
print("[lvfilter] using", iso)

sock = "/tmp/m4k_lvfilter.sock"
mon = "/tmp/m4k_lvfilter.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/logview_filter_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=on" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon)
m.setblocking(False)

acc = b""
def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d:
                    if not is_mon:
                        acc += d
                        log.write(d); log.flush()
            except BlockingIOError:
                pass
            except OSError:
                pass
        time.sleep(0.05)

def ssend(line):
    s.send((line + "\n").encode())

def hmp(cmd):
    m.send((cmd + "\n").encode())
    time.sleep(0.3)

def sendkey(k):
    hmp("sendkey " + k)

KEY = {" ": "spc", "\n": "ret", "/": "slash", "-": "minus",
       ".": "dot", ",": "comma", ";": "semicolon"}

def type_str(seq, hold=0.12):
    for ch in seq:
        if ch.isupper():
            k = "shift-" + ch.lower()   # HMP sendkey has no caps
        else:
            k = KEY.get(ch, ch)
        sendkey(k)
        time.sleep(hold)

def esc():
    sendkey("esc")

# ── boot: wait for shell prompt ──
t0 = time.time()
while b"m4sh ~>" not in re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", acc):
    drain(1)
    if time.time() - t0 > 120:
        print("[lvfilter] no shell prompt"); qemu.kill(); sys.exit(1)
print("[lvfilter] shell up")
drain(3)

# ── 0. seed /var/log/messages with a known multi-line file ──
#    nothing in the system creates /var/log/messages (logview's list
#    has always been empty on full-test), and m4sh '>>' append is
#    lossy (single partial read of old contents).  cp of the baked
#    man page m4sh.1 is reliable: 63 lines total, "M4KK1" on 4 of
#    them (counts verified host-side, baked into the ISO at build).
ssend("mkdir /var/log")
drain(2)
mark = len(acc)
ssend("cp /export/share/man/man1/m4sh.1 /var/log/messages")
drain(3)
out = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", acc[mark:])
if b"cannot" in out or b"not found" in out:
    print("[lvfilter] cp failed:", out.decode(errors="replace"))
    qemu.kill(); sys.exit(1)
print("[lvfilter] seeded /var/log/messages <- m4sh.1 (63 lines)")

# ── 1. spawn logview ──
mark = len(acc)
ssend("spawn /bin/logview")
ok = False
t0 = time.time()
while time.time() - t0 < 20:
    drain(1)
    if b"[LOGVIEW] surface ready" in acc[mark:]:
        ok = True; break
print("[lvfilter] spawn:", "OK" if ok else "FAIL")
if not ok:
    qemu.kill(); sys.exit(1)
drain(2)

# loaded trace gives line_count for sanity
lm = re.search(rb"\[LOGVIEW\] loaded (\d+) lines", acc)
if lm:
    print("[lvfilter] loaded lines:", lm.group(1).decode())

# ── 2. focus: click the terminal window's close box (topmost
#    non-app window) so ga_key sees logview as top-most.  Terminal
#    geometry per drag_repro: 680x456 at (60,40); close box sits in
#    its title bar left area (ga_chrome).  Click (72,49).
#    If ga dispatch already works (comment in apps_repro says ga_key
#    runs BEFORE terminal forward), this click is harmless.
#    Safer: first test without clicking; if no FILTER trace arrives,
#    click and retry once.
def try_filter():
    mark = len(acc)
    sendkey("slash"); time.sleep(0.3)
    type_str("M4KK1")
    sendkey("ret"); time.sleep(0.3)
    t0 = time.time()
    while time.time() - t0 < 8:
        drain(1)
        mm = re.search(
            rb'\[LOGVIEW\] FILTER "M4KK1" (\d+)/(\d+) lines',
            acc[mark:])
        if mm:
            return mm
    return None

mm = try_filter()
if not mm:
    # terminal may have focus: minimize via close-box click
    print("[lvfilter] no key delivery — clicking terminal close box")
    hmp("mouse_move 72 49")
    hmp("mouse_button 1")
    hmp("mouse_button 0")
    drain(2)
    mm = try_filter()

results = {}
if mm:
    mi, tot = int(mm.group(1)), int(mm.group(2))
    # "M4KK1" matches exactly 4 lines in m4sh.1 (host-verified);
    # total 63 host / 64 in-app (trailing-newline convention).
    results["filter"] = (mi == 4 and 0 < mi < tot)
    print(f"[lvfilter] filter 'M4KK1': {mi}/{tot} ->",
          "OK" if results["filter"] else "FAIL (expect 4 matches)")
else:
    results["filter"] = False
    print("[lvfilter] filter trace: FAIL")

# ── F jump ──
mark = len(acc)
sendkey("f"); time.sleep(0.3); sendkey("f")
t0 = time.time()
jm = None
while time.time() - t0 < 8:
    drain(1)
    jm = re.search(rb"\[LOGVIEW\] JUMP #(\d+) line (\d+)", acc[mark:])
    if jm:
        break
results["jump"] = bool(jm)
print("[lvfilter] F jump:", "OK" if jm else "FAIL")
if jm:
    print(f"[lvfilter]   jump -> #{jm.group(1).decode()} "
          f"store line {jm.group(2).decode()}")

# ── Esc clear ──
mark = len(acc)
esc(); time.sleep(0.5)
ok = False
t0 = time.time()
while time.time() - t0 < 8:
    drain(1)
    if b"[LOGVIEW] FILTER off" in acc[mark:]:
        ok = True; break
results["esc_clear"] = ok
print("[lvfilter] Esc clear:", "OK" if ok else "FAIL")

# ── no kernel panic ──
drain(2)
bad = (b"kernel panic" in acc.lower()
       or b"general protection" in acc.lower()
       or b"GPF" in acc)
results["no_kpanic"] = not bad
print("[lvfilter] no kernel panic/GPF:", "OK" if not bad else "FAIL")

ok = all(results.values())
print("RESULT:", "PASS" if ok else "FAIL")
qemu.kill()
sys.exit(0 if ok else 1)
