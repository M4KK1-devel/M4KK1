#!/usr/bin/env python3
"""wmenu_drag_probe.py — context-menu vs geometry-interaction mutex.

Opening a context menu must SUSPEND any in-flight drag/resize so the
geometry the menu was opened against cannot change underneath it.
Drives the full-test ISO via HMP mouse (L+R chord = mouse_button 3):

  1. win0 title-bar drag (left held) + chord right-click on the same
     title bar -> "rmenu: window menu" + "DRAG SUSPEND (menu open)",
     then Esc closes (serial-only: win geometry has no cheap pixel
     locator)
  2. wallpaper menu -> New Terminal (parse announced @ x,y)
  3. terminal title-bar drag +40/+24 (left still held) -> chord
     right-click on the title bar -> "rmenu: term window menu" +
     "TERM DRAG SUSPEND (menu open)"
  4. screendump A  (terminal already frozen at the drag position;
     the mode-4 menu floats at the click point, RIGHT of the
     detection column so it cannot pollute the walk-up)
  5. big cursor move while the menu stays open -> screendump B,
     assert B == A within 2 px (the actual regression this probe
     guards: the drag tracking block used to keep moving the window
     under the open menu)
Assertions come from sprach.c ser_puts lines + PPM pixel parsing
(terminal body tone (16,16,24) walk-up, same trick as drag_repro).
"""
import os, re, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[wmenu-drag] no full-test ISO, run build_krn.sh --full-test first")
    raise SystemExit(2)
iso = os.path.join("output", isos[0])
print("[wmenu-drag] using", iso)

sock = "/tmp/m4k_wmd.sock"
mon = "/tmp/m4k_wmd.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/wmenu_drag_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-serial", "unix:%s,server=on,wait=off" % sock,
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

acc = ""

def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d and not is_mon:
                    acc += d.decode("utf-8", "replace")
                    log.write(d)
                    log.flush()
            except BlockingIOError:
                pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.15)

def key(k):
    hmp("sendkey " + k)
    drain(0.4)

gx, gy = 400, 300

def move_to(tx, ty, step=8):
    global gx, gy
    while gx != tx or gy != ty:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx
        gy += dy

def right_click():
    hmp("mouse_button 2")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

def left_click():
    hmp("mouse_button 1")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

def wait_token(tok, t):
    end = time.time() + t
    while time.time() < end:
        drain(0.3)
        if tok in acc:
            return True
    return False

def dump(path):
    if os.path.exists(path):
        os.unlink(path)
    hmp("screendump " + path)
    drain(1.2)
    return open(path, "rb").read()

def term_topleft(data):
    """Terminal body tone (16,16,24) is unique on the desktop (same
    trick as drag_repro).  Find the leftmost >=60px run on row 300,
    then walk UP along column left+50 to the body top edge.  The
    mode-4 menu floats at the chord click point, which this probe
    keeps to the RIGHT of that column, so it never pollutes the
    walk-up."""
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    px = parts[3]
    target = (16, 16, 24)
    row = 300 * w
    left = -1
    run = 0
    for x in range(w):
        i = (row + x) * 3
        if (px[i], px[i+1], px[i+2]) == target:
            run += 1
            if run >= 60:
                left = x - run + 1
                break
        else:
            run = 0
    if left < 0:
        return None
    col = left + 50
    y = 300
    while y > 24:
        i = ((y - 1) * w + col) * 3
        if (px[i], px[i+1], px[i+2]) == target:
            y -= 1
        else:
            break
    return (left, y)

drain(0.5)
ok_boot = wait_token("painting initial scene", 60)
drain(3)
print("[wmenu-drag] boot:", ok_boot)

# ── 1) win0 title-bar drag + chord right-click ──
# stacking-mode geometry: win0 @ (140,90) 256x192, title 18px tall
move_to(240, 99)
drain(0.3)
hmp("mouse_button 1")       # press & HOLD -> DRAG START
drain(0.5)
move_to(270, 117, step=4)   # drag +30/+18
drain(0.8)
hmp("mouse_button 3")       # add RIGHT while left held (L+R chord)
drain(1.5)
move_to(310, 137, step=4)   # more moves while menu open
drain(1.0)
hmp("mouse_button 0")       # release everything
drain(0.5)
key("esc")                  # close the mode-4 menu
drain(0.8)

# ── 2) wallpaper menu -> New Terminal ──
RMENU_W, ITEM_H, PAD = 150, 22, 2
BARE_X, BARE_Y = 500, 200
move_to(BARE_X, BARE_Y)
drain(0.3)
right_click()
drain(1.0)
move_to(BARE_X + 40, BARE_Y + PAD + 0 * ITEM_H + ITEM_H // 2)
drain(0.3)
left_click()
got_term = wait_token("terminal window registered", 40)
drain(2)
mo = re.search(r"terminal window registered \(slot=\d+\) @ (\d+),(\d+)", acc)
if mo:
    tx, ty = int(mo.group(1)), int(mo.group(2))
    print("[wmenu-drag] terminal @", tx, ty)
else:
    tx, ty = 60, 40
TBX, TBY = tx + 100, ty + 9

# ── 3) terminal drag +40/+24 (left held), chord right-click ──
# after the drag the terminal sits at ~(tx+40, ty+24); the chord
# click at (TBX+40, TBY+24) is on its title bar (bar top = ty+24),
# and the menu that opens there stays RIGHT of the detection
# column (left+50 with left = tx+40 -> col = tx+90 << menu x).
move_to(TBX, TBY)
drain(0.3)
hmp("mouse_button 1")
drain(0.5)
move_to(TBX + 40, TBY + 24, step=4)
drain(1.0)
hmp("mouse_button 3")       # chord: menu opens HERE, drag suspends
drain(1.5)

# ── 4) screendump A (frozen at drag position) ──
posA = term_topleft(dump("/tmp/wmd_a.ppm"))
print("[wmenu-drag] A (menu open, frozen):", posA)

# ── 5) big cursor move while menu stays open, screendump B ──
move_to(650, 260, step=10)
drain(1.0)
posB = term_topleft(dump("/tmp/wmd_b.ppm"))
print("[wmenu-drag] B (cursor moved away):", posB)

hmp("mouse_button 0")
drain(0.5)
key("esc")
drain(0.8)

text = acc
drag_worked = frozen = False
if posA and posB:
    # drag moved the terminal by ~(+40,+24) before the menu opened
    drag_worked = (abs(posA[0] - tx) >= 30 and abs(posA[1] - ty) >= 12)
    # geometry frozen while the menu was open
    frozen = (abs(posB[0] - posA[0]) <= 2 and abs(posB[1] - posA[1]) <= 2)

checks = {
    "boot":          ok_boot,
    "win_drag_start": "[SPRACH] DRAG START" in text,
    "win_menu":      "rmenu: window menu" in text,
    "win_suspend":   "DRAG SUSPEND (menu open)" in text,
    "term_spawned":  got_term,
    "term_drag_start": "TERM DRAG START" in text,
    "term_menu":     "rmenu: term window menu" in text,
    "term_suspend":  "TERM DRAG SUSPEND (menu open)" in text,
    "drag_worked":   drag_worked,
    "frozen":        frozen,
    "kbd_close":     text.count("rmenu kbd close") >= 2,
    "no_panic":      "PANIC" not in text,
    "no_gpf":        "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
raise SystemExit(0 if ok else 1)
