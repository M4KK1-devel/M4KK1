#!/usr/bin/env python3
"""Controls repro v4: keyboard forwarding into the terminal.
Spawn terminal, click its window body to focus (active=-1 via title
click?), then send keys via QMP and watch the serial log for shell
command echo (e.g. type 'echo HI' + newline; the m4sh child inside
terminal should print HI).
"""
import socket, subprocess, sys, time, json

ISO = "/tmp/mtM.iso"
SER = "/tmp/ctrl_run5.log"
PORT = 4455

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

def key(s, qcode, hold=0.12):
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": True, "key": qcode}}]}})
    time.sleep(hold)
    qmp(s, {"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": False, "key": qcode}}]}})

def type_str(s, text):
    for ch in text:
        if ch.isupper():
            key(s, "shift"); time.sleep(0.05)
            key(s, ch.lower()); time.sleep(0.05)
            # shift release already handled by key() down/up
        else:
            key(s, ch); time.sleep(0.06)

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

    # click terminal body to focus (center of window)
    goto(s, 200, 200); time.sleep(0.3); click(s)
    time.sleep(1)

    # type: echo HI
    type_str(s, "echo HI")
    time.sleep(0.3)
    key(s, "ret")
    time.sleep(3)

    log = serial()
    for m in ["terminal window registered", "HI"]:
        print(f"marker '{m}': {'HIT' if m in log else 'MISS'}")
    print("---- tail ----")
    for line in log.splitlines()[-12:]:
        print(line)
finally:
    qemu.kill()
