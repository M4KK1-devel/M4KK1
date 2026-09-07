#!/usr/bin/env python3
"""Window-focus-policy probe: raise/activate edge cases via HMP.

Boots the newest *full-test.iso*, waits for the sprach main loop, then
drives focus transitions and asserts the new "[SPRACH] FOCUS ..." and
fallback serial logs:

  1. boot                        -> FOCUS NEW CLIENT (autologin terminal
                                    registers its surface: new window
                                    takes focus on startup)
  2. click Win-3 title bar       -> FOCUS CLICK window 2
                                    (click on non-client area raises)
  3. click Win-3 close box       -> CLOSE 2 + FOCUS FALLBACK window
                                    (focus falls back after closing the
                                    foreground window)
  4. click Win-2 close box       -> CLOSE 1 + FOCUS FALLBACK window 0
  5. click Win-1 close box       -> CLOSE 0 + FOCUS FALLBACK terminal
                                    (last demo window gone: terminal)
  6. super-e                     -> FOCUS NEW CLIENT 560x400
                                    (new fm window takes focus at launch)
  7. click fm title bar          -> FOCUS CLICK app 560x400
                                    (non-client click on foreign client)
  8. no panic/GPF across the run

Mouse geometry (sprach.c, stacking mode init):
  Win 1 (140,90) Win 2 (180,130) Win 3 (220,170), each 256x192,
  title bar 18px, close box: local (8..18, 4..14).
"""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not ISOS:
    print("FAIL: no *full-test.iso in output/ (run build_krn.sh --full-test)")
    sys.exit(1)
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))
print("ISO:", ISO, flush=True)

mon = "/tmp/m4k_foc.mon"
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
log = open("logs/focus_policy_serial.log", "wb")

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

# absolute cursor position tracker (HMP mouse_move is RELATIVE)
gx, gy = 400, 300

def move_to(tx, ty, step=8):
    global gx, gy
    n = 0
    while (gx != tx or gy != ty) and n < 400:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx; gy += dy
        n += 1

def left_click():
    hmp("mouse_button 1")
    mdrain(0.3)
    hmp("mouse_button 0")
    mdrain(0.3)

def title_click(win_x, win_y):
    """click the title bar (not the close box): local x ~128"""
    move_to(win_x + 128, win_y + 9)
    mdrain(0.2)
    left_click()

def close_click(win_x, win_y):
    """click the close box: local (8..18, 4..14) -> centre (13, 9)"""
    move_to(win_x + 13, win_y + 9)
    mdrain(0.2)
    left_click()

# 0. boot
if not wait_for(b"[SPRACH] Entering main loop", 150):
    print("FAIL: sprach never entered main loop")
    qemu.kill()
    sys.exit(1)
pump(3)

# 1. click the dock terminal launcher (far right) -> spawns
#    /bin/terminal -> it registers its surface and takes focus
#    (poll_terminal: active=-1 + raise, i.e. the classic new-window
#    focus policy for the terminal; NEW CLIENT logging covers the
#    fm/guiapp/altr2 clients in step 6).
#    (HMP sendkey cannot latch modifiers, so no ctrl-alt-t.)
buf.clear()
move_to(SCREEN_W := 800 - 32 - 8 + 16, 600 - 48 + (48 - 32) // 2 + 16)
mdrain(0.3)
left_click()
step("new window launch takes focus (terminal)",
     b"[SPRACH] terminal window registered", 40)
pump(2)

# stacking-mode window geometry (sprach_mode_stack.c); boot active=2
W1X, W1Y = 140, 90
W2X, W2Y = 180, 130
W3X, W3Y = 220, 170

# 2. click Win-1 title bar (foreground is the terminal: must switch)
buf.clear()
title_click(W1X, W1Y)
pump(2)
step("click non-client area raises window",
     b"[SPRACH] FOCUS CLICK window 0")

# 2b. Win-3 title: focus 0 -> 2
buf.clear()
title_click(W3X, W3Y)
pump(2)
step("click non-client area switches window",
     b"[SPRACH] FOCUS CLICK window 2")

# 3. close the focused Win-3 -> fallback to Win-2
buf.clear()
close_click(W3X, W3Y)
pump(2)
step("close fg window -> CLOSE 2", b"[SPRACH] CLOSE 2")
step("close fg window -> FOCUS FALLBACK window",
     b"[SPRACH] FOCUS FALLBACK window")

# 4. close Win-2 (it got the fallback focus) -> fallback window 0
buf.clear()
close_click(W2X, W2Y)
pump(2)
step("close cascade -> FOCUS FALLBACK window 0",
     b"[SPRACH] FOCUS FALLBACK window 0")

# 5. close Win-1 -> the terminal takes the focus back
buf.clear()
close_click(W1X, W1Y)
pump(2)
step("last window closed -> FOCUS FALLBACK terminal",
     b"[SPRACH] FOCUS FALLBACK terminal")

# 6. Super+E: fm launches -> NEW CLIENT focus at startup
buf.clear()
hmp("sendkey meta_l-e")
step("new app launch takes focus (fm)",
     b"[SPRACH] FOCUS NEW CLIENT 560x400", 30)

# 7. click the fm title bar (non-client area of a foreign client).
#    fm registers its surface at (100,60) (fm.c CREATE args).
buf.clear()
move_to(100 + 280, 60 + 9)
mdrain(0.3)
left_click()
pump(2)
step("foreign-client title click focuses app",
     b"[SPRACH] FOCUS CLICK app 560x400")

# 8. no panic/GPF across the whole run
pump(3)
allout = clean()
ok = (b"panic" not in allout.lower() and
      b"general protection" not in allout.lower())
results.append(("no panic/GPF", ok))
print(("PASS" if ok else "FAIL"), "no panic/GPF", flush=True)

qemu.kill()

npass = sum(1 for _, ok in results if ok)
print("RESULT: %d/%d" % (npass, len(results)))
sys.exit(0 if npass == len(results) else 1)
