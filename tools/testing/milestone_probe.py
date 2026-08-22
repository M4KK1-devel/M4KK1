import json, os, socket, struct, subprocess, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full-test.iso"
QMP = "/tmp/ms_qmp.sock"
LOG = "/tmp/ms_serial.log"
MILE = 0x807104

subprocess.run(["rm", "-f", QMP, LOG], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
time.sleep(45)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())

def qmp(name, args, wait=3):
    f.write(json.dumps({"execute": name, "arguments": args}) + "\n")
    f.flush()
    time.sleep(wait)
    while True:
        line = f.readline()
        if not line:
            return None
        r = json.loads(line)
        if "return" in r or "error" in r:
            return r

# milestone var + copland window
r1 = qmp("dump-guest-memory", {
    "paging": False, "protocol": "file:/tmp/ms_mile.dump",
    "begin": MILE & ~0xFFF, "length": 0x1000})
r2 = qmp("dump-guest-memory", {
    "paging": False, "protocol": "file:/tmp/ms_cop.dump",
    "begin": 0x600000, "length": 0x8000})
proc.kill()

def seg(path, want_va=None):
    d = open(path, "rb").read()
    phoff = struct.unpack_from("<Q", d, 0x20)[0]
    for i in range(4):
        o = phoff + i * 0x38
        if o + 56 > len(d):
            break
        t = struct.unpack_from("<I", d, o)[0]
        off = struct.unpack_from("<Q", d, o + 8)[0]
        va = struct.unpack_from("<Q", d, o + 16)[0]
        if t == 1 and (want_va is None or va == want_va):
            return d, off, va
    return d, None, None

if os.path.exists("/tmp/ms_mile.dump"):
    d, off, va = seg("/tmp/ms_mile.dump")
    wo = off + (MILE - va)
    v = struct.unpack_from("<I", d, wo)[0]
    print("milestone @0x%X = 0x%08X" % (MILE, v))
    print(hex(v & 0xFF), "->",
          {0: "init value", 0xA0: "session loop", 0xA1: "pre-draw",
           1: "form drawn", 2: "autologin start", 3: "auth OK",
           4: "auth FAIL", 5: "copland forked"}.get(v & 0xFF, "?"))

if os.path.exists("/tmp/ms_cop.dump"):
    d, off, va = seg("/tmp/ms_cop.dump")
    seg_data = d[off:off + 0x8000]
    nz = sum(1 for b in seg_data if b)
    print("copland @0x600000 nonzero:", nz)
