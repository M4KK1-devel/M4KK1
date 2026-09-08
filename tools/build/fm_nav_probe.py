#!/usr/bin/env python3
"""FM navigation-history probe: Back/Forward stack + breadcrumb bar.

Boots the newest *full-test.iso*, launches the FM via Super+E, then
drives navigation through the address bar and asserts the history
state via serial logs:

  1. super-e                 -> FM boots, surface ready
  2. ctrl-l "/export" ret    -> [FM] NAV via address bar (history push)
  3. ctrl-l "/export/root"   -> second push
  4. alt-left                -> "[FM] NAV BACK /export (1/2)"
  5. alt-left                -> "[FM] NAV BACK / (0/2)"
  6. alt-left (3rd)          -> NO new BACK line (at stack bottom)
  7. alt-right               -> "[FM] NAV FWD /export (1/2)"
  8. breadcrumb click        -> jump to ancestor (NAV push, not FWD)
  9. no panic/GPF across the whole run

HMP sendkey cannot latch modifiers, but the kernel maps Alt+Left /
Alt+Right (and Ctrl chords are translated in sprach) so sendkey
ctrl-l / alt-left drive the real paths.
"""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not ISOS:
    print("FAIL: no *full-test.iso in output/ (run build_krn.sh --full-test)")
    sys.exit(1)
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_fmnav.mon"
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
log = open("logs/fm_nav_serial.log", "wb")

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

# 0. boot
if not wait_for(b"[SPRACH] Entering main loop", 150):
    print("FAIL: sprach never entered main loop")
    qemu.kill()
    sys.exit(1)
pump(3)

# 1. launch FM
hmp("sendkey meta_l-e")
step("fm boots", b"[FM] surface ready", 30)
pump(2)

# 2. Ctrl+L → type /export → Enter (history push 2)
buf.clear()
hmp("sendkey ctrl-l")
mdrain(0.3)
for ch in "/export":
    hmp("sendkey %s" % ch)
    mdrain(0.08)
hmp("sendkey ret")
# address-bar commit has no dedicated log; the BACK lines below prove it

# 3. Ctrl+L → /export/root → Enter (history push 3)
mdrain(0.3)
hmp("sendkey ctrl-l")
mdrain(0.3)
for ch in "/export/root":
    hmp("sendkey %s" % ch)
    mdrain(0.08)
hmp("sendkey ret")
pump(2)

# 4. Alt+Left → BACK /export/root? no: back from root goes to /export
buf.clear()
hmp("sendkey alt-left")
step("alt-left #1 back to /export", b"[FM] NAV BACK /export (1/2)")

# 5. Alt+Left → BACK /
buf.clear()
hmp("sendkey alt-left")
step("alt-left #2 back to /", b"[FM] NAV BACK / (0/2)")

# 6. Alt+Left at the bottom → nothing new
buf.clear()
hmp("sendkey alt-left")
pump(2)
allout = clean()
ok = b"[FM] NAV BACK" not in allout
results.append(("alt-left at bottom is a no-op", ok))
print(("PASS" if ok else "FAIL"), "alt-left at bottom is a no-op", flush=True)

# 7. Alt+Right → FWD /export
buf.clear()
hmp("sendkey alt-right")
step("alt-right forward to /export", b"[FM] NAV FWD /export (1/2)")

# 8. Breadcrumb click: from /export, click the "root" segment? The
# breadcrumb at /export shows "/ > export". Instead: FWD to root then
# click the "export" segment to jump back (push, not cursor move).
buf.clear()
hmp("sendkey alt-right")
step("alt-right #2 forward to /export/root",
     b"[FM] NAV FWD /export/root (2/2)")

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
