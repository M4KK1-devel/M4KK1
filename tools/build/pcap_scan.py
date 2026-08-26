#!/usr/bin/env python3
"""Alternative parse: treat each 60-byte ARP / other frame; find ALL records
whose content contains TCP signature (ethertype 0x0800 + proto 6)."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
# brute scan: every 'dst mac' position where bytes 12:14 == 0800 and pkt23==6
n_tcp = 0
for i in range(len(d) - 40):
    if d[i+12:i+14] == b"\x08\x00" and d[i+23] == 6:
        # plausible eth frame; extract
        ihl = (d[i+14] & 0xF) * 4
        sp, dp = struct.unpack(">HH", d[i+14+ihl:i+16+ihl])
        fl = d[i+14+ihl+13]
        seq = struct.unpack(">I", d[i+14+ihl+4:i+18+ihl])[0]
        direction = "G->N" if d[i+6:i+12] == bytes.fromhex("525400123456") else "N->G"
        print("at %d %s TCP %d->%d fl=0x%02x seq=0x%x" % (i, direction, sp, dp, fl, seq))
        n_tcp += 1
        if n_tcp > 20:
            break
print("found:", n_tcp)
