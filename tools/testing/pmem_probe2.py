import glob, json, os, socket, struct, subprocess, time

ISO = max(glob.glob("/mnt/f/M4KK1/output/m4kk1_*.iso"), key=os.path.getmtime)
QMP = "/tmp/pm_qmp.sock"

subprocess.run(["rm", "-f", QMP], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:/tmp/pm_serial.log",
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print("booting...", flush=True)
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

def dump(base, length, path):
    qmp("dump-guest-memory", {
        "paging": False, "protocol": "file:" + path,
        "begin": base, "length": length}, 3)
    d = open(path, "rb").read()
    phoff = struct.unpack_from("<Q", d, 0x20)[0]
    for i in range(2):
        o = phoff + i * 0x38
        p_type = struct.unpack_from("<I", d, o)[0]
        p_offset = struct.unpack_from("<Q", d, o + 8)[0]
        p_vaddr = struct.unpack_from("<Q", d, o + 16)[0]
        p_filesz = struct.unpack_from("<Q", d, o + 32)[0]
        if p_type == 1:
            return d[p_offset:p_offset + p_filesz]
    return None

import re
regions = {
    "copland 0x600000": 0x600000,
    "shell 0x800000": 0x800000,
    "fm 0xD00000": 0xD00000,
    "cptest 0xE00000": 0xE00000,
    "sprach 0x1100000": 0x1100000,
}
for name, base in regions.items():
    seg = dump(base, 0x20000, "/tmp/pm_r.bin")
    if seg is None:
        print(name, "no seg")
        continue
    nz = sum(1 for b in seg if b)
    strs = [m.group().decode() for m in re.finditer(rb"[ -~]{10,}", seg[:0x8000])][:2]
    print("%s nonzero=%d/131072 samples=%s" % (name, nz, strs))
proc.kill()
