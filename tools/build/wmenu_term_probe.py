#!/usr/bin/env python3
"""wmenu_term_probe.py — mode-4 menu on the TERMINAL window + live hover
repaint + foreign-window right-click swallow.

Drives QEMU via HMP mouse events against the full-test ISO:
  1. wallpaper right-click -> "New Terminal" -> terminal registers
     (parse its announced @ x,y for exact title-bar geometry)
  2. right-click the terminal title bar   -> "rmenu: term window menu"
  3. move onto item 0 / item 1 / off menu -> "wmenu hover 0"/"1"/"none"
     (live hover repaint telemetry added this round)
  4. click Minimize -> "TERMINAL MIN", dock icon restores it
  5. Maximize / Restore / Close via the same menu
  6. double-click the Files desktop icon  -> /bin/fm surface ready
  7. right-click INSIDE the fm window     -> "rmenu: foreign window
     swallowed" and NO wallpaper menu opens behind it
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, re, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[wmenu-term] no full-test ISO, run build_krn.sh --full-test first")
    raise SystemExit(2)
iso = os.path.join("output", isos[0])
print("[wmenu-term] using", iso)

sock = "/tmp/m4k_wmt.sock"
mon = "/tmp/m4k_wmt.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/wmenu_term_serial.log", "wb")

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
    # HMP "mouse_button 2" reads as RIGHT in the guest (PS/2 bit1).
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

drain(0.5)
ok_boot = wait_token("painting initial scene", 60)
drain(3)
print("[wmenu-term] boot:", ok_boot)

RMENU_W, ITEM_H, PAD = 150, 22, 2

def menu_item_y(base_y, idx):
    return base_y + PAD + idx * ITEM_H + ITEM_H // 2

# 1) wallpaper right-click -> New Terminal
BARE_X, BARE_Y = 500, 200
move_to(BARE_X, BARE_Y)
drain(0.3)
right_click()
drain(1.0)
move_to(BARE_X + 40, menu_item_y(BARE_Y, 0))
drain(0.3)
left_click()
got_term = wait_token("terminal window registered", 40)
drain(2)
mo = re.search(r"terminal window registered \(slot=\d+\) @ (\d+),(\d+)", acc)
if mo:
    tx, ty = int(mo.group(1)), int(mo.group(2))
    print("[wmenu-term] terminal @", tx, ty)
else:
    tx, ty = 100, 60   # fm-side default from CREATE_SURFACE args
TBX, TBY = tx + 100, ty + 9   # title bar centre-left, clear of buttons

# 2) right-click the terminal title bar
move_to(TBX, TBY)
drain(0.3)
right_click()
drain(1.5)

# 3) hover telemetry: item 0 -> item 1 -> off
mx0 = TBX + 40
move_to(mx0, menu_item_y(TBY, 0))
drain(0.6)
move_to(mx0, menu_item_y(TBY, 1))
drain(0.6)
move_to(TBX - 80, TBY)
drain(0.6)

# 4) re-enter item 0 and click Minimize
move_to(mx0, menu_item_y(TBY, 0))
drain(0.3)
left_click()
drain(1.5)

# dock terminal icon (3 demo wins -> entry 4): x 184..216, y 560..592
move_to(200, 576)
drain(0.3)
left_click()
drain(2.0)

# 5) Maximize via menu
move_to(TBX, TBY)
drain(0.3)
right_click()
drain(1.5)
move_to(mx0, menu_item_y(TBY, 1))
drain(0.3)
left_click()
drain(2.0)

# maximized term title bar is y 24..42; Restore via menu
move_to(400, 32)
drain(0.3)
right_click()
drain(1.5)
move_to(440, menu_item_y(32, 1))
drain(0.3)
left_click()
drain(2.0)

# Close via menu (window back at original geometry)
move_to(TBX, TBY)
drain(0.3)
right_click()
drain(1.5)
move_to(mx0, menu_item_y(TBY, 2))
drain(0.3)
left_click()
drain(2.0)

# 6) double-click the Files desktop icon -> /bin/fm
move_to(54, 78)
drain(0.5)
for _ in range(2):
    left_click()
drain(1.0)
got_fm = wait_token("[FM] surface ready", 40)
drain(2)

# 7) right-click inside the fm window, clear of every demo window
#    (fm spans 100..660 x 60..460; demo wins end at x 476)
move_to(600, 100)
drain(0.5)
right_click()
drain(1.5)
# the click must NOT have opened a wallpaper menu behind fm
tail = acc[acc.rfind("foreign window swallowed"):] if "foreign window swallowed" in acc else ""
hidden_desktop_menu = ("desktop menu" not in tail)

text = acc
i_sw = text.find("foreign window swallowed")
no_menu_leak = i_sw < 0 or "desktop menu" not in text[i_sw:]

checks = {
    "boot":         ok_boot,
    "term_spawned": got_term,
    "term_menu":    "rmenu: term window menu" in text,
    "hover_0":      "wmenu hover 0" in text,
    "hover_1":      "wmenu hover 1" in text,
    "hover_none":   "wmenu hover none" in text,
    "term_min":     "[SPRACH] TERMINAL MIN" in text,
    "dock_restore": "Dock icon clicked: terminal" in text,
    "term_max":     "[SPRACH] TERMINAL MAX" in text,
    "term_restore": "[SPRACH] TERMINAL RESTORE" in text,
    "term_close":   "[SPRACH] TERMINAL CLOSE" in text,
    "fm_spawned":   got_fm,
    "foreign_swallow": "foreign window swallowed" in text,
    "no_menu_leak": no_menu_leak and hidden_desktop_menu,
    "no_panic":     "PANIC" not in text,
    "no_gpf":       "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
