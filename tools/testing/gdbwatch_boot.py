import os, subprocess, sys, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full-test.iso"
LOG = "/tmp/gw_serial.log"

proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-S", "-gdb", "tcp::1234", "-gdb-stdio"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("QEMU started, waiting for gdb attach", flush=True)
time.sleep(120)
proc.kill()
