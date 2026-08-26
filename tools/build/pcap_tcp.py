#!/usr/bin/env python3
"""List ONLY TCP frames in /tmp/wget2.pcap (skip giants by header)."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
off = 24
n = 0
tcp_n = 0
t0 = None
while off + 16 <= len(d):
    ts, tu, incl, orig = struct.unpack("<IIII", d[off:off+16])
    if incl > 1600 or off + 16 + incl > len(d):
        off += 16 + incl
        n += 1
        continue
    pkt = d[off+16:off+16+incl]
    off += 16 + incl
    n += 1
    if len(pkt) < 34 or struct.unpack(">H", pkt[12:14])[0] != 0x0800:
        continue
    if pkt[23] != 6:
        continue
    if t0 is None:
        t0 = ts + tu / 1e6
    rel = ts + tu / 1e6 - t0
    ihl = (pkt[14] & 0xF) * 4
    sp, dp = struct.unpack(">HH", pkt[14+ihl:16+ihl+2])
    fl = pkt[14+ihl+13]
    seq = struct.unpack(">I", pkt[14+ihl+4:18+ihl])[0]
    ack = struct.unpack(">I", pkt[14+ihl+8:22+ihl])[0]
    direction = "G->N" if pkt[6:12] == bytes.fromhex("525400123456") else "N->G"
    tcp_n += 1
    if tcp_n < 30 or tcp_n % 50 == 0:
        print("pkt%d %.3fs %s TCP %d->%d fl=0x%02x seq=0x%x ack=0x%x" %
              (n, rel, direction, sp, dp, fl, seq, ack))
print("total TCP frames:", tcp_n, "of", n)
