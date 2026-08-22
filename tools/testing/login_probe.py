import glob, json, os, socket, struct, subprocess, sys, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
QMP = "/tmp/lg_qmp.sock"
LOG = "/tmp/lg_serial.log"
SHOT = "/tmp/lg_shot.ppm"

KEYMAP = {c: (c if c.isalnum() else None) for c in
          "abcdefghijklmnopqrstuvwxyz0123456789"}

def sendkey(f, name, hold_shift=False):
    obj = {"execute": "sendkey", "arguments": {"keys": [name]}}
    f.write(json.dumps(obj) + "\n"); f.flush()
    time.sleep(0.12)
    while True:
        line = f.readline()
        if not line:
            return
        r = json.loads(line)
        if "return" in r or "error" in r:
            return

def type_str(f, s):
    for ch in s:
        if ch.isalnum() or ch in "-_=.@":
            sendkey(f, ch)
        elif ch == "\n":
            sendkey(f, "ret")
        else:
            sendkey(f, "spc")

subprocess.run(["rm", "-f", QMP, LOG, SHOT], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
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

print("typing root...", flush=True)
type_str(f, "root\n")
time.sleep(1)
print("typing 123456...", flush=True)
type_str(f, "123456\n")
time.sleep(12)

f.write(json.dumps({"execute": "screendump",
                    "arguments": {"filename": SHOT}}) + "\n")
f.flush()
time.sleep(2)
proc.kill()

d = open(SHOT, "rb").read()
p = d.split(b"\n", 3)
w, h = map(int, p[1].split())
pix = p[3]
def px(x, y):
    o = (y * w + x) * 3
    return pix[o:o+3].hex()
print("probes: (0,0)", px(0,0), "(400,60)", px(400,60),
      "(64,100)", px(64,100), "(320,100)", px(320,100),
      "(620,290)", px(620,290))
lines = open(LOG, errors="replace").read().splitlines()
print("serial tail:")
for l in lines[-6:]:
    print(" ", l)
