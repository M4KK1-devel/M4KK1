#!/usr/bin/env python3
"""fm navigation-history probe: Back/Forward stack + breadcrumb bar state.

Boots the newest *full-test.iso*, raises fm (Super+E), then drives the
address bar (Ctrl+L) and the Alt+Left / Alt+Right chord keys through a
full history state machine, asserting serial traces:

  1.  super-e raises fm            -> "[FM] surface ready"
 2.  ctrl-l "bin" ret             -> "[FM] NAV PUSH /bin (1/1)"
 3.  ctrl-l "etc" ret             -> "[FM] NAV PUSH /etc (2/2)"
 4.  alt-left                     -> "[FM] NAV BACK /bin (1/2)"
 5.  alt-left                     -> "[FM] NAV BACK / (0/2)"
 6.  alt-left at oldest           -> NO further NAV line (no-op)
 7.  alt-right                    -> "[FM] NAV FWD /bin (1/2)"
 8.  alt-right                    -> "[FM] NAV FWD /etc (2/2)"
 9.  alt-right at newest          -> NO further NAV line (no-op)
 10.  alt-left then ctrl-l "home"  -> "[FM] NAV PUSH /home (2/2)"
      (forward tail /etc truncated)
 11.  alt-right after truncation   -> NO NAV FWD (tail is gone)
 12.  alt-left                     -> "[FM] NAV BACK /bin (1/2)"
 13.  no panic/GPF across the run

fm boots into the autologin user's home dir (/export/root for root),
so step 2's Ctrl+L buffer starts as "/export/root": we backspace the
"export/root" tail before typing "bin".
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
        try:
            m.recv(65536)
        except BlockingIOError:
            pass
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
            if not d:
                break
            buf.extend(d)
            log.write(d)
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

def step(name, pat, timeout=20):
    ok = wait_for(pat, timeout)
    results.append((name, ok))
    print(("PASS" if ok else "FAIL"), name, flush=True)
    return ok

def step_absent(name, quiet_sec=4):
    """Assert NO '[FM] NAV' appears within quiet_sec (boundary no-op)."""
    buf.clear()
    end = time.time() + quiet_sec
    while time.time() < end:
        pump(0.5)
    ok = b"[FM] NAV" not in clean()
    results.append((name, ok))
    print(("PASS" if ok else "FAIL"), name, flush=True)
    return ok

def goto(path, from_len):
    """Ctrl+L, backspace out the current tail, type path, Enter."""
    # clear current entry: current path minus leading '/' -> that many chars
    # (caller passes how many backspaces are needed)
    hmp("sendkey ctrl-l")
    time.sleep(0.3)
    for _ in range(from_len):
        hmp("sendkey backspace")
    for ch in path:
        hmp("sendkey " + ch)
    hmp("sendkey ret")

# 0. boot
if not wait_for(b"[SPRACH] Entering main loop", 150):
    print("FAIL: sprach never entered main loop")
    log.close()
    qemu.kill()
    sys.exit(1)
pump(3)

# 1. raise fm (autostart or Super+E — either way surface must come up)
hmp("sendkey meta_l-e")
step("fm surface ready", b"[FM] surface ready", 60)
pump(2)

# 2. address-bar nav: /export/root -> /bin  (buffer starts as the
#    current path "/export/root": backspace the "export/root" tail,
#    keep the leading "/", then type "bin")
buf.clear()
hmp("sendkey ctrl-l")
time.sleep(0.3)
for _ in range(11):
    hmp("sendkey backspace")
for k in ("b", "i", "n"):
    hmp("sendkey " + k)
hmp("sendkey ret")
step("push /bin", b"[FM] NAV PUSH /bin (1/1)")

# 3. /bin -> /etc (backspace 3, type "etc")
buf.clear()
goto("etc", 3)
step("push /etc", b"[FM] NAV PUSH /etc (2/2)")

# 4-5. back twice
buf.clear()
hmp("sendkey alt-left")
step("back -> /bin", b"[FM] NAV BACK /bin (1/2)")
buf.clear()
hmp("sendkey alt-left")
step("back -> home", b"[FM] NAV BACK /export/root (0/2)")

# 6. back at oldest: must be a no-op
hmp("sendkey alt-left")
step_absent("back at oldest is no-op")

# 7-8. forward twice
buf.clear()
hmp("sendkey alt-right")
step("fwd -> /bin", b"[FM] NAV FWD /bin (1/2)")
buf.clear()
hmp("sendkey alt-right")
step("fwd -> /etc", b"[FM] NAV FWD /etc (2/2)")

# 9. forward at newest: no-op
hmp("sendkey alt-right")
step_absent("fwd at newest is no-op")

# 10. back once (to /bin), then push /home -> forward tail /etc truncated
buf.clear()
hmp("sendkey alt-left")
step("back -> /bin (pre-truncate)", b"[FM] NAV BACK /bin (1/2)")
buf.clear()
goto("home", 3)
step("push /home truncates tail", b"[FM] NAV PUSH /home (2/2)")

# 11. forward after truncation: nothing left to redo
hmp("sendkey alt-right")
step_absent("fwd after truncate is no-op")

# 12. back lands on /bin (the entry right before /home)
buf.clear()
hmp("sendkey alt-left")
step("back -> /bin (post-truncate)", b"[FM] NAV BACK /bin (1/2)")

# 13. no panic/GPF
pump(3)
allout = clean()
ok = (b"panic" not in allout.lower()
      and b"general protection" not in allout.lower())
results.append(("no panic/GPF", ok))
print(("PASS" if ok else "FAIL"), "no panic/GPF", flush=True)

qemu.kill()
log.close()

npass = sum(1 for _, ok in results if ok)
print("RESULT: %d/%d" % (npass, len(results)))
sys.exit(0 if npass == len(results) else 1)
