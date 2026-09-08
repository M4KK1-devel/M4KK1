#!/usr/bin/env python3
"""Diag2: check basic sendkey reaches the guest terminal echo."""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISO = "output/" + max((f for f in os.listdir("output") if f.endswith("full-test.iso")),
                      key=lambda f: os.path.getmtime("output/" + f))
mon = "/tmp/m4k_fmnav3.mon"
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
log = open("/tmp/fmnav_diag2.log", "wb")

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
pump(5)
buf.clear()

# 1. plain 'a' — autologin terminal should echo it (serial console)
hmp("sendkey a")
out = pump(2)
print("after 'a':", repr(out[-120:]), flush=True)

# 2. ctrl-l
buf.clear()
hmp("sendkey ctrl-l")
out = pump(2)
print("after ctrl-l:", repr(out[-120:]), flush=True)

# 3. alt-left with a wait after alt? try holding style: separate keys
buf.clear()
hmp("sendkey alt")
time.sleep(0.2)
hmp("sendkey left")
time.sleep(0.2)
hmp("sendkey alt")
out = pump(2)
print("after alt,left,alt:", repr(out[-200:]), flush=True)

qemu.kill()
