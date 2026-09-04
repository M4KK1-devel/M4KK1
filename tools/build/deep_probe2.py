#!/usr/bin/env python3
"""Deep functional probe v2: stdio serial (proven reliable) + HMP
monitor unix socket for sendkey / screendump injection.

Scenarios:
1. altr2: spawn -> i + type -> Esc -> Ctrl+S -> path -> Enter ->
   q -> shell cat readback == typed text.
2. pref: spawn -> 1 (wallpaper tab) -> 3 (theme) -> q -> shell cat
   /export/cfg/pref/wallpaper.conf == "3".
"""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_dp2.mon"
if os.path.exists(mon):
    os.unlink(mon)

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "stdio",
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL)

# wait for monitor socket
for _ in range(30):
    if os.path.exists(mon):
        break
    time.sleep(0.3)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon)
m.setblocking(False)

buf = bytearray()
log = open("logs/deep2_serial.log", "wb")

def mdrain(t=0.25):
    end = time.time() + t
    while time.time() < end:
        try: m.recv(65536)
        except BlockingIOError: pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    mdrain(0.12)

def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([qemu.stdout], [], [], 0.15)
        if r:
            try:
                d = os.read(qemu.stdout.fileno(), 4096)
            except OSError:
                break
            if not d: break
            buf.extend(d); log.write(d)
    log.flush()

def clean():
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", bytes(buf))

def send(line):
    qemu.stdin.write((line + "\n").encode())
    qemu.stdin.flush()

def wait_for(pat, timeout):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pat in clean(): return True
    return False

def key(name):
    hmp("sendkey " + name)
    time.sleep(0.12)

def type_str(text):
    for ch in text:
        if ch.isupper():
            key("shift-" + ch.lower())
        elif ch == " ":
            key("spc")
        elif ch == "/":
            key("slash")
        elif ch == ".":
            key("dot")
        elif ch == "-":
            key("minus")
        elif ch == "_":
            key("shift-minus")
        else:
            key(ch)

results = {}

if not wait_for(b"m4sh ~>", 90):
    print("BOOT FAIL"); qemu.kill(); sys.exit(1)
print("[boot] prompt OK", flush=True)
# sprach enters its main loop only after the first client surfaces
# exist — keys sent before "Entering main loop" are lost entirely
wait_for(b"Entering main loop", 40)
time.sleep(5)

# ── 1) altr2 ─────────────────────────────────────────
send("spawn /bin/altr")
results["altr_banner"] = wait_for(b"[ALTR2] surface ready", 25)
print("[altr] banner", results["altr_banner"], flush=True)
if results["altr_banner"]:
    wait_for(b"[CPTEST] ALL PASS", 45)   # cptest exits at ~40s and
    time.sleep(3)                  # would shadow the top surface
    key("i"); time.sleep(0.4)          # INSERT
    type_str("hello altr2 deep probe")
    time.sleep(0.4)
    key("esc"); time.sleep(0.4)        # NORMAL (sprach now forwards
    # bare Esc to the focused ga app when no chrome is open)
    key("shift-semicolon"); time.sleep(0.5)   # ':' → palette
    type_str("save /export/cfg/probe_altr.txt")
    key("ret"); time.sleep(1.2)
    key("shift-semicolon"); time.sleep(0.4)   # ':' palette again
    type_str("q")                        # palette quit command
    key("ret"); time.sleep(0.8)
    mark = len(buf)
    send("cat /export/cfg/probe_altr.txt")
    ok = False
    end = time.time() + 15
    while time.time() < end:
        pump(0.5)
        if b"hello altr2 deep probe" in clean()[mark:]:
            ok = True; break
    results["altr_save_readback"] = ok
    print("[altr] save+readback", ok, flush=True)

# ── 2) pref ──────────────────────────────────────────
send("spawn /bin/pref")
results["pref_banner"] = wait_for(b"[PREF] surface ready", 25)
print("[pref] banner", results["pref_banner"], flush=True)
if results["pref_banner"]:
    time.sleep(2)
    key("3"); time.sleep(1.0)         # theme 3 (wallpaper tab default)
    key("q"); time.sleep(0.8)
    mark = len(buf)
    send("cat /export/cfg/pref/wallpaper.conf")
    ok = False
    end = time.time() + 15
    while time.time() < end:
        pump(0.5)
        seg = clean()[mark:]
        mt = re.search(rb"3\s*\r?\n", seg[seg.find(b"wallpaper.conf"):])
        if mt:
            ok = True; break
    results["pref_persist"] = ok
    print("[pref] wallpaper.conf==3", ok, flush=True)

print()
print("=== DEEP2 RESULTS ===")
allok = True
for k, v in results.items():
    print(f"{k:22} {'OK' if v else 'FAIL'}")
    allok = allok and v
qemu.kill()
sys.exit(0 if allok else 1)
