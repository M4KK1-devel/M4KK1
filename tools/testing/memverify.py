import json, os, socket, struct, subprocess, time

ISO = "/tmp/mt2.iso"
QMP = "/tmp/mv_qmp.sock"
LOG = "/tmp/mv_serial.log"
DUMP = "/tmp/mv_copland.dump"
ELF = "/mnt/f/M4KK1/usr/bin/copland"

subprocess.run(["rm", "-f", QMP, LOG, DUMP], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:" + QMP + ",server=on,wait=off"],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def qmp(sock, cmd):
    sock.send((json.dumps(cmd) + "\n").encode())
    time.sleep(0.15)
    try:
        return sock.recv(65536)
    except Exception:
        return b""

# wait for boot to reach copland crash (EXC -> cli;hlt loop)
target = None
for i in range(60):
    time.sleep(1)
    try:
        with open(LOG, "rb") as f:
            data = f.read().decode("utf-8", "replace")
        if "EXC" in data and "COPLAND" in data:
            target = True
            break
    except FileNotFoundError:
        pass
print("boot reached EXC:", target)

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(QMP)
qmp(s, {"execute": "qmp_capabilities"})
qmp(s, {"execute": "dump-guest-memory",
        "arguments": {"paging": False, "protocol": "file:" + DUMP,
                      "begin": 0x600000, "length": 0x10000}})
time.sleep(1.5)
s.close()
proc.kill()

# parse dump: find PT_LOAD
with open(DUMP, "rb") as f:
    d = f.read()
phoff = struct.unpack_from("<Q", d, 0x20)[0]
phentsize = struct.unpack_from("<H", d, 0x36)[0]
phnum = struct.unpack_from("<H", d, 0x38)[0]
mem = b""
for i in range(phnum):
    off = phoff + i * phentsize
    p_type, p_flags = struct.unpack_from("<II", d, off)
    p_offset, p_vaddr, _p, p_filesz = struct.unpack_from("<QQQQ", d, off + 8)
    if p_type == 1:  # PT_LOAD
        mem += d[p_offset:p_offset + p_filesz]
        print("PT_LOAD vaddr=0x%X size=0x%X" % (p_vaddr, p_filesz))

with open(ELF, "rb") as f:
    elf = f.read()

# compare .text: file offset 0x1000 maps to vaddr 0x600000, filesz 0x6090
ftext = elf[0x1000:0x1000 + 0x6090]
mtext = mem[0:0x6090]
diff = sum(1 for a, b in zip(ftext, mtext) if a != b)
print(".text bytes compared=%d mismatched=%d" % (len(ftext), diff))
for i in range(0, min(len(ftext), len(mtext))):
    if ftext[i] != mtext[i]:
        print("first mismatch at vaddr 0x%X: file=0x%02X mem=0x%02X"
              % (0x600000 + i, ftext[i], mtext[i]))
        break
