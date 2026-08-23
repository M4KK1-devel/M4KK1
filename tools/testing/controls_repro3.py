#!/usr/bin/env python3
"""Controls repro v3: window controls after spawning terminal.
Sequence:
  1. far-right launcher -> spawn terminal, wait for slot= registration
  2. click dock terminal icon -> expect "Dock icon clicked: terminal"
  3. click terminal window close button (TERMINAL CLOSE)
  4. click dock terminal icon again -> re-show (window was closed)
Terminal geometry: need title bar coords. sprach windows: title bar
buttons at window left; terminal default position from sprach.c.
"""
import socket, subprocess, sys, time, json

ISO = "/tmp/mtM.iso"
SER = "/tmp/ctrl_run4.log"
PORT = 4454

def qmp(s, cmd):
    s.sendall((json.dumps(cmd) + "\n").encode())
    time.sleep(0.3)
    try:
        s.settimeout(1.0)
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

    ready = False
    for _ in range(150):
        time.sleep(1)
        try:
            with open(SER, "rb") as f:
                t = f.read().decode(errors="replace")
            if "mode 'stacking' initialized" in t:
                ready = True; break
        except FileNotFoundError:
            continue
    print("desktop ready:", ready)
    time.sleep(5)

    def serial():
        with open(SER, "rb") as f:
            return f.read().decode(errors="replace")

    # 1. spawn terminal
    goto(s, 776, 588); time.sleep(0.3); click(s)
    for _ in range(60):
        time.sleep(1)
        if "terminal window registered" in serial():
            break
    time.sleep(4)
    print("terminal up:", "terminal window registered" in serial())

    # 2. dock terminal icon: after launchpad icon + window icons.
    # launchpad at x 8..40; window icons start bx=52 pitch 44.
    # Only terminal exists -> its icon at x 52..84.
    goto(s, 68, 588); time.sleep(0.3); click(s)
    time.sleep(2)
    print("dock focus marker:", "Dock icon clicked: terminal" in serial())

    # 3. window close button — terminal title bar. Terminal window
    # default geometry from sprach.c: around x=200,y=100 (guess A);
    # we will scan: click at leftmost title bar pixel rows.
    # Instead of guessing, use TERMINAL CLOSE marker via min/close
    # buttons at title-left. Try a few candidate spots.
    for (cx, cy) in [(73, 49), (87, 49), (101, 49)]:
        goto(s, cx, cy); time.sleep(0.3); click(s)
        time.sleep(1.5)
        if "TERMINAL CLOSE" in serial() or "TERMINAL MIN" in serial():
            print(f"window button HIT at ({cx},{cy})")
            break
    else:
        print("window buttons: no hit (geometry unknown)")
    time.sleep(2)

    log = serial()
    print("---- markers ----")
    for m in ["dock launch: terminal", "terminal window registered",
              "Dock icon clicked: terminal", "TERMINAL CLOSE",
              "TERMINAL MIN", "TERMINAL MAX", "TERMINAL RESTORE"]:
        print(f"{m}: {'HIT' if m in log else 'MISS'}")
    print("---- tail ----")
    for line in log.splitlines()[-15:]:
        print(line)
finally:
    qemu.kill()
