#!/usr/bin/env python3
"""Diag3: after fm boots, drive Ctrl+L + /export + ret, then screendump
the framebuffer to check the address bar actually entered edit mode,
and dump every serial byte in a tight window around each key."""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISO = "output/" + max((f for f in os.listdir("output") if f.endswith("full-test.iso")),
                      key=lambda f: os.path.getmtime("output/" + f))
mon = "/tmp/m4k_fmnav4.mon"
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
log = open("/tmp/fmnav_diag3.log", "wb")

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
            buf.extend(d); log.write(d)
    log.flush()
    return bytes(buf)

def wait_for(pat, timeout):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pat in bytes(buf): return True
    return False

wait_for(b"[SPRACH] Entering main loop", 150)
pump(8)   # let cptest finish and the desktop settle
buf.clear()
hmp("sendkey meta_l-e")
wait_for(b"[FM] surface ready", 30)
pump(3)
buf.clear()

def keys(s, dt=0.10):
    for ch in s:
        name = {" ": "spc", "/": "slash"}.get(ch, ch)
        hmp("sendkey %s" % name)
        time.sleep(dt)

# Ctrl+L then type /export then Enter
hmp("sendkey ctrl-l")
time.sleep(0.3)
keys("/export")
time.sleep(0.3)
hmp("sendkey ret")
time.sleep(1.0)
out = pump(1)
print("after addr input:", repr(out[-200:]), flush=True)

# Alt+Left
buf.clear()
hmp("sendkey alt-left")
out = pump(2)
print("after alt-left:", repr(out[-200:]), flush=True)

# screendump for visual check
hmp("screendump /tmp/fmnav_diag3.ppm")
time.sleep(1.0)
qemu.kill()
print("done", flush=True)
