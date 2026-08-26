#!/usr/bin/env python3
"""Parse a pcap, print ARP/TCP/ICMP lines. Usage: pcap_parse.py file [max]"""
import struct, sys

d = open(sys.argv[1], "rb").read()
maxp = int(sys.argv[2]) if len(sys.argv) > 2 else 40
off = 24
i = 0
while off + 16 <= len(d) and i < maxp:
    ts, tl = struct.unpack("<II", d[off:off+8])
    off += 16
    pkt = d[off:off+tl]
    off += tl
    i += 1
    if len(pkt) < 14:
        print("pkt%d short" % i)
        continue
    et = struct.unpack(">H", pkt[12:14])[0]
    if et == 0x0806:
        op = struct.unpack(">H", pkt[20:22])[0]
        sip = ".".join(str(b) for b in pkt[28:32])
        tip = ".".join(str(b) for b in pkt[38:42])
        print("pkt%d ARP op=%d %s -> %s" % (i, op, sip, tip))
        continue
    if et != 0x0800 or len(pkt) < 34:
        print("pkt%d eth type=0x%04x len=%d" % (i, et, len(pkt)))
        continue
    src = ".".join(str(b) for b in pkt[26:30])
    dst = ".".join(str(b) for b in pkt[30:34])
    proto = pkt[23]
    ihl = (pkt[14] & 0x0F) * 4
    if proto == 6:
        sp, dp = struct.unpack(">HH", pkt[14+ihl:18+ihl])
        flags = pkt[14+ihl+13]
        payload = len(pkt) - 14 - ihl - ((pkt[14+ihl+12] >> 4) * 4)
        print("pkt%d TCP %s:%d -> %s:%d flags=0x%02x pay=%d" %
              (i, src, sp, dst, dp, flags, max(payload, 0)))
    elif proto == 1:
        print("pkt%d ICMP %s -> %s type=%d" % (i, src, dst, pkt[14+ihl]))
    else:
        print("pkt%d IP proto=%d %s -> %s" % (i, proto, src, dst))
