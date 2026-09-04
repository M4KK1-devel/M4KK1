#!/usr/bin/env python3
"""minimal: does a digit key reach pref's mailbox?"""
import subprocess, time, sys, os, socket, re

os.chdir("/mnt/f/M4KK1")
ISO = "output/m4kk1_0.0.1_build10-alpha1-full-test.iso"

sock = "/tmp/m4k_min.sock"; mon = "/tmp/m4k_min.mon"
for f in (sock, mon):
    if os.path.exists(f): os.unlink(f)
log = open("logs/min_probe_serial.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "unix:%s,server=on,wait=off" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
time.sleep(2)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock); s.setblocking(False)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon); m.setblocking(False)

buf = bytearray()
def mdrain(t=0.3):
    end = time.time() + t
    while time.time() < end:
        try: m.recv(65536)
        except BlockingIOError: pass
        time.sleep(0.05)
def hmp(cmd):
    m.sendall((cmd + "\n").encode()); mdrain(0.15)
def sdrain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d: buf.extend(d); log.write(d)
        except BlockingIOError: pass
        time.sleep(0.1)
    log.flush()
def send(line):
    s.sendall((line + "\n").encode()); time.sleep(0.3)
def wait_for(pat, timeout):
    import re as _re
    end = time.time() + timeout
    while time.time() < end:
        sdrain(0.5)
        clean = _re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", bytes(buf))
        if pat in clean: return True
    return False

def key(k):
    hmp("sendkey %s" % k); time.sleep(0.5)

# boot
if not wait_for(b"m4sh ~>", 90):
    print("BOOT FAIL"); qemu.kill(); sys.exit(1)
time.sleep(3)

# spawn pref, wait, then single digit with long settle
send("spawn /bin/pref")
wait_for(b"[PREF] surface ready", 25)
time.sleep(3)                      # let it settle to top
key("1")                           # wallpaper tab
time.sleep(1.5)
key("3")                           # theme 3
time.sleep(2)
key("q")                           # quit pref
time.sleep(2)

send("cat /export/cfg/pref/wallpaper.conf")
time.sleep(3)
sdrain(2)
mark = len(buf)
seg = bytes(buf)
# print the tail so we can see the cat output
tail = seg[-600:]
print(repr(tail.decode("utf-8", "replace")))
qemu.kill()
