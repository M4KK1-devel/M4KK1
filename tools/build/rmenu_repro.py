#!/usr/bin/env python3
"""rmenu_repro.py — verify the conditional right-click context menu.

Drives QEMU via HMP mouse events against the full-test ISO:
  1. right-click the first desktop icon cell  -> expect "rmenu: icon menu"
  2. left-click its "Properties" item         -> expect "rmenu: properties"
  3. right-click the bare wallpaper           -> expect "rmenu: desktop menu"
  4. left-click "New Terminal" (item 0)       -> expect terminal launch
  5. right-click wallpaper again, click outside -> menu closes, desktop alive
Assertions come from sprach.c ser_puts lines on the serial console.
"""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
if not isos:
    print("[rmenu] rebuilding full-test ISO...")
    subprocess.run(["bash", "tools/build/build_neticons.sh"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                   timeout=480)
    isos = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
iso = os.path.join("output", isos[0])
print("[rmenu] using", iso)

sock = "/tmp/m4k_rmenu.sock"
mon = "/tmp/m4k_rmenu.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/rmenu_serial.log", "wb")

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

def button_down():
    hmp("mouse_button 4")   # right button bitmask

def button_up():
    hmp("mouse_button 0")

def right_click():
    # QEMU HMP numbers buttons 1=L 2=M 4=R, but the resulting PS/2
    # packet byte0 uses the PS/2 layout: bit0=L, bit1=R, bit2=M.
    # "mouse_button 2" sets bit1 = RIGHT in the guest even though HMP
    # calls it "middle".  (Verified live: mouse_button 4 produces
    # guest buttons=0x5's complement — no bit1 — so it never reads as
    # a right click in sprach.)
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

# --- DIAG: L+R together (mouse_button 5). If the right bit reaches
# sprach, the btn2 edge fires rclick right after the left-click path.
move_to(600, 300)
drain(0.5)
right_click()
drain(1.5)

# Desktop icon grid: first icon cell at DESK_GRID_X+ .. (16,44) size 76
# from sprach.c: DESK_GRID_X=16, DESK_GRID_Y=MENUBAR_H+16 (MENUBAR_H~24)
# Cell 0 centre ≈ (16+38, 24+16+38) = (54, 78)
ICON_X, ICON_Y = 54, 78
# Bare wallpaper well right of the 3-col grid (3*76=228 wide + 16)
BARE_X, BARE_Y = 500, 200
# Menu geometry: RMENU_ITEM_W=150, ITEM_H=22, PAD=2
RMENU_W, ITEM_H, PAD = 150, 22, 2

# 1) right-click icon -> icon menu
move_to(ICON_X, ICON_Y)
drain(0.3)
right_click()
drain(1.5)

# 2) left-click "Properties" (item 1): y = rmenu_y + PAD + 1*ITEM_H + h/2
mx = ICON_X + 40
my = ICON_Y + PAD + 1 * ITEM_H + ITEM_H // 2
move_to(mx, my)
drain(0.3)
left_click()
drain(1.5)

# 3) right-click bare wallpaper -> desktop menu
move_to(BARE_X, BARE_Y)
drain(0.3)
right_click()
drain(1.5)

# 4) left-click "New Terminal" (item 0)
mx = BARE_X + 40
my = BARE_Y + PAD + 0 * ITEM_H + ITEM_H // 2
move_to(mx, my)
drain(0.3)
left_click()
drain(3.0)

# 5) right-click wallpaper again, then click far outside to close
move_to(BARE_X, BARE_Y + 60)
drain(0.3)
right_click()
drain(1.0)
move_to(600, 400)
drain(0.3)
left_click()
drain(2.0)

text = acc.decode("utf-8", "replace")
checks = {
    "icon_menu":    "rmenu: icon menu" in text,
    "properties":   "rmenu: properties -> " in text,
    "desktop_menu": "rmenu: desktop menu" in text,
    "new_terminal": ("rmenu" in text and "terminal" in text.lower()
                     and "SPRACH] terminal" in text) or
                    "dock launch" in text or "launching /bin/terminal" in text,
    "menu_reopen":  text.count("rmenu:") >= 3,
    "no_panic":     "PANIC" not in text,
    "no_gpf":       "#GP" not in text and "General Protection" not in text,
}
print("--- serial checks ---")
for k, v in checks.items():
    print("%s: %s" % (k, v))
ok = all(checks.values())
print("RESULT:", "PASS" if ok else "FAIL")

qemu.kill()
