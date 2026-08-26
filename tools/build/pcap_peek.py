#!/usr/bin/env python3
"""Peek at the skipped region in /tmp/wget2.pcap."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
# first skip starts at 24+16+42+16+64 = 162
off = 162
# print record headers + first bytes of every ~50th packet in the region
end = min(off + 19544, len(d))
i = 0
shown = 0
while off + 16 <= end and shown < 12:
    ts, tu, incl, orig = struct.unpack("<IIII", d[off:off+16])
    if incl > 1600 or incl == 0:
        print("stop: incl=%d at %d" % (incl, off))
        break
    pkt = d[off+16:off+16+incl]
    off += 16 + incl
    i += 1
    if i % 40 == 1 and len(pkt) >= 40:
        et = struct.unpack(">H", pkt[12:14])[0]
        p = pkt[23] if et == 0x0800 and len(pkt) > 23 else 0
        ihl = (pkt[14] & 0xF) * 4 if len(pkt) > 14 else 0
        desc = "eth" if et != 0x0800 else ("TCP" if p == 6 else ("ICMP" if p == 1 else "IP%d" % p))
        extra = ""
        if p == 6 and len(pkt) >= 14+ihl+14:
            sp, dp = struct.unpack(">HH", pkt[14+ihl:16+ihl+2])
            fl = pkt[14+ihl+13]
            extra = " %d->%d fl=0x%02x" % (sp, dp, fl)
        print("f%d len=%d %s%s" % (i, incl, desc, extra))
        shown += 1
print("total frames in region:", i)
