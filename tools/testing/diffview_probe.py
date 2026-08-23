import json, os, socket, struct, subprocess, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full-test.iso"
QMP = "/tmp/dv_qmp.sock"
LOG = "/tmp/dv_serial.log"

def dump_at(f, tag, addr, length=0x1000):
    f.write(json.dumps({"execute": "dump-guest-memory", "arguments": {
        "paging": False, "protocol": "file:/tmp/dv_%s.dump" % tag,
        "begin": addr, "length": length}}) + "\n")
    f.flush()
    while True:
        line = f.readline()
        if not line:
            return None
        r = json.loads(line)
        if "return" in r or "error" in r:
            return r

def seg_data(path):
    d = open(path, "rb").read()
    phoff = struct.unpack_from("<Q", d, 0x20)[0]
    for i in range(4):
        o = phoff + i * 0x38
        if o + 56 > len(d):
            break
        t = struct.unpack_from("<I", d, o)[0]
        off = struct.unpack_from("<Q", d, o + 8)[0]
        va = struct.unpack_from("<Q", d, o + 16)[0]
        if t == 1:
            return d[off:off + 0x1000]
    return None

subprocess.run(["rm", "-f", QMP, LOG, "/tmp/dv_*.dump"], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
time.sleep(20)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())

dump_at(f, "kern_t1", 0x100000)
dump_at(f, "mdm_t1", 0x807000)
dump_at(f, "cop_t1", 0x600000)
print("t=20s dumps done", flush=True)
time.sleep(25)
dump_at(f, "mdm_t2", 0x807000)
dump_at(f, "cop_t2", 0x600000)
proc.kill()

k = seg_data("/tmp/dv_kern_t1.dump")
if k:
    print("kernel @0x100000 head:", k[:16].hex())
    idx = k.find(b"BUDDY")
    print("kernel contains 'BUDDY':", idx)

for tag in ("mdm", "cop"):
    a = seg_data("/tmp/dv_%s_t1.dump" % tag)
    b = seg_data("/tmp/dv_%s_t2.dump" % tag)
    if a and b:
        same = sum(1 for x, y in zip(a, b) if x == y)
        print("%s: t1 head %s | t2 head %s | same %d/4096" %
              (tag, a[:8].hex(), b[:8].hex(), same))
        if tag == "mdm":
            v1 = struct.unpack_from("<I", a, 0x104)[0]
            v2 = struct.unpack_from("<I", b, 0x104)[0]
            print("milestone t1=0x%08X t2=0x%08X" % (v1, v2))
