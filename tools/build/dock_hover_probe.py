#!/usr/bin/env python3
"""dock_hover_probe.py — taskbar-hover-preview verification.

Boots the full-test ISO headless, moves the mouse onto the launchpad
dock icon, screendumps, and asserts on the pixels:
  1. hover highlight plate (0x00A0A0B0) under the launchpad icon
     (sample x=22..26, y=566..570 band inside the plate, majority vote)
  2. tooltip chip background (0x00C0C0C0) + dark 5x7 glyphs
     (0x00101010) + accent marker pixel at the dock's top-left
Then moves the cursor away to the work area and asserts the plate +
chip pixels are GONE (hover state clears).  Serial log is checked for
the "[SPRACH] dock hover" trace lines.

Colour format is 0x00BBGGRR (matches fm_visual.py conventions).
"""
import os, socket, subprocess, time, sys

os.chdir("/mnt/f/M4KK1")
isos = sorted(f for f in os.listdir("output")
              if f.endswith(".iso") and "full" in f)
if not isos:
    print("no full-test ISO; rebuild first")
    sys.exit(2)
iso = os.path.join("output", isos[-1])
print("[dockhover]", iso)

mon = "/tmp/dockhover_qmp.sock"
if os.path.exists(mon):
    os.unlink(mon)
ser_sock = "/tmp/dockhover_ser.sock"
if os.path.exists(ser_sock):
    os.unlink(ser_sock)
qlog = open("/tmp/dockhover_qemu.log", "wb")
p = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-serial", "file:logs/dockhover_serial.log",
     "-qmp", f"unix:{mon},server,nowait",
     "-monitor", f"unix:{ser_sock},server,nowait",
     "-display", "none"],
    stdout=qlog, stderr=qlog)

for _ in range(100):
    if os.path.exists(mon) and os.path.exists(ser_sock):
        break
    time.sleep(0.1)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(mon)
s.setblocking(False)

m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(ser_sock)
m.setblocking(False)

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
    m.settimeout(0.05)
    try:
        while True:
            b = m.recv(65536)
            if not b:
                break
    except socket.timeout:
        pass

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.15)

def jump_to(tx, ty):
    """Absolute move via HMP mouse_move (QEMU's default PS/2 mouse —
    the injection path verified working by appmenu_probe.py; QMP
    input-send-event does NOT reach the guest on this setup).
    Hops of 8px keep deltas well inside the PS/2 255 limit.  HMP
    mouse_move y grows DOWN like screen y (QEMU applies the PS/2
    y inversion itself)."""
    global cur_x, cur_y
    while cur_x != tx or cur_y != ty:
        dx = max(-8, min(8, tx - cur_x))
        dy = max(-8, min(8, ty - cur_y))
        hmp("mouse_move %d %d" % (dx, dy))
        cur_x += dx
        cur_y += dy
    drain(0.8)

cur_x, cur_y = 400, 300

drain(1.0)

# Boot (incl. full-test auto-run CPTEST etc.) can take well over 30 s.
# Poll the serial log until sprach reports its main loop, THEN move.
deadline = time.time() + 180
while time.time() < deadline:
    drain(2.0)
    try:
        with open("logs/dockhover_serial.log", "r", errors="replace") as f:
            if "Entering main loop" in f.read():
                break
    except OSError:
        pass
drain(3.0)   # let surfaces settle

# QMP handshake before any command
drain(0.5)
s.sendall(b'{"execute":"qmp_capabilities"}\n')
drain(0.3)

def dump(path):
    if os.path.exists(path):
        os.unlink(path)
    s.sendall(('{"execute":"screendump","arguments":{"filename":"%s"}}\n'
               % path).encode())
    end = time.time() + 10
    while time.time() < end:
        drain(0.3)
        if os.path.exists(path):
            try:
                if os.path.getsize(path) > 1000:
                    break
            except OSError:
                pass
    drain(0.5)

def load_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    idx = 0
    def tok():
        nonlocal idx
        while data[idx:idx+1].isspace():
            idx += 1
        s0 = idx
        while not data[idx:idx+1].isspace():
            idx += 1
        return data[s0:idx]
    magic = tok(); w = int(tok()); h = int(tok()); tok()
    idx += 1
    px = data[idx:]
    def pix(x, y):
        o = (y * w + x) * 3
        return (px[o], px[o+1], px[o+2])
    return pix

# --- Phase 1: hover the launchpad icon (centre ~ (24, 584)) ---
jump_to(24, 584)
drain(2.0)      # let sprach repaint + composite the dock
dump("/tmp/dockhover1.ppm")
pix = load_ppm("/tmp/dockhover1.ppm")

def near(c, t, tol=18):
    return all(abs(a - b) <= tol for a, b in zip(c, t))

# Byte order (live-verified this round): PPM tuples read the u32
# colour straight as 0x00RRGGBB, i.e. 0x00A0A0B0 -> (0xA0,0xA0,0xB0),
# 0x00FFD060 -> (0xFF,0xD0,0x60).
#
# launchpad icon at bx=8, plate = (6,558)-(41,593).  The tooltip
# chip (x 2..59, y 554..569) covers the plate's upper rows, so
# sample the plate's left margin columns x=6..7 BELOW the chip
# (y 571..593; icon body starts at x=8, chip ends at y=569).
plate_hits = sum(1 for x in (6, 7) for y in range(571, 594)
                 if near(pix(x, y), (0xA0, 0xA0, 0xB0)))
print(f"[dockhover] launchpad plate margin hits={plate_hits}")
ok_plate_l = plate_hits >= 30

# tooltip chip (x 2..59, y 554..569): bg 0x00C0C0C0, dark 5x7
# glyphs 0x00101010 ("Launchpad" drawn at (4,557), 9 chars).
chip_bg = sum(1 for x in range(4, 56) for y in range(556, 570)
              if near(pix(x, y), (0xC0, 0xC0, 0xC0)))
glyph = sum(1 for x in range(4, 56) for y in range(556, 570)
            if near(pix(x, y), (0x10, 0x10, 0x10), 30))
# accent marker pixel at local (2+cw,17) = (60,569) screen.
accent = sum(1 for x in range(52, 68)
             if near(pix(x, 569), (0xFF, 0xD0, 0x60), 40))
print(f"[dockhover] chip_bg={chip_bg} glyph={glyph} accent={accent}")
ok_tip = chip_bg > 40 and glyph > 15 and accent >= 1

# --- Phase 2: hover the far-right terminal launcher (775,575) ---
# lx=760 -> plate=(758,558)-(793,593): NO chip overlap here, so all
# four margin columns are pure plate colour (icon body x 760..791).
jump_to(775, 575)
drain(2.0)
dump("/tmp/dockhover2.ppm")
pix2 = load_ppm("/tmp/dockhover2.ppm")
ring = sum(1 for x in (758, 759, 792, 793) for y in range(560, 592)
           if near(pix2(x, y), (0xA0, 0xA0, 0xB0)))
print(f"[dockhover] launcher plate ring hits={ring}")
ok_plate_t = ring >= 100

# --- Phase 3: move away to work area — hover state must clear ---
jump_to(400, 300)
drain(2.0)
dump("/tmp/dockhover3.ppm")
pix3 = load_ppm("/tmp/dockhover3.ppm")
plate3 = sum(1 for x in (6, 7) for y in range(571, 594)
             if near(pix3(x, y), (0xA0, 0xA0, 0xB0)))
plate3b = sum(1 for x in (758, 759, 792, 793) for y in range(560, 592)
              if near(pix3(x, y), (0xA0, 0xA0, 0xB0)))
chip3 = sum(1 for x in range(4, 56) for y in range(556, 570)
            if near(pix3(x, y), (0xC0, 0xC0, 0xC0)))
print(f"[dockhover] after-leave launchpad={plate3} launcher={plate3b} chip={chip3}")
ok_clear = plate3 == 0 and plate3b == 0 and chip3 == 0

try:
    p.terminate(); p.wait(5)
except Exception:
    p.kill()

# serial trace check
trace = ""
try:
    trace = open("logs/dockhover_serial.log", "r", errors="replace").read()
except OSError:
    pass
ok_trace = ("dock hover 0" in trace) and ("dock hover none" in trace)
print(f"[dockhover] serial trace ok={ok_trace}")

ok_plate = ok_plate_l and ok_plate_t
print(f"HIGHLIGHT={'PASS' if ok_plate else 'FAIL'}")
print(f"TOOLTIP={'PASS' if ok_tip else 'FAIL'}")
print(f"CLEAR={'PASS' if ok_clear else 'FAIL'}")
print(f"TRACE={'PASS' if ok_trace else 'FAIL'}")
all_ok = ok_plate and ok_tip and ok_clear and ok_trace
print("DOCK_HOVER_PROBE:", "PASS (4/4)" if all_ok else "FAIL")
sys.exit(0 if all_ok else 1)
