#!/usr/bin/env python3
"""wallpaper_probe.py — verify the desktop right-click theme submenu.

Drives QEMU via HMP mouse events against the full-test ISO:
  1. right-click the bare wallpaper       -> "rmenu: desktop menu"
  2. left-click "Change Wallpaper"        -> "rmenu: theme menu" (mode 3)
  3. left-click theme item 4 (Desert)     -> "rmenu: theme 4"
     + live apply + "/etc/wallpaper" write ("wallpaper saved: theme 4")
  4. re-open, pick item 1 (Ocean)         -> "rmenu: theme 1"
  5. serial-shell `cat /etc/wallpaper`    -> echoes "1"
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, re, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[wallpaper] rebuilding full-test ISO...")
    subprocess.run(["bash", "tools/build/build_neticons.sh"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   timeout=480)
    isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
iso = os.path.join("output", isos[0])
print("[wallpaper] using", iso)

sock = "/tmp/m4k_wallpaper.sock"
mon = "/tmp/m4k_wallpaper.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/wallpaper_serial.log", "wb")

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
    # HMP "middle" bit = PS/2 right button (see rmenu_repro.py note)
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

# Menu geometry: RMENU_ITEM_W=150, ITEM_H=22, PAD=2
RMENU_W, ITEM_H, PAD = 150, 22, 2
# Bare wallpaper well right of the 3-col icon grid
BARE_X, BARE_Y = 500, 200

def open_theme_menu():
    move_to(BARE_X, BARE_Y)
    drain(0.3)
    right_click()
    drain(1.5)
    # left-click "Change Wallpaper" (item 1); the submenu anchors at
    # THIS mouse position (sprach sets rmenu_x/y = mouse at activate)
    mx = BARE_X + 40
    my = BARE_Y + PAD + 1 * ITEM_H + ITEM_H // 2
    move_to(mx, my)
    drain(0.3)
    left_click()
    drain(1.5)
    return my   # submenu anchor y

def pick_theme(idx, anchor_y):
    # submenu opened at anchor_y: item idx screen-rect starts at
    # anchor_y + PAD + idx*ITEM_H (hit test adds back MENUBAR_H)
    mx = BARE_X + 40
    my = anchor_y + PAD + idx * ITEM_H + ITEM_H // 2
    move_to(mx, my)
    drain(0.3)
    left_click()
    drain(2.0)

# 1) theme 4 (Desert)
ay = open_theme_menu()
pick_theme(4, ay)

# 2) re-open, theme 1 (Ocean)
ay = open_theme_menu()
pick_theme(1, ay)

# 3) verify persistence via serial shell: cat /etc/wallpaper
#    (full-test m4sht reads stdin from the serial console)
s.sendall(b"cat /etc/wallpaper\r\n")
drain(4.0)

text = acc.decode("utf-8", "replace")
checks = {
    "desktop_menu":  "rmenu: desktop menu" in text,
    "theme_menu":    "rmenu: theme menu" in text,
    "theme4":        "rmenu: theme 4" in text,
    "theme1":        "rmenu: theme 1" in text,
    "saved4":        "wallpaper saved: theme 4" in text,
    "saved1":        "wallpaper saved: theme 1" in text,
    # shell echo of the file content: "1" on its own line right after
    # the cat command echo (LF-only output, ANSI-coloured prompt after)
    "persist":       "cat /etc/wallpaper" in text and
                     re.search(r"cat /etc/wallpaper\s*\n\s*1\s*\n", text)
                     is not None,
    "no_panic":      "PANIC" not in text,
    "no_gpf":        "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
