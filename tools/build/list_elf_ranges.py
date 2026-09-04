#!/usr/bin/env python3
"""List load ranges (vaddr..vaddr+memsz) of all PT_LOAD segments per ELF."""
import struct, glob, os

def load_range(path):
    try:
        d = open(path, 'rb').read()
    except OSError:
        return None
    phoff = struct.unpack_from('<Q', d, 0x20)[0]
    phentsize = struct.unpack_from('<H', d, 0x36)[0]
    phnum = struct.unpack_from('<H', d, 0x38)[0]
    lo, hi = 1 << 30, 0
    for i in range(phnum):
        o = phoff + i * phentsize
        ptype = struct.unpack_from('<I', d, o)[0]
        off, va, pa, fsz, msz = struct.unpack_from('<QQQQQ', d, o + 8)
        if ptype == 1 and msz > 0:
            lo = min(lo, va)
            hi = max(hi, va + msz)
    if lo > hi:
        return None
    return lo, hi

names = []
for pat in ['usr/src/cmd/*.elf', 'usr/src/sprach/*.elf', 'usr/src/shell/*.elf',
            'usr/src/fm/*.elf', 'usr/src/copland/*.elf', 'usr/src/**/*.elf']:
    for f in glob.glob(pat, recursive=True):
        if os.path.basename(f) not in [n[0] for n in names]:
            names.append((os.path.basename(f), f))

rows = []
for name, f in sorted(set(names)):
    r = load_range(f)
    if r:
        rows.append((r[0], r[1], name))
rows.sort()
print('%-16s %-12s %-12s %s' % ('name', 'start', 'end', 'size'))
for lo, hi, n in rows:
    print('%-16s 0x%08X   0x%08X   %6.1f KB' % (n, lo, hi, (hi - lo) / 1024))
