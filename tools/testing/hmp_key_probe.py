import os, socket, subprocess, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
MON = "/tmp/hmp_mon.sock"
INT = "/tmp/hmp_int.log"

subprocess.run(["rm", "-f", MON, INT], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:/dev/null",
     "-monitor", "unix:%s,server=on,nowait" % MON,
     "-d", "int", "-D", INT],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
time.sleep(30)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(MON)
s.settimeout(3)
try:
    s.recv(4096)
except OSError:
    pass
s.sendall(b"sendkey a\n")
time.sleep(2)
s.sendall(b"sendkey b\n")
time.sleep(2)
proc.kill()

d = open(INT, errors="replace").read()
print("total:", len(d))
print("v=21 count in tail:", d[-30000:].count("v=21"))
print("last vecs:", d[-2000:].count("v="))
