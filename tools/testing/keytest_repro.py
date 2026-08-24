#!/usr/bin/env python3
"""Key delivery test: boot desktop, send Ctrl+Alt+T via QMP key events,
then plain letters. Ctrl+Alt+T must produce a serial log line if keys
reach the kernel keyboard buffer. Also test 'esc' and a letter with
no modifiers as control."""
import socket, subprocess, time, json, sys

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build5-alpha1-full-test.iso"
SER = "/tmp/keytest_loop1.log"
PORT = 4474

qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-serial", f"file:{SER}",
     "-display", "none", "-qmp", f"tcp:127.0.0.1:{PORT},server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    time.sleep(3)
    s = socket.create_connection(("127.0.0.1", PORT), timeout=10)
    s.settimeout(5)
    def qmp(obj):
        s.sendall((json.dumps(obj) + "\r\n").encode())
        time.sleep(0.05)
        try:
            while True:
                l = s.recv(65536)
                if not l: break
        except socket.timeout:
            pass
    qmp({"execute": "qmp_capabilities"})

    # wait for desktop
    for _ in range(60):
        time.sleep(2)
        try:
            t = open(SER, "rb").read().decode(errors="replace")
            if "mode 'stacking' initialized" in t:
                break
        except FileNotFoundError:
            continue
    print("desktop ready:", "mode 'stacking' initialized" in t)
    time.sleep(2)

    def key(qcode, hold=0.1, mods=()):
        evs = [{"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": m}}} for m in mods]
        evs.append({"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": qcode}}})
        qmp({"execute": "input-send-event", "arguments": {"events": evs}})
        time.sleep(hold)
        evs = [{"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": qcode}}}]
        evs += [{"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": m}}} for m in mods]
        qmp({"execute": "input-send-event", "arguments": {"events": evs}})
        time.sleep(0.05)

    # 1. Ctrl+Alt+T — sprach logs this on serial
    key("t", mods=("ctrl", "alt"))
    time.sleep(4)
    t = open(SER, "rb").read().decode(errors="replace")
    print("ctrl-alt-T logged:", "Ctrl+Alt+T" in t)

    # 2. letters into the (now focused) terminal
    # NOTE: QMP qcodes are key NAMES, not characters — " " is not a
    # valid qcode and would be silently rejected (the "missing space"
    # red herring).  Map the few specials explicitly.
    QCODE = {" ": "spc", "\n": "ret", "\t": "tab", "\b": "backsp"}
    for ch in "echo hi":
        key(QCODE.get(ch, ch))
    key("ret")
    time.sleep(4)

    # 3. dump screen for evidence
    qmp({"execute": "screendump", "arguments": {"filename": "/tmp/keytest.ppm"}})
    time.sleep(2)
    print("---- serial tail ----")
    print(t[-1200:])
finally:
    qemu.kill()
