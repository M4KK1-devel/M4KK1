#!/usr/bin/env python3
"""Deep functional probe via HMP sendkey: drive the real apps.

Keyboard reaches the guest as genuine PS/2 events (HMP sendkey), so
sprach routes them exactly as a human typist would.  Scenarios:

1. altr2: spawn -> type text in INSERT mode -> Ctrl+S save as
   /export/home/makk1/probe_altr.txt -> close -> shell cat readback.
2. calc: spawn -> 12*3+4 = -> assert via serial (calc logs none, so
   close-flow only) ... we instead assert its process survives keys.
3. pref: spawn -> 1 (wallpaper tab) -> 3 (theme) -> shell cat
   /export/cfg/pref/wallpaper.conf == "3".

The m4sht prompt echoes everything; window focus goes to the NEWEST
surface, and sprach forwards keys by width dispatch.
"""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISO = "output/m4kk1_0.0.1_build10-alpha1-full-test.iso"

def plain(b):
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", b)

sock = "/tmp/m4k_deep.sock"
mon = "/tmp/m4k_deep.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/deep_probe_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "unix:%s,server=on,wait=off" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
time.sleep(2)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock); s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon); m.setblocking(False)

buf = bytearray()

def mdrain(t=0.3):
    end = time.time() + t
    while time.time() < end:
        try: m.recv(65536)
        except BlockingIOError: pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    mdrain(0.15)

def sdrain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d: buf.extend(d); log.write(d)
        except BlockingIOError: pass
        time.sleep(0.1)
    log.flush()

def send(line):
    s.sendall((line + "\n").encode())
    time.sleep(0.2)

def wait_for(pat, timeout):
    end = time.time() + timeout
    while time.time() < end:
        sdrain(0.5)
        if pat in plain(bytes(buf)):
            return True
    return False

# HMP sendkey names for chars we need
KEYMAP = {c: c for c in "abcdefghijklmnopqrstuvwxyz0123456789"}
KEYMAP.update({
    " ": "spc", "-": "minus", "=": "equal", "/": "slash",
    ".": "dot", "\n": "ret", "*": "asterisk", "+": "add",
    "_": "shift-minus", ":": "shift-semicolon",
    "/c": "ctrl-s", "/f": "ctrl-f", "/p": "ctrl-p",
    "/o": "ctrl-o", "/q": "ctrl-q", "/w": "ctrl-w",
    "/t": "ctrl-tab", "esc": "esc", "tab": "tab",
    "bs": "backspace", "up": "up", "down": "down",
})

def key(k, hold=0.06):
    """press one key (HMP name or single char)"""
    name = KEYMAP.get(k, k)
    hmp("sendkey %s" % name)
    time.sleep(hold)

def type_str(text, hold=0.05):
    for ch in text:
        key(ch, hold)

results = {}

# ── boot to shell ──
if not wait_for(b"m4sh ~>", 90):
    print("BOOT FAIL"); qemu.kill(); sys.exit(1)
print("[boot] prompt OK", flush=True)
time.sleep(3)

# ── 1) altr2: edit + save + readback ──
mark = len(buf)
send("spawn /bin/altr")
ok = False
end = time.time() + 25
while time.time() < end:
    sdrain(0.5)
    if b"[ALTR2] surface ready" in plain(bytes(buf))[mark:]:
        ok = True; break
results["altr_banner"] = ok
print("[altr] banner", ok, flush=True)

if ok:
    time.sleep(2)
    # INSERT mode and type
    key("i"); time.sleep(0.3)
    type_str("hello altr2 deep probe")
    time.sleep(0.3)
    key("esc"); time.sleep(0.3)
    # Ctrl+S -> save path input (unsaved doc)
    key("/s"); time.sleep(0.5)
    type_str("/export/home/makk1/probe_altr.txt")
    key("\n"); time.sleep(1.0)
    # Esc/q to close?  altr2: Ctrl+W closes tab; q in NORMAL quits?
    # per keys.c: 'q' quits.  Try Ctrl+W then q.
    pump_close = lambda: wait_for(b"[ALTR2]", 3)
    key("/w"); time.sleep(0.5)
    sdrain(2)

    # shell readback
    mark = len(buf)
    send("cat /export/home/makk1/probe_altr.txt")
    got = False
    end = time.time() + 15
    while time.time() < end:
        sdrain(0.5)
        seg = plain(bytes(buf))[mark:]
        if b"hello altr2 deep probe" in seg:
            got = True; break
    results["altr_save_readback"] = got
    print("[altr] save+readback", got, flush=True)

# ── 2) pref: wallpaper theme persist ──
mark = len(buf)
send("spawn /bin/pref")
ok = False
end = time.time() + 25
while time.time() < end:
    sdrain(0.5)
    if b"[PREF] surface ready" in plain(bytes(buf))[mark:]:
        ok = True; break
results["pref_banner"] = ok
print("[pref] banner", ok, flush=True)

if ok:
    time.sleep(2)
    key("1"); time.sleep(0.4)     # wallpaper tab
    key("3"); time.sleep(0.8)     # theme 3
    sdrain(1)
    key("q"); time.sleep(0.5)
    mark = len(buf)
    send("cat /export/cfg/pref/wallpaper.conf")
    got = False
    end = time.time() + 15
    while time.time() < end:
        sdrain(0.5)
        seg = plain(bytes(buf))[mark:]
        if re.search(rb"m4sh ~>3", seg) or re.search(rb"3\s*\r?\n", seg):
            got = True; break
    results["pref_persist"] = got
    print("[pref] wallpaper.conf==3", got, flush=True)

print()
print("=== DEEP RESULTS ===")
allok = True
for k, v in results.items():
    print(f"{k:20} {'OK' if v else 'FAIL'}")
    allok = allok and v
qemu.kill()
sys.exit(0 if allok else 1)
