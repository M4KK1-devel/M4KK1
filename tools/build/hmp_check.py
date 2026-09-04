#!/usr/bin/env python3
"""verify HMP sendkey reaches the guest: boot, wait prompt, send an
'A' via sendkey, and check the shell echo line contains 'A'."""
import subprocess, time, sys, os, socket, re

os.chdir("/mnt/f/M4KK1")
ISO = "output/m4kk1_0.0.1_build10-alpha1-full-test.iso"
sock = "/tmp/m4k_hmp.sock"; mon = "/tmp/m4k_hmp.mon"
for f in (sock, mon):
    if os.path.exists(f): os.unlink(f)

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "unix:%s,server=on,wait=off" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdout=subprocess.DEVNULL, stderr=None)
time.sleep(3)
for _ in range(20):
    if os.path.exists(sock) and os.path.exists(mon):
        break
    time.sleep(0.5)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock); s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon); m.setblocking(False)
buf = bytearray()

def drain(t):
    end = time.time() + t
    while time.time() < end:
        for sk in (s, m):
            try:
                d = sk.recv(65536)
                if sk is s and d: buf.extend(d)
            except BlockingIOError:
                pass
        time.sleep(0.1)

def hmp(cmd):
    m.sendall((cmd + "\n").encode())
    drain(0.3)

def clean():
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", bytes(buf))

# boot to prompt
end = time.time() + 90
while time.time() < end:
    drain(0.5)
    if b"m4sh ~>" in clean(): break
print("prompt:", b"m4sh ~>" in clean())
time.sleep(3)
mark = len(buf)
hmp("sendkey a")
time.sleep(1)
hmp("sendkey shift-a")
time.sleep(1)
hmp("sendkey ret")
time.sleep(2)
drain(2)
seg = clean()[mark:]
print("after keys:", repr(seg[-300:]))
qemu.kill()
