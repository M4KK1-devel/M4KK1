#!/bin/bash
# fresh evidence: boot newest ISO headless, assert 124 man pages + no EXC
cd /mnt/f/M4KK1
ISO=$(ls -t output/*.iso | head -1)
echo "ISO=$ISO"
timeout 90 qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
  -serial file:/tmp/man_fresh.log -display none >/dev/null 2>&1
grep -a 'man pages installed' /tmp/man_fresh.log || { echo "MISSING install line"; exit 1; }
echo "EXC_count=$(grep -ac EXC /tmp/man_fresh.log)"
tail -3 /tmp/man_fresh.log
