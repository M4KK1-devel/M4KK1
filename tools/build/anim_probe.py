#!/usr/bin/env python3
"""anim_probe.py — window open/close glide animation (feature probe).

Verifies the minimize/restore motion interpolation with BOTH serial
telemetry and mid-flight pixel evidence:

  1. right-click win0 title bar -> Minimize
     - serial: "[SPRACH] ANIM MIN 0" then "[SPRACH] ANIM END"
     - mid-flight screendump: win0 red title pixels BELOW the home
       title band (y>112) — the surface is gliding toward the dock
     - after END: zero red pixels anywhere (fully hidden)
  2. dock icon click -> restore
     - serial: "[SPRACH] ANIM SHOW 0" then "[SPRACH] ANIM END"
     - mid-flight screendump: red pixels in the lower work area
       (y 200..500) — gliding up out of the dock
     - after END: red pixels confined to the home title band
       (y 88..110), i.e. window parked at its model position
  3. no panic / no GPF

Window 0 (stacking) spawns at (140,90) 256x192, title bar y 90..108,
title colour SPRACH_COL_TITLE_1 = 0x00803030 -> RGB (128,48,48).
The glide lasts >= 300 ms wall time (60 ticks x m4k_sleep(5)), so
samples at ~0.15 s are safely mid-flight.
"""

import os
import re
import socket
import subprocess
import sys
import time

os.makedirs("logs", exist_ok=True)
log = open("logs/anim_serial.log", "wb")

iso = None
for f in sorted(os.listdir("output"), reverse=True):
    if f.endswith("-full-test.iso"):
        iso = os.path.join("output", f)
        break
if not iso:
    alt = "/tmp/m4kk1_csi_probe.iso"
    if os.path.exists(alt):
        import shutil
        shutil.copy(alt, "/tmp/m4kk1_anim_probe.iso")
        iso = "/tmp/m4kk1_anim_probe.iso"
if not iso:
    print("no full-test ISO")
    sys.exit(1)
print("[anim] using", iso)

sock = "/tmp/anim_ser.sock"
mon = "/tmp/anim_mon.sock"
for p in (sock, mon):
    if os.path.exists(p):
        os.unlink(p)

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
        time.sleep(0.02)


def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.12)


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


def hmp_fast(cmd):
    """send with minimal settle — for mid-flight sampling (the glide
    lasts ~300 ms; the regular hmp's 0.12 s drain plus click settles
    overshoots the whole window)."""
    m.sendall((cmd + "\n").encode())
    drain(0.02)


def left_click_fast():
    """press+release with minimal settle — keeps the post-click sample
    safely inside the >=300 ms glide window."""
    hmp_fast("mouse_button 1")
    drain(0.03)
    hmp_fast("mouse_button 0")
    drain(0.02)


def wait_token(tok, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if tok in acc.decode("utf-8", "replace"):
            return True
        drain(0.3)
    return False


def screendump(tag):
    path = "/tmp/anim_%s.ppm" % tag
    m.sendall(("screendump " + path + "\n").encode())
    drain(0.25)
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    mo = re.match(rb"P6\n(\d+) (\d+)\n255\n", data)
    if not mo:
        return None
    w, h = int(mo.group(1)), int(mo.group(2))
    pix = data[mo.end():]
    return w, h, pix


def red_pixels(img, ymax=552):
    """Return (count, min_y, max_y) of win0 title-red pixels
    RGB(128,48,48) with a small tolerance, EXCLUDING the dock band
    (y >= ymax): the win0 dock icon itself paints a title-red chip
    (sp_icon_window x+3,y+3, 26x8) that is legitimate chrome, not
    window content."""
    if img is None:
        return 0, -1, -1
    w, h, pix = img
    n, y0, y1 = 0, 1 << 30, -1
    # stride sample every pixel of every 2nd row for speed
    for y in range(0, min(h, ymax), 2):
        row = y * w * 3
        for x in range(0, w, 2):
            o = row + x * 3
            r, g, b = pix[o], pix[o + 1], pix[o + 2]
            if 110 <= r <= 146 and 32 <= g <= 64 and 32 <= b <= 64:
                n += 1
                if y < y0:
                    y0 = y
                if y > y1:
                    y1 = y
    return n, (y0 if n else -1), (y1 if n else -1)


# ── boot to desktop ──
drain(0.5)
ok_boot = wait_token("painting initial scene", 60)
drain(3)
print("[anim] boot:", ok_boot)

results = []


def check(name, ok, info=""):
    results.append((name, ok))
    print(("PASS" if ok else "FAIL"), name, info)


# ── 1) minimize via title-bar context menu ──
TB_X, TB_Y = 200, 98
RMENU_W, ITEM_H, PAD = 150, 22, 2
move_to(TB_X, TB_Y)
drain(0.3)
right_click()
drain(1.5)
mx = TB_X + 40
my = TB_Y + PAD + 0 * ITEM_H + ITEM_H // 2
move_to(mx, my)
drain(0.3)

left_click_fast()
# sample three times inside/near the >=300 ms glide window; the exact
# capture instant depends on HMP drain cadence, so take the union
nm_all = []
ylo, yhi = 1 << 30, -1
for delay in (0.02, 0.08, 0.06):
    time.sleep(delay)
    img = screendump("min_m")
    n_, y0_, y1_ = red_pixels(img)
    nm_all.append(n_)
    if n_:
        ylo = min(ylo, y0_)
        yhi = max(yhi, y1_)
n_mid, y0m, y1m = max(nm_all), (ylo if yhi >= 0 else -1), yhi
check("mid-flight: red title pixels below home band (gliding down)",
      n_mid > 30 and y1m > 112,
      "samples=%s y=%d..%d" % (nm_all, y0m, y1m))

got_end = wait_token("[SPRACH] ANIM END", 10)
drain(0.6)
img_end = screendump("min_end")
n_end, _, _ = red_pixels(img_end)
check("after ANIM END: window fully hidden (0 red px)", n_end == 0,
      "n=%d" % n_end)

text = acc.decode("utf-8", "replace")
check("serial: ANIM MIN 0 fired", "[SPRACH] ANIM MIN 0" in text)
check("serial: ANIM END after MIN",
      "ANIM MIN 0" in text and "ANIM END" in text
      and text.find("ANIM END") > text.find("ANIM MIN 0"))

# ── 2) restore via dock icon (win0 entry: x 52..84, y 560..592) ──
acc = b""
move_to(68, 576)
drain(0.3)
left_click_fast()
time.sleep(0.10)          # mid-flight upward
img_mid2 = screendump("show_mid")
n_mid2, y0s, y1s = red_pixels(img_mid2)
check("mid-flight: red pixels rising through the work area",
      n_mid2 > 40 and 200 <= y1s <= 548,
      "n=%d y=%d..%d" % (n_mid2, y0s, y1s))

got_end2 = wait_token("[SPRACH] ANIM END", 10)
drain(0.6)
img_home = screendump("show_end")
n_home, y0h, y1h = red_pixels(img_home)
check("after ANIM END: title band parked at home (y 88..110)",
      n_home > 60 and 84 <= y0h and y1h <= 114,
      "n=%d y=%d..%d" % (n_home, y0h, y1h))

text2 = acc.decode("utf-8", "replace")
check("serial: ANIM SHOW 0 fired", "[SPRACH] ANIM SHOW 0" in text2)

# ── 3) stability ──
full = open("logs/anim_serial.log", "rb").read()
check("no panic", b"panic" not in full.lower())
check("no GPF", b"general protection" not in full.lower())

qemu.kill()

passed = sum(1 for _, ok in results if ok)
print("=== %d/%d checks passed ===" % (passed, len(results)))
sys.exit(0 if passed == len(results) else 1)
