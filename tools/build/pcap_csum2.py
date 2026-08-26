#!/usr/bin/env python3
"""Correct 16-bit-word checksum verification for the TCP SYN."""
import struct

def cksum(data):
    if len(data) & 1:
        data += b"\x00"
    s = 0
    for i in range(0, len(data), 2):
        s += (data[i] << 8) | data[i+1]
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF

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
    flags = tcp[13]
    got = struct.unpack(">H", tcp[16:18])[0]
    pseudo = src + dst + b"\x00\x06" + struct.pack(">H", len(tcp))
    c = bytearray(tcp)
    c[16:18] = b"\x00\x00"
    want = cksum(bytes(c) and pseudo + bytes(c))
    iph = pkt[14:14+ihl]
    ipgot = struct.unpack(">H", iph[10:12])[0]
    ipw = cksum(iph[:10] + b"\x00\x00" + iph[12:])
    print("TCP %d->%d flags=0x%02x seq=0x%08x" % (sp, dp, flags, seq))
    print("  tcp csum got=0x%04x computed=0x%04x %s" %
          (got, want, "OK" if got == want else "BAD"))
    print("  ip  csum got=0x%04x computed=0x%04x %s" %
          (ipgot, ipw, "OK" if ipgot == ipw else "BAD"))
    print("  full hex:", pkt.hex(" "))
