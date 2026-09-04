#!/usr/bin/env python3
"""minimal single-key test: ONE '1' keypress into pref, slowly."""
import subprocess, time, sys, os, select, re, socket

os.chdir("/mnt/f/M4KK1")
ISOS = [f for f in os.listdir("output") if f.endswith("full-test.iso")]
ISO = "output/" + max(ISOS, key=lambda f: os.path.getmtime("output/" + f))

mon = "/tmp/m4k_mk2.mon"
if os.path.exists(mon): os.unlink(mon)
qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", ISO, "-m", "512",
    "-vga", "std", "-serial", "stdio",
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-net", "none", "-no-reboot"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL)
for _ in range(30):
    if os.path.exists(mon): break
    time.sleep(0.3)
m = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
m.connect(mon); m.setblocking(False)

buf = bytearray()
log = open("logs/min2_serial.log", "wb")
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
            try:
                d = os.read(qemu.stdout.fileno(), 4096)
            except OSError: break
            if not d: break
            buf.extend(d); log.write(d)
    log.flush()
def clean():
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", bytes(buf))
def send(line):
    qemu.stdin.write((line + "\n").encode()); qemu.stdin.flush()
def wait_for(pat, timeout):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if pat in clean(): return True
    return False

if not wait_for(b"m4sh ~>", 90):
    print("BOOT FAIL"); qemu.kill(); sys.exit(1)
print("prompt ok", flush=True)
time.sleep(3)

send("spawn /bin/pref")
print("pref ready:", wait_for(b"[PREF] surface ready", 25), flush=True)
time.sleep(5)                      # generous settle

print("sending single '1' ...", flush=True)
hmp("sendkey 1")
time.sleep(3)
pump(2)

print("sending single '3' ...", flush=True)
hmp("sendkey 3")
time.sleep(3)
pump(2)

hmp("sendkey q")
time.sleep(2)

mark = len(buf)
send("cat /export/cfg/pref/wallpaper.conf")
time.sleep(4)
pump(2)
seg = clean()[mark:]
i = seg.find(b"wallpaper.conf")
print("cat output:", repr(seg[i:i+40].decode("utf-8","replace")) if i>=0 else "not-shown")
qemu.kill()
