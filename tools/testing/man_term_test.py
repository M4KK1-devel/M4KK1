#!/usr/bin/env python3
"""Interactive man command test on the M4KK1 desktop.

Boots the full ISO (TEST_AUTOLOGIN), clicks the dock terminal
launcher via the HMP monitor, types `man ls`, screendumps, and
checks pixel/serial evidence.
"""
import os, socket, subprocess, sys, time

os.chdir("/mnt/f/M4KK1")
ISO = subprocess.run("ls output/m4kk1_*.iso | head -1",
                     shell=True, capture_output=True, text=True).stdout.strip()
if not ISO:
    print("NO ISO"); sys.exit(2)

SER = "/tmp/m4kk1_man_term.log"
MON = "/tmp/m4kk1_man_term.mon"
for f in (SER, MON):
    try: os.unlink(f)
    except OSError: pass

proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO, "-m", "512",
     "-display", "none", "-serial", "file:" + SER,
     "-monitor", "unix:%s,server=on,nowait" % MON],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

print("booting (28s)...", flush=True)
time.sleep(28)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(2)
ok = False
for _ in range(30):
    try:
        s.connect(MON)
        ok = True
        break
    except OSError:
        time.sleep(0.5)
if not ok:
    print("FAIL: monitor connect failed"); proc.kill(); sys.exit(1)

def cmd(line, wait=0.7):
    try:
        s.sendall(line.encode() + b"\n")
    except OSError:
        pass
    time.sleep(wait)
    try:
        return s.recv(65536).decode("utf-8", "replace")
    except socket.timeout:
        return ""

cmd("")  # banner

# 1. click dock terminal launcher (guest cursor 400,300 -> 776,576)
cmd("mouse_move %d %d" % ((776 - 400) // 2, (576 - 300) // 2), 1.5)
cmd("mouse_button 1", 0.4)
cmd("mouse_button 0", 2.5)
print("clicked launcher", flush=True)

# 2. type man ls
for ch in "man ls":
    cmd("sendkey %s" % ch, 0.3)
cmd("sendkey ret", 3.0)
print("typed man ls", flush=True)

# 3. screendump
cmd("screendump /tmp/m4kk1_man_page.ppm", 1.5)
subprocess.run(["cp", "/tmp/m4kk1_man_page.ppm", "output/man_page_shot.ppm"])

# 4. serial evidence
data = open(SER, "rb").read().decode("utf-8", "replace")
print("install line:", "man pages installed: 32" in data)
print("=== serial tail ===")
for ln in data.splitlines()[-15:]:
    print(ln)

proc.terminate()
try:
    proc.wait(timeout=10)
except subprocess.TimeoutExpired:
    proc.kill()
print("DONE")
