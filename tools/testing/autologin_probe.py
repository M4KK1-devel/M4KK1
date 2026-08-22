import glob, json, os, socket, struct, subprocess, sys, time

ISO = sys.argv[1] if len(sys.argv) > 1 else \
    "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full-test.iso"
QMP = "/tmp/al_qmp.sock"
LOG = "/tmp/al_serial.log"
SHOT = "/tmp/al_shot.ppm"

subprocess.run(["rm", "-f", QMP, LOG, SHOT], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting (autologin)...", flush=True)
time.sleep(45)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())
f.write(json.dumps({"execute": "screendump",
                    "arguments": {"filename": SHOT}}) + "\n"); f.flush()
time.sleep(2)
proc.kill()

lines = open(LOG, errors="replace").read().splitlines()
print("serial tail:")
for l in lines[-14:]:
    print(" ", l)

d = open(SHOT, "rb").read()
p = d.split(b"\n", 3)
w, h = map(int, p[1].split())
pix = p[3]
def px(x, y):
    o = (y * w + x) * 3
    return pix[o:o+3].hex()
print("probes: (0,0)", px(0,0), "(400,24)", px(400,24),
      "(64,100)", px(64,100), "(320,100)", px(320,100),
      "(620,290)", px(620,290), "(620,296)", px(620,296))
