#!/usr/bin/env python3
"""Verify checksums of the TCP SYN in /tmp/wget2.pcap."""
import struct

d = open("/tmp/wget2.pcap", "rb").read()
off = 24
while off + 16 <= len(d):
    ts, us, incl, orig = struct.unpack("<IIII", d[off:off+16])
    off += 16
    pkt = d[off:off+incl]
    off += incl
    if len(pkt) < 34 or pkt[12:14] != b"\x08\x00" or pkt[23] != 6:
        continue
    ihl = (pkt[14] & 0xF) * 4
    src = pkt[26:30]
    dst = pkt[30:34]
    tcp = pkt[14+ihl:]
    sp, dp, seq, ack = struct.unpack(">HHII", tcp[:12])
    off_b = tcp[12]
    flags = tcp[13]
    got = struct.unpack(">H", tcp[16:18])[0]
    pseudo = src + dst + b"\x00\x06" + struct.pack(">H", len(tcp))
    c = bytearray(tcp)
    c[16:18] = b"\x00\x00"
    s = sum(pseudo) + sum(c)
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    want = (~s) & 0xFFFF
    # ip checksum too
    iph = bytearray(pkt[14:14+ihl])
    iph[10:12] = b"\x00\x00"
    s2 = sum(iph)
    while s2 >> 16:
        s2 = (s2 & 0xFFFF) + (s2 >> 16)
    ipok = ((~s2) & 0xFFFF) == struct.unpack(">H", pkt[24:26])[0]
    print("TCP %d->%d flags=0x%02x win=0x%04x hdrlen=%d" % (sp, dp, flags,
          struct.unpack('>H', tcp[14:16])[0], (off_b >> 4) * 4))
    print("  tcp csum got=0x%04x computed=0x%04x %s" %
          (got, want, "OK" if got == want else "BAD"))
    print("  ip csum %s" % ("OK" if ipok else "BAD"))
