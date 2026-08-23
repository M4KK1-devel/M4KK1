#!/usr/bin/env python3
"""Reproduce the WM restart loop with injected mouse movement via QMP."""
import json, socket, subprocess, sys, time, os

ISO = "/tmp/mtJ.iso"
SER = "/tmp/ms24_run.log"
QMP = "/tmp/qmp24.sock"
OUT = "/tmp/mouse_repro.log"

if os.path.exists(QMP):
    os.unlink(QMP)

# boot QEMU with a display-less machine but QMP control
qemu = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-vga", "std", "-display", "none",
     "-serial", "file:" + SER,
     "-qmp", "tcp:127.0.0.1:4444,server,nowait"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# wait for QMP socket
time.sleep(2)

s = socket.create_connection(("127.0.0.1", 4444))
f = s.makefile("rw")

def qmp(cmd, **args):
    f.write(json.dumps({"execute": cmd, "arguments": args}) + "\n")
    f.flush()
    while True:
        line = f.readline()
        if not line:
            return None
        msg = json.loads(line)
        if "return" in msg or "error" in msg:
            return msg

qmp("qmp_capabilities")
print("QMP up, booting 30s...", file=sys.stderr)
time.sleep(30)

# inject continuous mouse movement for 40 seconds
t0 = time.time()
n = 0
while time.time() - t0 < 40:
    qmp("mouse_move", dx=8, dy=-5)
    qmp("mouse_move", dx=-7, dy=6)
    n += 2
    time.sleep(0.02)
print("sent %d mouse_move events" % n, file=sys.stderr)

time.sleep(20)   # observe
qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()

# analyze
log = open(SER, errors="replace").read()
stalls = log.count("heartbeat stalled")
starts = log.count("Window manager starting")
print("mouse_move events: %d" % n)
print("WM starts: %d" % starts)
print("heartbeat stalls: %d" % stalls)
tail = [l for l in log.splitlines() if l.strip()][-8:]
print("---- serial tail ----")
for l in tail:
    print(l)
