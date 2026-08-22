import json, os, socket, struct, subprocess, time

ISO = "/mnt/f/M4KK1/output/m4kk1_0.0.1_build4-alpha1-full.iso"
QMP = "/tmp/mz_qmp.sock"
LOG = "/tmp/mz_serial.log"
MILE = 0x807104

subprocess.run(["rm", "-f", QMP, LOG], check=False)
proc = subprocess.Popen(
    ["qemu-system-i386", "-boot", "d", "-cdrom", ISO,
     "-m", "512", "-vga", "std", "-display", "none",
     "-serial", "file:" + LOG,
     "-qmp", "unix:%s,server=on,nowait" % QMP],
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(30)

# QMP socket may need a moment after boot start
s = None
for _ in range(10):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(QMP)
        break
    except (ConnectionRefusedError, FileNotFoundError):
        time.sleep(1)
if s is None:
    print("QMP connect failed; serial tail:",
          open(LOG, errors="replace").read().splitlines()[-1:])
    proc.kill()
    raise SystemExit(1)
f = s.makefile("rw", encoding="utf-8", newline="\n")
json.loads(f.readline())
f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n"); f.flush()
json.loads(f.readline())
f.write(json.dumps({"execute": "dump-guest-memory", "arguments": {
    "paging": False, "protocol": "file:/tmp/mz_mile.dump",
    "begin": MILE & ~0xFFF, "length": 0x1000}}) + "\n"); f.flush()
time.sleep(3)
proc.kill()

d = open("/tmp/mz_mile.dump", "rb").read()
phoff = struct.unpack_from("<Q", d, 0x20)[0]
for i in range(4):
    o = phoff + i * 0x38
    t = struct.unpack_from("<I", d, o)[0]
    if t == 1:
        off = struct.unpack_from("<Q", d, o + 8)[0]
        va = struct.unpack_from("<Q", d, o + 16)[0]
        v = struct.unpack_from("<I", d, off + (MILE - va))[0]
        print("milestone = 0x%08X" % v)
        print("meaning:", {0xDEAD0000: "init", 0xDEAD00A0: "session loop",
                           0xDEAD00A1: "pre-draw", 0xDEAD0001: "form drawn",
                           0xDEAD0002: "autologin start",
                           0xDEAD0003: "auth OK", 0xDEAD0004: "auth FAIL",
                           0xDEAD0005: "copland forked"}.get(v, "?"))
        break
print("serial tail:", open(LOG, errors="replace").read().splitlines()[-1])
