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
qlog = open("/tmp/dockhover_qemu.log", "wb")
p = subprocess.Popen(
    ["qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
     "-serial", "file:logs/dockhover_serial.log",
     "-qmp", f"unix:{mon},server,nowait", "-display", "none"],
    stdout=qlog, stderr=qlog)

for _ in range(100):
    if os.path.exists(mon):
        break
    time.sleep(0.1)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(mon)
s.setblocking(False)
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

def jump_to(tx, ty):
    """Relative move via QMP input-send-event, split into hops of
    <=200 px: the PS/2 packet format caps deltas at 255 (overflow
    packets get clamped by the kernel driver), so one big event would
    undershoot.  y is inverted (PS/2 y grows up, screen y down)."""
    global cur_x, cur_y
    while cur_x != tx or cur_y != ty:
        dx = max(-100, min(100, tx - cur_x))
        dy = max(-100, min(100, ty - cur_y))
        s.sendall(('{"execute":"input-send-event","arguments":{"events":['
                   '{"type":"rel","data":{"axis":"x","value":%d}},'
                   '{"type":"rel","data":{"axis":"y","value":%d}}]}}\n'
                   % (dx, -dy)).encode())
        cur_x += dx
        cur_y += dy
        drain(0.15)
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

# launchpad icon at bx=8, icon_y=8 within the 48px dock at y=552;
# plate = (6,554)-(42,590).  Sample inside the plate border ring but
# off the icon body: x in 20..28, y in 566..570 (icon rows) — the
# icon fills 8..39 inside, so instead sample the plate's left margin
# column x=7 (between plate edge 6 and icon 8) and the corner band
# x=20..28/y=555..556 above the icon.
plate_hits = 0
for y in range(554, 590):
    if near(pix(7, y), (0xB0, 0xA0, 0xA0)):   # BGR: B=0xB0,G=0xA0,R=0xA0
        plate_hits += 1
corner_hits = sum(1 for x in range(20, 29) for y in (555, 556)
                  if near(pix(x, y), (0xB0, 0xA0, 0xA0)))
print(f"[dockhover] plate column hits={plate_hits} corner={corner_hits}")
ok_plate = plate_hits >= 20 and corner_hits >= 6

# tooltip chip at (2,554)-(2+cw,570): background 0xC0C0C0, text 0x101010.
chip_bg = sum(1 for x in range(4, 56) for y in range(556, 570)
              if near(pix(x, y), (0xC0, 0xC0, 0xC0)))
glyph = sum(1 for x in range(4, 56) for y in range(556, 570)
            if near(pix(x, y), (0x10, 0x10, 0x10), 30))
# accent marker pixel right after the chip: scan y=571 x=6..70
accent = sum(1 for x in range(4, 80) if near(pix(x, 571), (0xC0, 0xE0, 0x50), 30))
print(f"[dockhover] chip_bg={chip_bg} glyph={glyph} accent={accent}")
ok_tip = chip_bg > 40 and glyph > 15 and accent >= 1

# --- Phase 2: move away to work area — hover state must clear ---
jump_to(400, 300)
drain(2.0)
dump("/tmp/dockhover2.ppm")
pix2 = load_ppm("/tmp/dockhover2.ppm")
plate2 = sum(1 for y in range(554, 590) if near(pix2(7, y), (0xB0, 0xA0, 0xA0)))
chip2 = sum(1 for x in range(4, 56) for y in range(556, 570)
            if near(pix2(x, y), (0xC0, 0xC0, 0xC0)))
print(f"[dockhover] after-leave plate={plate2} chip={chip2}")
ok_clear = plate2 == 0 and chip2 == 0

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

print(f"HIGHLIGHT={'PASS' if ok_plate else 'FAIL'}")
print(f"TOOLTIP={'PASS' if ok_tip else 'FAIL'}")
print(f"CLEAR={'PASS' if ok_clear else 'FAIL'}")
print(f"TRACE={'PASS' if ok_trace else 'FAIL'}")
all_ok = ok_plate and ok_tip and ok_clear and ok_trace
print("DOCK_HOVER_PROBE:", "PASS (4/4)" if all_ok else "FAIL")
sys.exit(0 if all_ok else 1)
