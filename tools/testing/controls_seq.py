#!/usr/bin/env python3
"""Full window-controls sequence test (black-box):
spawn terminal via far-right dock launcher, then exercise every title
bar button IN THE RIGHT ORDER so each gets validated:
  MIN (hide) -> dock restore -> MAX (fullscreen) -> RESTORE -> CLOSE.
Button geometry (sprach.h): window at (60,40); CTRL_Y=4, CTRL_SIZE=10;
CLOSE x=8, MIN x=22, MAX x=36 → centers: close(73,49) min(87,49) max(101,49).
"""
import socket, subprocess, sys, time, json, os

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build5-alpha1-full-test.iso"
SER = "/tmp/ctrl_seq5.log"
PORT = 4479

if not os.path.exists(ISO):
    sys.exit(f"FAIL: ISO missing: {ISO}")

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

    # spawn terminal via far-right dock launcher
    goto(s, 776, 588); time.sleep(0.3); click(s)
    for _ in range(60):
        time.sleep(1)
        if "terminal window registered" in serial():
            break
    time.sleep(4)
    print("terminal up:", "terminal window registered" in serial())

    # 1. MIN -> expect TERMINAL MIN
    goto(s, 87, 49); time.sleep(0.3); click(s); time.sleep(2)
    print("MIN:", "HIT" if "TERMINAL MIN" in serial() else "MISS")
    # hidden -> restore via terminal dock icon. Dock order: launchpad
    # (x8..40), then window icons starting bx=52 pitch 44. cptest
    # claims slot for window 0 at 52..84, cptest fills windows 0+1+2 (bx 52..184), so terminal icon = 184..216.
    goto(s, 200, 588); time.sleep(0.3); click(s); time.sleep(2)

    # 2. MAX -> expect TERMINAL MAX
    goto(s, 101, 49); time.sleep(0.3); click(s); time.sleep(2)
    print("MAX:", "HIT" if "TERMINAL MAX" in serial() else "MISS")

    # 3. RESTORE (max button again) -> expect TERMINAL RESTORE.
    # While maximized the window spans the work area (0,24)-(800,600),
    # so the title bar buttons sit at CTRL offsets from (0,24):
    # max=(36+5, 24+4+5)=(41,33), close=(8+5,24+4+5)=(13,33).
    goto(s, 41, 33); time.sleep(0.3); click(s); time.sleep(2)
    print("RESTORE:", "HIT" if "TERMINAL RESTORE" in serial() else "MISS")

    # 4. CLOSE -> expect TERMINAL CLOSE (back at normal geometry)
    goto(s, 73, 49); time.sleep(0.3); click(s); time.sleep(2)
    print("CLOSE:", "HIT" if "TERMINAL CLOSE" in serial() else "MISS")

    log = serial()
    print("---- summary ----")
    for m in ["TERMINAL MIN", "TERMINAL MAX", "TERMINAL RESTORE", "TERMINAL CLOSE"]:
        print(f"{m}: {'HIT' if m in log else 'MISS'}")
finally:
    qemu.kill()
