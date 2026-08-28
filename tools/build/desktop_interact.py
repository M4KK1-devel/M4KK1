#!/usr/bin/env python3
"""Desktop INTERACTIVE verification: QMP input-send-event drives the
real PS/2 mouse — click tray glyphs (bluetooth panel, toggle radio),
click wallpaper swatches in Settings, screendump each state."""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if f.endswith("full-test.iso")],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_desk2.sock"
qmp = "/tmp/m4k_desk2.qmp"
for f in (sock, qmp):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/desktop_interact.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-qmp", "unix:%s,server=on,wait=off" % qmp,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.setblocking(False)

q = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
q.connect(qmp)
q.setblocking(False)
qbuf = b""

def qdrain(t=0.3):
    global qbuf
    end = time.time() + t
    while time.time() < end:
        try:
            qbuf += q.recv(65536)
        except BlockingIOError:
            pass
        time.sleep(0.05)

def qmp_cmd(cmd):
    q.sendall(('{"execute":"%s"}\n' % cmd).encode())
    qdrain(0.2)

qdrain(0.5)
q.sendall(b'{"execute":"qmp_capabilities"}\n')
qdrain(0.3)

def shot(name):
    q.sendall(('{"execute":"screendump","arguments":{"filename":"/tmp/%s.ppm"}}\n' % name).encode())
    qdrain(1.0)

def drain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d:
                log.write(d)
        except BlockingIOError:
            pass
        time.sleep(0.1)

# absolute PS/2 via qemu uses relative events; use input-send-event
# with rel movements from center-ish. Track guest cursor by simulating
# from top-left. Guest starts at (400,300) typical. We move by deltas.
# Simpler: use the PS/2 controller's absolute? Not available. Use rel.

def mouse_move(dx, dy):
    q.sendall(('{"execute":"input-send-event","arguments":{"events":['
               '{"type":"rel","data":{"axis":"x","value":%d}},'
               '{"type":"rel","data":{"axis":"y","value":%d}}]}}\n' % (dx, dy)).encode())
    qdrain(0.15)

def mouse_click():
    q.sendall(b'{"execute":"input-send-event","arguments":{"events":['
              b'{"type":"btn","data":{"down":true,"button":"left"}},'
              b'{"type":"btn","data":{"down":false,"button":"left"}}]}}\n')
    qdrain(0.4)

def move_to(tx, ty):
    """Guest cursor tracks deltas from an assumed start (400,300)."""
    global gx, gy
    dx = tx - gx
    dy = ty - gy
    steps = max(abs(dx), abs(dy), 1)
    n = min(steps, 60)
    for i in range(n):
        mouse_move(int(round(dx / n)), int(round(dy / n)))
    gx, gy = tx, ty

gx, gy = 400, 300
drain(30)   # boot to desktop

# 1) Bluetooth tray glyph: third glyph, x ~ 664+40=704..720, y=10
move_to(712, 10)
drain(0.5)
mouse_click()
drain(2)
shot("desk_bt_panel")

# 2) Click the radio line to toggle ON (panel at clock_x=tray_x0-8,
# radio text y ~ MENUBAR+28 → guest y=24+28=52; x = panel_x+20)
# tray_x0 = clock_x0 - 64 = (800-56-12) - 64 = 668; clock_x = 660
# radio line: y in [24+24, 24+40) → 48..64, x within panel width
move_to(700, 54)
drain(0.5)
mouse_click()
drain(2)
shot("desk_bt_on")

# 3) Wallpaper: click brand → app menu → System Settings → swatch 4
move_to(20, 10)
drain(0.5)
mouse_click()
drain(1.5)
move_to(20, 24 + 28 * 1 + 14)   # item 1 = System Settings
drain(0.5)
mouse_click()
drain(2)
shot("desk_settings")

# swatch 4 (desert): panel centered at (800-168)/2=316; swatch t=0
# x = 316+12..56, y=200+108..122 (row 0); t=0 is classic; click t=0
move_to(316 + 34, 200 + 115)
drain(0.5)
mouse_click()
drain(2)
shot("desk_theme0")
# swatch 4: t=3 → col 0 row 1: x=316+12+34, y=200+128+7
move_to(316 + 34, 200 + 135)
drain(0.5)
mouse_click()
drain(2.5)
shot("desk_theme3")

qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()

text = open("logs/desktop_interact.log", "rb").read().decode("utf-8", "replace")
checks = {
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
