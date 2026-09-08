#!/usr/bin/env python3
"""Minimal: does sendkey reach the sprach key loop after FM is up?
Watch for ANY [SPRACH] line after each sendkey."""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISO = "output/" + max((f for f in os.listdir("output") if f.endswith("full-test.iso")),
                      key=lambda f: os.path.getmtime("output/" + f))
mon = "/tmp/m4k_fmnav2.mon"
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
pump(3)
buf.clear()
hmp("sendkey meta_l-e")
wait_for(b"[FM] surface ready", 30)
pump(2)
buf.clear()

# single 'j' key: fm moves selection down; nothing on serial though.
# Better: send ctrl-l then '/' and see if ANY line appears.
print("--- sendkey ctrl-l ---", flush=True)
hmp("sendkey ctrl-l")
out = pump(2)
print(repr(out[-300:]), flush=True)

print("--- sendkey slash ---", flush=True)
hmp("sendkey slash")
out = pump(2)
print(repr(out[-300:]), flush=True)

print("--- sendkey alt-left ---", flush=True)
buf.clear()
hmp("sendkey alt-left")
out = pump(3)
print(repr(out[-400:]), flush=True)

qemu.kill()
