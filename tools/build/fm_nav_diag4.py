#!/usr/bin/env python3
"""Diag4: determine which sendkey spelling drives the FM address bar.
Try ctrl_l-l then type, screendump, check edit mode (white bg)."""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISO = "output/" + max((f for f in os.listdir("output") if f.endswith("full-test.iso")),
                      key=lambda f: os.path.getmtime("output/" + f))
mon = "/tmp/m4k_fmnav5.mon"
if os.path.exists(mon):
    os.unlink(mon)
qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512", "-vga", "std",
    "-serial", "stdio", "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
for _ in range(30):
    if os.path.exists(mon): break
    time.sleep(0.3)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon); m.setblocking(False)

buf = bytearray()
def mdrain(t=0.25):
    end = time.time() + t
    while time.time() < end:
        try: m.recv(65536)
        except BlockingIOError: pass
        time.sleep(0.05)

def hmp(cmd):
    m.sendall((cmd + "\n").encode()); mdrain(0.15)

def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        r, _, _ = select.select([qemu.stdout], [], [], 0.15)
        if r:
            try: d = os.read(qemu.stdout.fileno(), 4096)
            except OSError: break
            if not d: break
            buf.extend(d)
    return bytes(buf)

def wait_for(pat, timeout):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pat in bytes(buf): return True
    return False

wait_for(b"[SPRACH] Entering main loop", 150)
pump(8)
buf.clear()
hmp("sendkey meta_l-e")
wait_for(b"[FM] surface ready", 30)
pump(3)
buf.clear()

# Test A: ctrl_l spelling
hmp("sendkey ctrl_l-l")
time.sleep(0.5)
hmp("screendump /tmp/fmnav_a.ppm")
time.sleep(0.8)

# Esc out of edit mode if it engaged
hmp("sendkey esc")
time.sleep(0.5)

# Test B: ctrl spelling with long hold
hmp("sendkey ctrl-l")
time.sleep(0.5)
hmp("screendump /tmp/fmnav_b.ppm")
time.sleep(0.8)

qemu.kill()
print("done", flush=True)
