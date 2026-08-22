import glob, json, os, socket, struct, subprocess, sys, time

ISO = sys.argv[1] if len(sys.argv) > 1 else \
    "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full-test.iso"
QMP = "/tmp/md_qmp.sock"
LOG = "/tmp/md_serial.log"

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

def qmp(name, args, wait=4):
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

# ELF dumps of the fixed user-ELF addresses
for tag, addr in (("copland", 0x600000), ("sprach", 0x1100000),
                  ("cptest", 0xE00000), ("conn", 0x720000),
                  ("mbox", 0x7F0000)):
    r = qmp("dump-guest-memory", {
        "paging": False, "protocol": "file:/tmp/md_%s.dump" % tag,
        "begin": addr, "length": 0x2000}, 3)
    print(tag, r if r and "error" in r else "dumped")

proc.kill()

def seg_data(path):
    d = open(path, "rb").read()
    phoff = struct.unpack_from("<Q", d, 0x20)[0]
    for i in range(4):
        o = phoff + i * 0x38
        if o + 56 > len(d):
            break
        p_type = struct.unpack_from("<I", d, o)[0]
        p_offset = struct.unpack_from("<Q", d, o + 8)[0]
        p_vaddr = struct.unpack_from("<Q", d, o + 16)[0]
        p_filesz = struct.unpack_from("<Q", d, o + 32)[0]
        if p_type == 1:
            seg = d[p_offset:p_offset + min(p_filesz, 0x1000)]
            nz = sum(1 for b in seg if b)
            return p_vaddr, nz, seg[:64].hex()
    return None, 0, ""

for tag in ("copland", "sprach", "cptest", "conn", "mbox"):
    p = "/tmp/md_%s.dump" % tag
    if os.path.exists(p):
        va, nz, head = seg_data(p)
        print("%s @%s nonzero=%d head=%s" % (tag, hex(va) if va else "?", nz, head[:40]))

# Mailbox raw words
p = "/tmp/md_mbox.dump"
if os.path.exists(p):
    d = open(p, "rb").read()
    phoff = struct.unpack_from("<Q", d, 0x20)[0]
    off = struct.unpack_from("<Q", d, phoff + 8)[0]
    words = struct.unpack_from("<16I", d, off)
    print("mailbox words:", [hex(w) for w in words[:8]])
