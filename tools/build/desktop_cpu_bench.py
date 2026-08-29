#!/usr/bin/env python3
"""Desktop idle CPU benchmark.

Boots an ISO headless, waits for the desktop, then samples the QEMU
host process CPU time (/proc/<pid>/stat utime+stime) over two windows.
Guest-side wasted repaints (the perf target) show up directly as host
CPU burn under TCG. Usage: desktop_cpu_bench.py <iso> <label>
"""
import os, socket, subprocess, sys, time

iso, label = (sys.argv[1:3] + ["", ""])[:2]
if not iso or not label:
    print("usage: desktop_cpu_bench.py <iso> <label>")
    raise SystemExit(2)
os.chdir("/mnt/f/M4KK1")
sock, mon = "/tmp/m4k_bench.sock", "/tmp/m4k_bench.mon"
for f in (sock, mon):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/cpu_bench_%s.log" % label, "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-monitor", "unix:%s,server=on,wait=off" % mon,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

# serial via socket (non-blocking read, just drain)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
time.sleep(2)
s.connect(sock)
s.setblocking(False)

def sdrain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d:
                log.write(d)
        except BlockingIOError:
            pass
        time.sleep(0.1)

sdrain(45)   # boot to idle desktop

def cpu_ticks():
    with open("/proc/%d/stat" % qemu.pid) as f:
        parts = f.read().split()
    return (int(parts[13]) + int(parts[14])) / os.sysconf("SC_CLK_TCK")

CLK = os.sysconf("SC_CLK_TCK")
results = []
for w in (12, 12):
    t0, c0 = time.time(), cpu_ticks()
    time.sleep(w)
    t1, c1 = time.time(), cpu_ticks()
    pct = (c1 - c0) / (t1 - t0) * 100
    results.append(pct)
    print("%s window %ds: %.1f%% host CPU" % (label, w, pct))
    sdrain(1)

qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()
print("%s RESULT: avg %.1f%% host CPU over idle desktop" %
      (label, sum(results) / len(results)))
