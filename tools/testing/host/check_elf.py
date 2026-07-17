#!/usr/bin/env python3
"""
check_elf.py — 检查生成的 ELF 格式是否正确
用法: python3 check_elf.py <path/to/elf>
"""
import struct
import sys

ELF_MAGIC = b'\x7fELF'

def check_elf(path):
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic != ELF_MAGIC:
            print(f"FAIL: {path}: bad magic {magic.hex()}")
            return False

        f.seek(4)
        ei_class = struct.unpack('B', f.read(1))[0]
        ei_data = struct.unpack('B', f.read(1))[0]

        print(f"  Class: {'32-bit' if ei_class == 1 else '64-bit' if ei_class == 2 else 'unknown'}")
        print(f"  Endian: {'Little' if ei_data == 1 else 'Big' if ei_data == 2 else 'unknown'}")

        f.seek(16)
        e_type = struct.unpack('<H', f.read(2))[0]
        e_machine = struct.unpack('<H', f.read(2))[0]
        e_entry = struct.unpack('<I', f.read(4))[0]
        e_phoff = struct.unpack('<I', f.read(4))[0]
        e_shoff = struct.unpack('<I', f.read(4))[0]

        type_names = {0: 'NONE', 1: 'REL', 2: 'EXEC', 3: 'DYN', 4: 'CORE'}
        mach_names = {0: 'NONE', 3: 'i386', 40: 'ARM', 62: 'x86_64'}

        print(f"  Type: {type_names.get(e_type, f'unknown ({e_type})')}")
        print(f"  Machine: {mach_names.get(e_machine, f'unknown ({e_machine})')}")
        print(f"  Entry: 0x{e_entry:08x}")
        print(f"  PHDRs: at 0x{e_phoff:x}, SHDRs: at 0x{e_shoff:x}")

        if e_type not in (2, 3):
            print(f"FAIL: {path}: not an executable or shared object")
            return False

        if e_phoff == 0:
            print(f"FAIL: {path}: no program headers")
            return False

        print(f"PASS: {path}: valid ELF")
        return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <elf_file>")
        sys.exit(1)

    all_pass = True
    for path in sys.argv[1:]:
        if not check_elf(path):
            all_pass = False
    sys.exit(0 if all_pass else 1)
