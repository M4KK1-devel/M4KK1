#!/usr/bin/env python3
"""appmenu_probe.py — verify the menubar app-menu actions:
About / Preferences / Force Quit.

Drives QEMU (full-test ISO) via HMP mouse:
  1. click the "M4KK1" brand in the menubar   -> "app menu open"
  2. click "About This PC" (item 0)           -> "about panel open"
     click outside                            -> "app panel closed"
  3. reopen, click "Preferences" (item 1)     -> "preferences panel open"
     click a volume + button                  -> "prefs: volume -> 80"
     click outside                            -> "app panel closed"
  4. reopen, click "Force Quit" (item 2)      -> "FQ-WIN:" lines +
                                                 "force quit: closed N"
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted(f for f in os.listdir("output")
             if f.endswith("full-test.iso") or f.endswith("-full.iso"))
if not isos:
    print("[appmenu] no full-test ISO; run build_krn.sh --full-test first")
    raise SystemExit(1)
iso = os.path.join("output", isos[0])
print("[appmenu] using", iso)

sock = "/tmp/m4k_appmenu.sock"
mon = "/tmp/m4k_appmenu.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/appmenu_serial.log", "wb")

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

def left_click():
    hmp("mouse_button 1")
    drain(0.3)
    hmp("mouse_button 0")
    drain(0.3)

drain(0.5)
drain(30)   # boot to desktop (autologin)

# App-menu geometry (sprach.c): brand hit area x 6..66 in the menubar;
# dropdown items: item i occupies y = MENUBAR_H + 4 + i*26 .. +22.
BRAND_X, BRAND_Y = 30, 12

def menu_item_y(i):
    return 24 + 4 + i * 26 + 8

def open_menu():
    move_to(BRAND_X, BRAND_Y)
    drain(0.3)
    left_click()
    drain(1.5)

def click_item(i):
    move_to(80, menu_item_y(i))
    drain(0.3)
    left_click()
    drain(2.0)

# Panel geometry: 240x180 centred (x=280), pre-created at y=180 by
# sprach_create_app_menu (s->y = 180).
PANEL_X = (800 - 240) // 2   # 280
PANEL_Y = 180

# 1+2) open menu -> About
open_menu()
click_item(0)   # About This PC
drain(1.0)

# close by clicking outside (upper-left wallpaper)
move_to(120, 150)
drain(0.3)
left_click()
drain(1.5)

# 3) reopen -> Preferences
open_menu()
click_item(1)   # Preferences
drain(1.0)

# click volume "+" (panel-local x 180..200, y 66..82 -> absolute)
move_to(PANEL_X + 190, PANEL_Y + 74)
drain(0.3)
left_click()
drain(1.5)

# close by clicking outside
move_to(120, 150)
drain(0.3)
left_click()
drain(1.5)

# 4) reopen -> Force Quit
open_menu()
click_item(2)   # Force Quit
drain(2.5)

text = acc.decode("utf-8", "replace")

def after(marker, needle):
    i = text.find(marker)
    return i >= 0 and needle in text[i:]

checks = {
    "menu_open":    "app menu open" in text,
    "menu_about":   "app menu: About This PC" in text,
    "about_panel":  "about panel open" in text,
    "panel_close1": after("about panel open", "app panel closed"),
    "menu_prefs":   "app menu: Preferences" in text,
    "prefs_panel":  "preferences panel open" in text,
    "prefs_volume": after("preferences panel open", "prefs: volume -> 80"),
    "panel_close2": after("preferences panel open", "app panel closed"),
    "menu_fq":      "app menu: Force Quit" in text,
    "fq_listed":    "FQ-WIN:" in text,
    "fq_closed":    after("FQ-WIN:", "force quit: closed "),
    "fq_nonzero":   (lambda t: any(
                        "force quit: closed %d" % n in t
                        for n in range(1, 20)))(text),
    "no_panic":     "PANIC" not in text,
    "no_gpf":       "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
