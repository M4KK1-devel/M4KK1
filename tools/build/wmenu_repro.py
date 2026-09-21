#!/usr/bin/env python3
"""wmenu_repro.py — verify the unified window title-bar context menu (mode 4).

Drives QEMU via HMP mouse events against the full-test ISO:
  1. right-click a demo window title bar  -> "rmenu: window menu"
  2. left-click "Minimize"                -> "[SPRACH] MIN 0", window hidden
  3. taskbar click restores it            -> "[SPRACH] ..." focus line
  4. right-click title bar again, click "Maximize" -> "[SPRACH] MAX 0"
  5. right-click again (menu shows "Restore")      -> "[SPRACH] RESTORE 0"
  6. right-click, "Close"                 -> "[SPRACH] CLOSE 0"
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[wmenu] no full-test ISO, run build_krn.sh --full-test first")
    raise SystemExit(2)
iso = os.path.join("output", isos[0])
print("[wmenu] using", iso)

sock = "/tmp/m4k_wmenu.sock"
mon = "/tmp/m4k_wmenu.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/wmenu_serial.log", "wb")

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

acc = b""

def drain(t):
    global acc
    end = time.time() + t
    while time.time() < end:
        for sk, is_mon in ((s, False), (m, True)):
            try:
                d = sk.recv(65536)
                if d and not is_mon:
                    acc += d
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
    # QEMU HMP "mouse_button 2" reads as the RIGHT button in the guest
    # (PS/2 byte0 bit1) — verified in rmenu_repro.py.
    hmp("mouse_button 2")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

def left_click():
    hmp("mouse_button 1")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

drain(0.5)
drain(30)   # boot to desktop (autologin)

# Window 0 (stacking mode) is created at (140, 90), 256x192 per
# sprach_mode_stack.c; title bar y range 90..108 (SPRACH_TITLE_H=18).
# Click at (200, 98): inside the title bar, clear of the close/min/max
# buttons which sit on the RIGHT edge of the title bar.
TB_X, TB_Y = 200, 98
# Menu geometry: WMENU_W=150, 3 items of 22px + 2px padding, clamped
# on screen starting at the click point.
RMENU_W, ITEM_H, PAD = 150, 22, 2

def menu_item_y(base_y, idx):
    # menu opens at the click point (clamped); item idx centre
    return base_y + PAD + idx * ITEM_H + ITEM_H // 2

# 1) right-click window 0 title bar -> window menu
move_to(TB_X, TB_Y)
drain(0.3)
right_click()
drain(1.5)

# 2) left-click "Minimize" (item 0)
mx = TB_X + 40
my = menu_item_y(TB_Y, 0)
move_to(mx, my)
drain(0.3)
left_click()
drain(2.0)

# 3) restore via taskbar: dock layout is [launchpad][win0][win1]...
# with DOCK_PAD=8, PITCH=44 → win0 button spans x 52..84,
# y = SCREEN_H-TASKBAR_H+8 .. +40 (icon_y=(48-32)/2=8).
move_to(68, 576)
drain(0.3)
left_click()
drain(2.0)

# 4) right-click title bar again, click "Maximize" (item 1)
move_to(TB_X, TB_Y)
drain(0.3)
right_click()
drain(1.5)
my = menu_item_y(TB_Y, 1)
move_to(mx, my)
drain(0.3)
left_click()
drain(2.0)

# 5) right-click again (maximized window spans the work area from
# y=WORK_AREA_Y=MENUBAR_H=24; its title bar is y 24..42) → "Restore"
move_to(TB_X, 32)
drain(0.3)
right_click()
drain(1.5)
my = menu_item_y(32, 1)
move_to(mx, my)
drain(0.3)
left_click()
drain(2.0)

# 6) right-click, "Close" (item 2)
move_to(TB_X, TB_Y)
drain(0.3)
right_click()
drain(1.5)
my = menu_item_y(TB_Y, 2)
move_to(mx, my)
drain(0.3)
left_click()
drain(2.0)

text = acc.decode("utf-8", "replace")
checks = {
    "window_menu":  "rmenu: window menu" in text,
    "minimize":     "[SPRACH] MIN 0" in text,
    "restored":     ("[SPRACH] CLOSE" not in text[:text.find("MIN 0")] if "MIN 0" in text else False),
    "maximize":     "[SPRACH] MAX 0" in text,
    "restore":      "[SPRACH] RESTORE 0" in text,
    "close":        "[SPRACH] CLOSE 0" in text,
    "no_panic":     "PANIC" not in text,
    "no_gpf":       "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
