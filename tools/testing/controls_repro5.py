#!/usr/bin/env python3
"""Controls repro v5: keyboard forwarding, verdict via screendump.
Spawn terminal, focus, type 'echo HI' + Enter, screendump the frame,
crop the terminal window region for visual inspection.
"""
import socket, subprocess, sys, time, json

ISO = "/tmp/mtM.iso"
SER = "/tmp/ctrl_run6.log"
PORT = 4456
SHOT = "/tmp/ctrl_run6.ppm"
CROP = "/tmp/ctrl_term_crop.ppm"

def qmp(s, cmd):
    s.sendall((json.dumps(cmd) + "\n").encode())
    time.sleep(0.3)
    try:
        s.settimeout(2.0)
        return s.recv(65536).decode(errors="replace")
    except socket.timeout:
        return ""

def move(s, dx, dy):
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "rel", "data": {"axis": "x", "value": dx}},
        {"type": "rel", "data": {"axis": "y", "value": dy}}]}})

def click(s):
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "btn", "data": {"down": True, "button": "left"}}]}})
    time.sleep(0.15)
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "btn", "data": {"down": False, "button": "left"}}]}})

def key(s, qcode, hold=0.12):
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": True, "key": qcode}}]}})
    time.sleep(hold)
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": False, "key": qcode}}]}})

def goto(s, x, y):
    for _ in range(12):
        move(s, -100, -100); time.sleep(0.02)
    cx = cy = 0
    while cx < x:
        step = min(100, x - cx); move(s, step, 0); cx += step; time.sleep(0.02)
    while cy < y:
        step = min(100, y - cy); move(s, 0, step); cy += step; time.sleep(0.02)

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none", "-no-reboot",
     "-serial", f"file:{SER}",
     "-qmp", f"tcp:127.0.0.1:{PORT},server=on,wait=off"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    s = None
    for _ in range(60):
        time.sleep(1)
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2); break
        except OSError:
            continue
    if s is None:
        print("FATAL: no QMP"); sys.exit(1)
    time.sleep(1.0)
    s.settimeout(2.0)
    try: s.recv(65536)
    except Exception: pass
    qmp(s, {"execute": "qmp_capabilities"})

    def serial():
        with open(SER, "rb") as f:
            return f.read().decode(errors="replace")

    ready = False
    for _ in range(150):
        time.sleep(1)
        if "mode 'stacking' initialized" in serial():
            ready = True; break
    print("desktop ready:", ready)
    time.sleep(5)

    # spawn terminal
    goto(s, 776, 588); time.sleep(0.3); click(s)
    for _ in range(60):
        time.sleep(1)
        if "terminal window registered" in serial():
            break
    time.sleep(6)
    print("terminal up:", "terminal window registered" in serial())

    # baseline shot BEFORE typing
    qmp(s, {"execute": "screendump", "arguments": {"filename": "/tmp/ctrl_before.ppm"}})
    time.sleep(1)

    # click terminal body to focus
    goto(s, 200, 200); time.sleep(0.3); click(s)
    time.sleep(1)

    # type echo HI
    for ch in "echo hi":
        key(s, ch); time.sleep(0.08)
    key(s, "ret")
    time.sleep(4)

    # shot AFTER typing
    qmp(s, {"execute": "screendump", "arguments": {"filename": SHOT}})
    time.sleep(2)

    # crop terminal window (60,40)-(60+560?,40+TERM_H) — TERM_W/H unknown;
    # crop generous region 60,40 .. 560,420
    with open(SHOT, "rb") as f:
        data = f.read()
    # parse P6 header
    def parse_ppm(buf):
        idx = 0
        parts = []
        while len(parts) < 4:
            # read token
            while buf[idx:idx+1].isspace(): idx += 1
            if buf[idx:idx+1] == b"#":
                while buf[idx:idx+1] not in (b"\n", b""): idx += 1
                continue
            start = idx
            while not buf[idx:idx+1].isspace(): idx += 1
            parts.append(buf[start:idx])
        idx += 1  # single whitespace
        return parts, idx
    parts, off = parse_ppm(data)
    w, h = int(parts[1]), int(parts[2])
    row = w * 3
    x0, y0, x1, y1 = 55, 35, min(640, w), min(420, h)
    out = bytearray()
    out += f"P6\n{x1-x0} {y1-y0}\n255\n".encode()
    for y in range(y0, y1):
        out += data[off + y*row + x0*3 : off + y*row + x1*3]
    with open(CROP, "wb") as f:
        f.write(bytes(out))
    print(f"cropped {x1-x0}x{y1-y0} -> {CROP} (screen {w}x{h})")
finally:
    qemu.kill()
