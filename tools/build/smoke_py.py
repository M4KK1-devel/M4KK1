#!/usr/bin/env python3
"""Boot M4KK1 in QEMU, inject commands over the serial unix socket."""
import os, socket, subprocess, sys, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if f.endswith("full-test.iso")],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_net.sock"
if os.path.exists(sock):
    os.unlink(sock)
log_path = "logs/smoke_py.log"
log = open(log_path, "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

time.sleep(3)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sock)
s.setblocking(False)

def drain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            data = s.recv(65536)
            if data:
                log.write(data)
        except BlockingIOError:
            pass
        time.sleep(0.1)

def send(line):
    s.sendall((line + "\r").encode())

drain(30)          # boot
send("ifconfig")
drain(6)
send("ping 10.0.2.2")
drain(15)
send("ls /")
drain(6)
qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()

text = open(log_path, "rb").read().decode("utf-8", "replace")
for kw in ["e1000", "Network up", "eth0", "PING", "reply", "inet "]:
    for line in text.splitlines():
        if kw in line:
            print("[%s] %s" % (kw, line.strip()[:120]))
print("--- tail ---")
print("\n".join(text.splitlines()[-25:]))
