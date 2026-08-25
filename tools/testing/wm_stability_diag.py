#!/usr/bin/env python3
"""WM stability diagnosis: boot full-test (autologin) ISO, watch 300 s.
Reports: WM restart count (serial), heartbeat liveness via serial tail,
Dock/Bar pixel assertions from periodic screendumps."""
import socket, subprocess, time, json, sys, os

import glob
ISO = (sorted(glob.glob("/mnt/f/M4KK1/output/m4kk1_*-full-test.iso"))
        or [""])[-1]
SER = "/tmp/wm_diag_serial.log"
PORT = 4483
SHOTS = "/tmp/wm_diag"
if not ISO:
    sys.exit("no full-test ISO found")

subprocess.run(["rm", "-f", SER] + [f"{SHOTS}_{i}.ppm" for i in range(4)], check=False)

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-serial", f"file:{SER}",
     "-display", "none", "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(3)
    s = None
    for _ in range(20):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
            break
        except ConnectionRefusedError:
            time.sleep(1)
    if s is None:
        raise ConnectionRefusedError("QMP never opened")
    s.settimeout(3)
    def qmp(obj):
        s.sendall((json.dumps(obj) + "\r\n").encode())
        time.sleep(0.3)
        try:
            while True:
                if not s.recv(65536): break
        except socket.timeout:
            pass
    qmp({"execute": "qmp_capabilities"})

    t0 = time.time()
    boot_ok = False
    while time.time() - t0 < 60:
        time.sleep(2)
        try:
            t = open(SER, "rb").read().decode(errors="replace")
        except FileNotFoundError:
            continue
        if "mode 'stacking' initialized" in t:
            boot_ok = True
            break
        if "PANIC" in t:
            print("PANIC during boot!")
            break
    print("desktop initialized:", boot_ok)

    # Watch 300 s, screendump every 75 s
    for i in range(4):
        time.sleep(75)
        qmp({"execute": "screendump", "arguments": {"filename": f"{SHOTS}_{i}.ppm"}})
        t = open(SER, "rb").read().decode(errors="replace")
        restarts = t.count("WM heartbeat stalled")
        wm_starts = t.count("[SPRACH] Window manager starting")
        copland_starts = t.count("[COPLAND] Display server starting")
        print(f"t={75*(i+1)}s: WM starts={wm_starts} restarts={restarts} "
              f"copland starts={copland_starts} PANIC={'PANIC' in t} "
              f"EXC={'[EXC]' in t}")
finally:
    qemu.terminate()
    qemu.wait()

t = open(SER, "rb").read().decode(errors="replace")
print("=== serial tail (last 25 lines) ===")
for l in t.splitlines()[-25:]:
    print(" ", l)

# Pixel assertions on the last shot
import glob
shots = sorted(glob.glob(f"{SHOTS}_*.ppm"))
if shots:
    d = open(shots[-1], "rb").read()
    parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    pix = parts[3]
    def px(x, y):
        o = (y * w + x) * 3
        return tuple(pix[o:o+3])
    # Menubar band y=10: brand area left, clock right
    def band_dark_ratio(y, x0, x1):
        dark = n = 0
        for x in range(x0, x1, 4):
            r, g, b = px(x, y)
            n += 1
            if r < 90 and g < 90 and b < 110: dark += 1
        return dark / n if n else 0
    print("last shot:", shots[-1])
    print("menubar y=10 dark ratio (left 0-160):", band_dark_ratio(10, 4, 160))
    print("menubar y=10 dark ratio (right 640-796):", band_dark_ratio(10, 640, 796))
    print("dock y=580 dark ratio (4-796):", band_dark_ratio(580, 4, 796))
    print("sample px: (0,10)", px(0,10), "(400,10)", px(400,10),
          "(400,580)", px(400,580), "(400,300)", px(400,300))
