#!/usr/bin/env python3
"""Controls repro v2: window-level controls.
Spawn terminal via far-right dock launcher, wait for window, then:
  - click dock terminal icon (focus/raise)
  - click window close button (title bar left, red)
  - verify via serial markers.
"""
import socket, subprocess, sys, time, json

ISO = "/tmp/mtM.iso"
SER = "/tmp/ctrl_run2.log"
PORT = 4452

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

    # far-right launcher icon: x = 800-32-8=760..792, y 572..604
    goto(s, 776, 588); time.sleep(0.3); click(s)
    print("clicked far-right launcher")
    # wait for terminal registration
    spawned = False
    for _ in range(60):
        time.sleep(1)
        try:
            with open(SER, "rb") as f:
                t = f.read().decode(errors="replace")
            if "terminal" in t and ("registered" in t or "spawned" in t or "TERM" in t):
                spawned = True; break
        except FileNotFoundError:
            continue
    print("terminal spawn marker:", spawned)
    time.sleep(8)

    with open(SER, "rb") as f:
        log = f.read().decode(errors="replace")
    print("---- tail ----")
    for line in log.splitlines()[-30:]:
        print(line)
finally:
    qemu.kill()
