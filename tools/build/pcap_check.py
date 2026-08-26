#!/usr/bin/env python3
"""Dump /tmp/wget2.pcap; resync after a corrupt record header."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
off = 24
n = 0
while off + 16 <= len(d) and n < 60:
    ts, tu, incl, orig = struct.unpack("<IIII", d[off:off+16])
    if incl > 1600 or off + 16 + incl > len(d):
        # resync: search for a plausible next record (12 zeros + sane incl)
        j = off + 1
        found = False
        while j < len(d) - 16:
            ts2, tu2, incl2, orig2 = struct.unpack("<IIII", d[j:j+16])
            if incl2 == orig2 and 42 <= incl2 <= 1600:
                print("resync +%d (skipped %d bytes)" % (j - off, j - off))
                off = j
                found = True
                break
            j += 1
        if not found:
            print("no resync; stop at", off)
            break
        continue
    pkt = d[off+16:off+16+incl]
    off += 16 + incl
    n += 1
    if len(pkt) < 14:
        print("pkt%d len=%d short" % (n, incl))
        continue
    et = struct.unpack(">H", pkt[12:14])[0]
    line = ""
    if et == 0x0806:
        op = struct.unpack(">H", pkt[20:22])[0]
        line = "ARP op=%d" % op
    elif et == 0x0800:
        p = pkt[23]
        ihl = (pkt[14] & 0xF) * 4
        if p == 6:
            sp, dp = struct.unpack(">HH", pkt[14+ihl:16+ihl+2])
            fl = pkt[14+ihl+13]
            pay = incl - 14 - ihl - ((pkt[14+ihl+12] >> 4) * 4)
            line = "TCP %d->%d fl=0x%02x pay=%d" % (sp, dp, fl, pay)
        elif p == 1:
            line = "ICMP t=%d" % pkt[14+ihl]
        else:
            line = "IP p=%d" % p
    else:
        line = "eth 0x%04x" % et
    print("pkt%d len=%d %s" % (n, incl, line))
