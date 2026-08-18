#!/usr/bin/env python3
"""Catch the exact write that zeroes back_buffer (0x1234de0) via gdbstub watchpoint."""
import glob, os, socket, subprocess, sys, time

ISO = sys.argv[1] if len(sys.argv) > 1 else max(
    glob.glob("output/m4kk1_*.iso"), key=os.path.getmtime)
KRN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "output", "m4kk1.krn")
MON = "/tmp/m4k_watch_mon.sock"
SER = "/tmp/m4k_watch_serial.log"
GDB = "/tmp/m4k_watch.gdb"

for f in (MON, SER, GDB):
    if os.path.exists(f):
        os.unlink(f)

subprocess.run(["pkill", "-f", "qemu-system-i38[6]"], capture_output=True)
time.sleep(1)

qemu = [
    "qemu-system-i386",
    "-cdrom", ISO, "-m", "512", "-vga", "std",
    "-display", "none",
    "-serial", "file:" + SER,
    "-monitor", "unix:%s,server=on,nowait" % MON,
    "-gdb", "tcp::1234",
    "-daemonize",
]
print("booting", ISO, flush=True)
subprocess.run(qemu, check=True)

with open(GDB, "w") as f:
    f.write("""set pagination off
set architecture i386
file %s
target remote 127.0.0.1:1234
watch *0x1234de0
continue
printf "\\n=== STOP 1 ===\\n"
info registers eip cs eax ebx ecx edx esi edi
x/14i $eip-14
printf "val 0x1234de0 = 0x%%x  val 0x1234de4 = 0x%%x\\n", *(unsigned int*)0x1234de0, *(unsigned int*)0x1234de4
continue
printf "\\n=== STOP 2 ===\\n"
info registers eip cs eax ebx ecx edx esi edi
x/14i $eip-14
bt
x/12wx $esp
printf "val 0x1234de0 = 0x%%x  val 0x1234de4 = 0x%%x\\n", *(unsigned int*)0x1234de0, *(unsigned int*)0x1234de4
quit
""" % KRN)

for attempt in range(8):
    try:
        r = subprocess.run(["gdb", "-batch", "-x", GDB], capture_output=True, text=True, timeout=200)
        if "target remote" in r.stderr or "remote" in r.stdout or "STOP" in r.stdout:
            print(r.stdout)
            if r.stderr.strip():
                print("STDERR:", r.stderr[:2000])
            break
    except subprocess.TimeoutExpired:
        print("gdb timed out - watch never fired")
        break
    time.sleep(1.5)

print("=== serial tail ===")
try:
    with open(SER, "rb") as f:
        data = f.read()
    print(data[-3000:].decode("utf-8", "replace"))
except FileNotFoundError:
    print("no serial log")

subprocess.run(["pkill", "-f", "qemu-system-i38[6]"], capture_output=True)
