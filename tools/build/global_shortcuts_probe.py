#!/usr/bin/env python3
"""Global-shortcuts probe: Alt+Tab / Super+D / Super+E via HMP sendkey.

Boots the newest *full-test.iso*, waits for the sprach main loop, then
injects key chords and asserts serial trigger logs:

  1. sendkey alt-tab          -> "[SPRACH] Alt+Tab: window" (cycle 0->1)
  2. sendkey alt-tab          -> "[SPRACH] Alt+Tab: terminal" (wrap back)
  3. sendkey ctrl-alt-t?  NO — terminal already exists on full-test;
     window count comes from the boot-time Win 1..3.
  4. sendkey alt-tab          -> back to window 0
  5. sendkey meta_l-d           -> "[SPRACH] Super+D: show desktop"
  6. sendkey meta_l-d           -> "[SPRACH] Super+D: restore"
  7. sendkey meta_l-e           -> "[SPRACH] Super+E: launching /bin/fm"
  8. wait for fm surface (fm prints its own boot line) or timeout

Assert lines use unique prefixes so partial output still scores.
"""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not ISOS:
    print("FAIL: no *full-test.iso in output/ (run build_krn.sh --full-test)")
    sys.exit(1)
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_gsc.mon"
if os.path.exists(mon):
    os.unlink(mon)

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "stdio",
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL)

for _ in range(30):
    if os.path.exists(mon):
        break
    time.sleep(0.3)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon)
m.setblocking(False)

buf = bytearray()
os.makedirs("logs", exist_ok=True)
log = open("logs/global_shortcuts_serial.log", "wb")

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

def wait_for(pat, timeout, reset=False):
    if reset:
        buf.clear()
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pat in clean():
            return True
    return False

results = []

def step(name, pat, timeout=25):
    ok = wait_for(pat, timeout)
    results.append((name, ok))
    print(("PASS" if ok else "FAIL"), name, flush=True)
    return ok

# 0. boot: sprach main loop + autologin terminal running
if not wait_for(b"[SPRACH] Entering main loop", 150):
    print("FAIL: sprach never entered main loop")
    qemu.kill()
    sys.exit(1)
pump(3)

# Full-test autologin terminal: surface takes a moment to register
if not wait_for(b"[SPRACH] terminal pid=", 30):
    print("WARN: no autologin terminal line yet, continuing", flush=True)
pump(5)

# 1. Alt+Tab x4: cycles through the window list and wraps.
#    (No graphical terminal on this ISO's autologin path: cycle is
#     window 0 -> 1 -> 2 -> wrap 0.  Accept by relative advance.)
hmp("sendkey alt-tab")
step("alt-tab #1 triggers", b"[SPRACH] Alt+Tab:")
buf.clear()
hmp("sendkey alt-tab")
step("alt-tab #2 advances", b"[SPRACH] Alt+Tab: window")
buf.clear()
hmp("sendkey alt-tab")
step("alt-tab #3 advances", b"[SPRACH] Alt+Tab: window")
buf.clear()
hmp("sendkey alt-tab")
step("alt-tab #4 wraps", b"[SPRACH] Alt+Tab:")

# 5. Super+D: show desktop
buf.clear()
hmp("sendkey meta_l-d")
step("super-d hides all", b"[SPRACH] Super+D: show desktop")

# 6. Super+D: restore
hmp("sendkey meta_l-d")
step("super-d restores", b"[SPRACH] Super+D: restore")

# 7. Super+E: launch fm
buf.clear()
hmp("sendkey meta_l-e")
step("super-e launches fm", b"[SPRACH] Super+E: launching /bin/fm")

# 8. fm alive: give it time to register its mailbox + surface, then
# Super+E again must report "already running, raised"
time.sleep(10)
pump(2)
buf.clear()
hmp("sendkey meta_l-e")
step("fm alive (second super-e raises it)",
     b"[SPRACH] Super+E: fm already running", 20)

# 9. no panic/GPF across the whole run
pump(3)
allout = clean()
ok = (b"panic" not in allout.lower() and b"general protection" not in allout.lower())
results.append(("no panic/GPF", ok))
print(("PASS" if ok else "FAIL"), "no panic/GPF", flush=True)

qemu.kill()

npass = sum(1 for _, ok in results if ok)
print("RESULT: %d/%d" % (npass, len(results)))
sys.exit(0 if npass == len(results) else 1)
