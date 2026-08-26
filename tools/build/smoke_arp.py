#!/usr/bin/env python3
"""Boot with pcap capture to debug ARP/ICMP exchange."""
import os, socket, subprocess, time

os.chdir("/mnt/f/M4KK1")
isos = sorted([f for f in os.listdir("output") if f.endswith("full-test.iso")],
              key=lambda f: os.path.getmtime(os.path.join("output", f)))
iso = os.path.join("output", isos[-1])
sock = "/tmp/m4k_net2.sock"
pcap = "/tmp/m4k_net.pcap"
for f in (sock, pcap):
    if os.path.exists(f):
        os.unlink(f)
log = open("logs/smoke_arp.log", "wb")

qemu = subprocess.Popen([
    "qemu-system-i386", "-cdrom", iso, "-m", "512", "-vga", "std",
    "-netdev", "user,id=n0", "-object", "filter-dump,id=f1,netdev=n0,file=%s" % pcap,
    "-device", "e1000,netdev=n0",
    "-serial", "unix:%s,server=on,wait=off" % sock,
    "-display", "none", "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
time.sleep(2)
print("qemu pid", qemu.pid, "poll:", qemu.poll())
time.sleep(1)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    s.connect(sock)
    print("serial connected")
except Exception as e:
    print("serial connect failed:", e)
s.setblocking(False)

def drain(t):
    end = time.time() + t
    while time.time() < end:
        try:
            d = s.recv(65536)
            if d:
                log.write(d)
        except BlockingIOError:
            pass
        time.sleep(0.1)

def send(line):
    s.sendall((line + "\r").encode())

drain(25)
send("ping 10.0.2.2")
drain(20)
qemu.terminate()
try:
    qemu.wait(5)
except Exception:
    qemu.kill()
log.close()
print("--- serial grep ---")
text = open("logs/smoke_arp.log", "rb").read().decode("utf-8", "replace")
for line in text.splitlines():
    if any(k in line for k in ("PING", "reply", "e1000", "Network")):
        print(line.strip()[:120])
print("--- pcap ---")
print("size", os.path.getsize(pcap) if os.path.exists(pcap) else "missing")
if os.path.exists(pcap):
    import struct
    data = open(pcap, "rb").read()
    off = 24
    n = 0
    while off + 16 <= len(data):
        ts, tus, caplen, orig = struct.unpack("<IIII", data[off:off+16])
        off += 16 + caplen
        n += 1
        pkt = data[off-caplen:off]
        if len(pkt) >= 14:
            etype = (pkt[12] << 8) | pkt[13]
            kind = {0x0806: "ARP", 0x0800: "IP"}.get(etype, "ETH%04x" % etype)
            extra = ""
            if etype == 0x0806 and len(pkt) >= 42:
                op = (pkt[20] << 8) | pkt[21]
                extra = " op=%d %d.%d.%d.%d -> %d.%d.%d.%d" % (
                    op, pkt[28], pkt[29], pkt[30], pkt[31],
                    pkt[38], pkt[39], pkt[40], pkt[41])
            if etype == 0x0800 and len(pkt) >= 38:
                proto = pkt[23]
                extra = " proto=%d %d.%d.%d.%d -> %d.%d.%d.%d" % (
                    proto, pkt[26], pkt[27], pkt[28], pkt[29],
                    pkt[30], pkt[31], pkt[32], pkt[33])
            print("pkt%d %s%s" % (n, kind, extra))
