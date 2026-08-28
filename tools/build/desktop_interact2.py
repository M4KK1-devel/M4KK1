#!/usr/bin/env python3
"""Desktop interactive verification via the HMP monitor's mouse_move /
mouse_button commands (they drive the PS/2 device directly)."""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if f.endswith("full-test.iso")],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_desk3.sock"
mon = "/tmp/m4k_desk3.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/desktop_interact2.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
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

def mdrain(t=0.3):
    end = time.time() + t
    while time.time() < end:
        try:
            m.recv(65536)
        except BlockingIOError:
            pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    mdrain(0.2)

def sdrain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d:
                log.write(d)
        except BlockingIOError:
            pass
        time.sleep(0.1)

def shot(name):
    hmp("screendump /tmp/%s.ppm" % name)
    mdrain(1.0)

gx, gy = 400, 300     # assumed guest start (kernel clamps to screen)

def move_to(tx, ty, step=8):
    global gx, gy
    while gx != tx or gy != ty:
        dx = max(-step, min(step, tx - gx))
        dy = max(-step, min(step, ty - gy))
        hmp("mouse_move %d %d" % (dx, dy))
        gx += dx
        gy += dy

def click():
    hmp("mouse_button 1")
    mdrain(0.3)
    hmp("mouse_button 0")
    mdrain(0.3)

mdrain(0.5)
sdrain(30)   # boot to desktop

# Probe: after moving, check serial for mouse activity lines.
move_to(400, 100)
sdrain(2)

# 1) Bluetooth tray glyph (~712, 10)
move_to(712, 10)
sdrain(0.5)
click()
sdrain(2)
shot("desk_bt_panel")

# 2) toggle radio ON (radio text line y≈52 guest, x≈700)
move_to(700, 54)
sdrain(0.4)
click()
sdrain(2)
shot("desk_bt_on")

# 3) brand menu → System Settings
move_to(20, 10)
sdrain(0.4)
click()
sdrain(1.5)
move_to(20, 24 + 28 + 14)
sdrain(0.4)
click()
sdrain(2)
shot("desk_settings")

# 4) swatch t=3 (graphite, row1 col0): panel x=316, y=200
move_to(316 + 34, 200 + 135)
sdrain(0.4)
click()
sdrain(2.5)
shot("desk_theme3")

qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()

text = open("logs/desktop_interact2.log", "rb").read().decode("utf-8", "replace")
checks = {
    "mouse_events": "mouse" in text.lower(),
    "bt_panel_open": "bluetooth" in text.lower(),
    "bt_toggle": ("bluetooth radio ON" in text) or ("bluetooth radio OFF" in text),
    "settings_open": "system settings" in text,
    "theme_switch": "wallpaper theme ->" in text,
    "no_panic": "PANIC" not in text,
    "no_gpf": "GPF" not in text,
}
print("--- serial checks ---")
ok = True
for k, v in checks.items():
    print("%s: %s" % (k, v))
    if not v:
        ok = False
print("RESULT:", "PASS" if ok else "FAIL")
raise SystemExit(0 if ok else 1)
