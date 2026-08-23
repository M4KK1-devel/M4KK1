import glob, json, os, socket, subprocess, sys, time

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

def qmp(execute, arguments=None, wait=3.0):
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

# dump-guest-memory (elf format, default): parse the ELF dump to
# extract the target range contents.
print(qmp("dump-guest-memory", {
    "paging": False, "protocol": "file:/tmp/pm_e0.dump",
    "begin": 0xE00000, "length": 0x30000}))
print(qmp("dump-guest-memory", {
    "paging": False, "protocol": "file:/tmp/pm_conn.dump",
    "begin": 0x720000, "length": 0x1000}))
proc.kill()

import struct

def load_elf_dump(path):
    """Return {paddr: bytes} from a QEMU elf guest dump."""
    d = open(path, "rb").read()
    segs = {}
    if d[:4] != b"\x7fELF":
        print(path, "not an ELF dump:", d[:8].hex())
        return segs
    e_phoff = struct.unpack_from("<I", d, 28)[0]
    e_phentsize = struct.unpack_from("<H", d, 42)[0]
    e_phnum = struct.unpack_from("<H", d, 44)[0]
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr = struct.unpack_from("<III", d, off)
        p_filesz = struct.unpack_from("<I", d, off + 16)[0]
        if p_type == 1:
            segs[p_vaddr] = d[p_offset:p_offset + p_filesz]
    return segs

for name, want in (("/tmp/pm_e0.dump", 0xE00000),
                   ("/tmp/pm_conn.dump", 0x720000)):
    if not os.path.exists(name):
        print(name, "MISSING")
        continue
    segs = load_elf_dump(name)
    for addr, data in segs.items():
        nz = sum(1 for b in data if b)
        print("%s seg 0x%X len %d nonzero %d head %s"
              % (name, addr, len(data), nz, data[:16].hex()))
