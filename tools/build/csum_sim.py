#!/usr/bin/env python3
"""Replicate the kernel tcp_checksum algorithm on the captured SYN
to find why it differs from the RFC checksum."""
import struct

pkt = bytes.fromhex(
    "52 55 0a 00 02 02 52 54 00 12 34 56 08 00 "
    "45 00 00 28 00 03 00 00 40 06 62 bd 0a 00 02 0f 0a 00 02 02 "
    "04 00 a5 19 00 00 10 00 00 00 00 00 50 02 10 00 b4 d2 00 00")
tcp = pkt[34:]
src = 0x0a00020f
dst = 0x0a000202

def fold(s):
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return s

# kernel algorithm: LE u16 loads of network-order bytes
# memory bytes b0 b1 -> LE u16 = b0 | b1<<8
def kernel_algo(tcp_bytes, src, dst, ln):
    s = 0
    for i in range(0, ln // 2 * 2, 2):
        s += tcp_bytes[i] | (tcp_bytes[i+1] << 8)
    if ln & 1:
        s += tcp_bytes[ln - 1]
    s += (src >> 16) & 0xFFFF
    s += src & 0xFFFF
    s += (dst >> 16) & 0xFFFF
    s += dst & 0xFFFF
    s += 6
    s += ln
    return (~fold(s)) & 0xFFFF

# checksum field zeroed
c = bytearray(tcp)
c[16:18] = b"\x00\x00"
print("kernel algo -> 0x%04x (captured 0xb4d2)" % kernel_algo(bytes(c), src, dst, 20))

# what src/dst would produce the captured value?
# standard (big-endian words) result:
def std_algo(tcp_bytes, src, dst, ln):
    s = 0
    for i in range(0, ln, 2):
        s += (tcp_bytes[i] << 8) | tcp_bytes[i+1]
    s += (src >> 16) & 0xFFFF
    s += src & 0xFFFF
    s += (dst >> 16) & 0xFFFF
    s += dst & 0xFFFF
    s += 6
    s += ln
    return (~fold(s)) & 0xFFFF
print("big-endian words -> 0x%04x" % std_algo(bytes(c), src, dst, 20))

# try src/dst byte-swapped (little-endian pseudo)
sbs = ((src & 0xFF) << 24) | ((src & 0xFF00) << 8) | ((src >> 8) & 0xFF00) | (src >> 24)
dbs = ((dst & 0xFF) << 24) | ((dst & 0xFF00) << 8) | ((dst >> 8) & 0xFF00) | (dst >> 24)
print("LE pseudo -> 0x%04x" % kernel_algo(bytes(c), sbs, dbs, 20))
