import glob, json, os, socket, struct, subprocess, sys, time

ISO = sys.argv[1] if len(sys.argv) > 1 else \
    "/tmp/m4kk1_base/output/m4kk1_0.0.1_build4-alpha1-full.iso"
TAG = sys.argv[2] if len(sys.argv) > 2 else "base"
QMP = "/tmp/%s_qmp.sock" % TAG
LOG = "/tmp/%s_serial.log" % TAG

subprocess.run(["rm", "-f", QMP, LOG], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting", TAG, flush=True)
time.sleep(35)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())

def qmp(execute, arguments=None, wait=2.0):
    obj = {"execute": execute}
    if arguments:
        obj["arguments"] = arguments
    f.write(json.dumps(obj) + "\n"); f.flush()
    time.sleep(wait)
    while True:
        line = f.readline()
        if not line:
            return None
        r = json.loads(line)
        if "return" in r or "error" in r:
            return r

qmp("screendump", {"filename": "/tmp/%s_shot.ppm" % TAG})
proc.kill()

lines = open(LOG, errors="replace").read().splitlines()
print("serial tail:")
for l in lines[-8:]:
    print(" ", l)
d = open("/tmp/%s_shot.ppm" % TAG, "rb").read()
p = d.split(b"\n", 3)
w, h = map(int, p[1].split())
pix = p[3]
def px(x, y):
    o = (y * w + x) * 3
    return pix[o:o+3].hex()
print("screen probes: (0,0)", px(0,0), "(400,60)", px(400,60),
      "(64,100)", px(64,100), "(320,100)", px(320,100))
