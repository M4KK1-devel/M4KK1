import json, os, socket, struct, subprocess, time

ISO = "/tmp/mt4.iso"
QMP = "/tmp/fb_qmp.sock"
LOG = "/tmp/fb_serial.log"
DUMP = "/tmp/fb_screen.dump"

subprocess.run(["rm", "-f", QMP, LOG, DUMP], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:" + QMP + ",server=on,wait=off"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def qmp(sock, cmd):
    sock.send((json.dumps(cmd) + "\n").encode())
    buf = ""
    while True:
        line = sock.recv(65536).decode(errors="replace")
        buf += line
        if "return" in buf or "error" in buf:
            try:
                return json.loads(buf.strip().splitlines()[-1])
            except Exception:
                return buf

# VESA linear framebuffer for vga=std at 800x600x32 is at 0xFD000000
FB = 0xFD000000
SZ = 800 * 600 * 4

time.sleep(35)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
qmp(s, {"execute": "qmp_capabilities"})
r = qmp(s, {"execute": "dump-guest-memory",
            "arguments": {"paging": False, "protocol": "file:" + DUMP,
                          "begin": FB, "length": SZ}})
print("dump:", r)
proc.terminate()
time.sleep(1)

# parse ELF dump -> first PT_LOAD
with open(DUMP, "rb") as f:
    d = f.read()
phoff = struct.unpack_from("<Q", d, 0x20)[0]
phentsize = struct.unpack_from("<H", d, 0x36)[0]
phnum = struct.unpack_from("<H", d, 0x38)[0]
seg = None
for i in range(phnum):
    off = phoff + i * phentsize
    p_type = struct.unpack_from("<I", d, off)[0]
    p_offset = struct.unpack_from("<Q", d, off + 8)[0]
    p_vaddr = struct.unpack_from("<Q", d, off + 16)[0]
    p_filesz = struct.unpack_from("<Q", d, off + 32)[0]
    if p_type == 1 and p_filesz > 1024:
        seg = (p_offset, p_vaddr, p_filesz)
        break
print("PT_LOAD offset=0x%x vaddr=0x%x size=0x%x" % seg)

pixels = d[seg[0]:seg[0] + SZ]
probes = [(10, 10), (400, 300), (400, 24), (100, 590), (790, 590)]
for (x, y) in probes:
    px = struct.unpack_from("<I", pixels, (y * 800 + x) * 4)[0]
    print("px(%d,%d) = 0x%08X" % (x, y, px))

# count distinct colors in a coarse grid
colors = set()
for yy in range(0, 600, 6):
    for xx in range(0, 800, 6):
        colors.add(struct.unpack_from("<I", pixels, (yy * 800 + xx) * 4)[0])
print("distinct colors (coarse grid):", len(colors))
