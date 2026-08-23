#!/usr/bin/env python3
"""Controls repro: click menubar brand (app menu), menubar clock,
dock launchpad icon, dock window icons — then check serial log for
the corresponding [SPRACH] click markers.

Coordinates follow sprach.c hit-tests:
  brand text: x 6..52, y 0..24        -> app menu toggle
  clock area: x >= 800-8*7-12=732     -> clock popup
  launchpad dock icon: x 8..40, y 572..604 (TASKBAR y=552, icon_y=8)
  dock window icons: x 52..84, 96..128, ...
"""
import socket, subprocess, sys, time, json

ISO = "/tmp/mtM.iso"
SER = "/tmp/ctrl_run1.log"
PORT = 4451

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
    # park top-left with big negative chunks (clamped at 0)
    for _ in range(12):
        move(s, -100, -100)
        time.sleep(0.02)
    # step positive in <=100 chunks
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
    # wait for QMP + desktop ready
    s = None
    for _ in range(60):
        time.sleep(1)
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
            break
        except OSError:
            continue
    if s is None:
        print("FATAL: no QMP"); sys.exit(1)
    time.sleep(1.0)
    s.settimeout(2.0)
    try: s.recv(65536)
    except Exception: pass
    qmp(s, {"execute": "qmp_capabilities"})

    # wait for desktop (sprach banner) up to 150s
    ready = False
    for _ in range(150):
        time.sleep(1)
        try:
            with open(SER, "rb") as f:
                t = f.read().decode(errors="replace")
            if "mode 'stacking' initialized" in t:
                ready = True
                break
        except FileNotFoundError:
            continue
    print("desktop ready:", ready)
    time.sleep(5)

    results = []
    # 1. brand text -> app menu
    goto(s, 30, 12); time.sleep(0.3); click(s)
    time.sleep(2)
    # 2. close menu (click brand again)
    click(s)
    time.sleep(1)
    # 3. clock area
    goto(s, 760, 12); time.sleep(0.3); click(s)
    time.sleep(2)
    # 4. close clock
    click(s)
    time.sleep(1)
    # 5. launchpad via dock
    goto(s, 24, 588); time.sleep(0.3); click(s)
    time.sleep(3)
    # 6. close launchpad via same icon
    click(s)
    time.sleep(2)

    with open(SER, "rb") as f:
        log = f.read().decode(errors="replace")
    for marker, name in [
        ("app menu open", "brand->app menu"),
        ("app menu closed", "menu close"),
        ("clock popup open", "clock toggle"),
        ("clock popup closed", "clock close"),
        ("launchpad open", "dock->launchpad"),
        ("launchpad closed", "launchpad close"),
    ]:
        print(f"{name}: {'HIT' if marker in log else 'MISS'}")
    print("---- tail ----")
    for line in log.splitlines()[-25:]:
        print(line)
finally:
    qemu.kill()
