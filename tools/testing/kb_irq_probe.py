import glob, json, os, socket, subprocess, sys, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
QMP = "/tmp/kb_qmp.sock"
INT = "/tmp/kb_int.log"

subprocess.run(["rm", "-f", QMP, INT], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:/dev/null",
     "-d", "int", "-D", INT,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
time.sleep(30)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())

n0 = os.path.getsize(INT)
print("int log before keys:", n0, flush=True)
f.write(json.dumps({"execute": "sendkey", "arguments":
                    {"keys": ["a"]}}) + "\n"); f.flush()
time.sleep(2)
f.write(json.dumps({"execute": "sendkey", "arguments":
                    {"keys": ["b"]}}) + "\n"); f.flush()
time.sleep(2)
proc.kill()

n1 = os.path.getsize(INT)
print("int log after keys:", n1)
# scan the tail for v=21 (irq1 keyboard) / v=74 etc.
d = open(INT, errors="replace").read()
tail = d[-20000:]
for vec in ("v=21 ", "v=74 ", "v=20 "):
    print(vec, tail.count(vec))
