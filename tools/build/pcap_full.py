#!/usr/bin/env python3
"""Full robust dump of /tmp/wget2.pcap with timestamps + both directions."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
off = 24
n = 0
GUEST_MAC = bytes.fromhex("525400123456")
t0 = None
while off + 16 <= len(d) and n < 200:
    ts, tu, incl, orig = struct.unpack("<IIII", d[off:off+16])
    if incl > 1600 or off + 16 + incl > len(d):
        print("pkt? GIANT/currupt incl=%d at off=%d (content zero=%s)" %
              (incl, off, set(d[off+16:off+16+64]) == {0}))
        off += 16 + incl          # trust the header and jump
        n += 1
        continue
    pkt = d[off+16:off+16+incl]
    off += 16 + incl
    n += 1
    if t0 is None:
        t0 = ts + tu / 1e6
    rel = ts + tu / 1e6 - t0
    if len(pkt) < 14:
        print("pkt%d %.3fs len=%d short" % (n, rel, incl))
        continue
    direction = "G->N" if pkt[6:12] == GUEST_MAC else "N->G"
    et = struct.unpack(">H", pkt[12:14])[0]
    line = ""
    if et == 0x0806:
        op = struct.unpack(">H", pkt[20:22])[0]
        line = "ARP op=%d" % op
    elif et == 0x0800:
        p = pkt[23]
        ihl = (pkt[14] & 0xF) * 4
        if p == 6 and len(pkt) >= 14 + ihl + 20:
            sp, dp = struct.unpack(">HH", pkt[14+ihl:16+ihl+2])
            fl = pkt[14+ihl+13]
            seq = struct.unpack(">I", pkt[14+ihl+4:18+ihl])[0]
            line = "TCP %d->%d fl=0x%02x seq=0x%x" % (sp, dp, fl, seq)
        elif p == 1:
            line = "ICMP t=%d" % pkt[14+ihl]
        else:
            line = "IP p=%d" % p
    else:
        line = "eth 0x%04x" % et
    print("pkt%d %.3fs %s len=%d %s" % (n, rel, direction, incl, line))
