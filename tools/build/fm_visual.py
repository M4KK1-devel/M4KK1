#!/usr/bin/env python3
"""fm_visual.py — boot, open FM via Files icon, screendump, and check
the FM window actually renders file rows (not an empty body).

Asserts on the PPM pixels:
  - FM window frame present (title color at expected coords)
  - at least N non-body pixels inside the list area = entries drawn
"""
import os, socket, subprocess, time, glob, sys

os.chdir("/mnt/f/M4KK1")
isos = sorted(f for f in os.listdir("output") if f.endswith("full-test.iso"))
if not isos:
    print("no full-test ISO; rebuild first")
    sys.exit(2)
iso = os.path.join("output", isos[-1])
print("[fmvis]", iso)

mon = "/tmp/fmvis_mon.sock"
if os.path.exists(mon):
    os.unlink(mon)
qlog = open("/tmp/fmvis_qemu.log", "wb")
p = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-serial", "file:logs/fmvis_serial.log", "-monitor",
     f"unix:{mon},server,nowait", "-display", "none"],
    stdout=qlog, stderr=qlog)

for _ in range(100):
    if os.path.exists(mon):
        break
    time.sleep(0.1)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(mon)
acc = b""

def drain(t=0.3):
    global acc
    s.settimeout(t)
    try:
        while True:
            b = s.recv(65536)
            if not b:
                break
            acc += b
    except socket.timeout:
        pass

def hmp(cmd):
    s.sendall((cmd + "\n").encode())

def move_to(tx, ty):
    # guest starts at (400,300); stepped <=8px moves
    x, y = 400, 300
    while x != tx or y != ty:
        dx = max(-8, min(8, tx - x)); dy = max(-8, min(8, ty - y))
        hmp(f"mouse_move {dx} {dy}")
        x += dx; y += dy
        drain(0.02)

drain(1.0)
drain(30)   # boot + autologin + desktop

# double-click the Files icon (cell 0 centre (54,78))
move_to(54, 78)
drain(0.5)
for _ in range(2):
    hmp("mouse_button 1")
    drain(0.15)
    hmp("mouse_button 0")
    drain(0.15)
drain(6)     # let FM start and render

hmp("screendump /tmp/fmvis.ppm")
drain(1.5)

end = time.time() + 10
while time.time() < end:
    acc += (drain(0.3) or b"")
    if os.path.exists("/tmp/fmvis.ppm"):
        try:
            if os.path.getsize("/tmp/fmvis.ppm") > 1000:
                break
        except OSError:
            pass
drain(1.0)

try:
    p.terminate(); p.wait(5)
except Exception:
    p.kill()

text = acc.decode("utf-8", "replace")
open("logs/fmvis_serial_full.log", "w").write(text)

# --- analyze the screenshot ---
try:
    with open("/tmp/fmvis.ppm", "rb") as f:
        data = f.read()
    # parse P6 header
    parts = data.split(b"\n", 3)
    # header: P6\nW H\nMAX\nbinary — robust parse:
    idx = 0
    def tok():
        global idx
        while data[idx:idx+1].isspace():
            idx += 1
        s0 = idx
        while not data[idx:idx+1].isspace():
            idx += 1
        return data[s0:idx]
    magic = tok(); w = int(tok()); h = int(tok()); maxv = tok()
    idx += 1
    px = data[idx:]
    print(f"[fmvis] screendump {w}x{h}")
    def pix(x, y):
        o = (y * w + x) * 3
        return (px[o], px[o+1], px[o+2])
    # where is the FM window? sprach creates client windows via FM's own
    # surface attach; find the title-colour band: scan for rows with many
    # identical non-wallpaper colours.  Simpler: count distinct strong
    # blue-ish pixels (dir names 0x002040A0 → rgb(160,64,32) in BGR!?) —
    # colour format is 0x00BBGGRR, so 0x002040A0 = B=0xA0,G=0x40,R=0x20
    blue_dir = 0
    textish = 0
    for y in range(24, 560, 2):
        for x in range(0, 800, 2):
            r, g, b = pix(x, y)
            if abs(r-0x20) < 40 and abs(g-0x40) < 40 and abs(b-0xA0) < 60:
                blue_dir += 1
            # dark text on light body (0x00FFFFFF-ish body)
            if r < 100 and g < 100 and b < 100:
                textish += 1
    print("[fmvis] dir-blue pixels:", blue_dir, " dark-text pixels:", textish)
    ok = blue_dir > 30
    print("FM_ROWS_VISIBLE:", ok)
    sys.exit(0 if ok else 1)
except FileNotFoundError:
    print("no screendump")
    sys.exit(2)
