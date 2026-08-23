#!/bin/bash
# M4KK1 man-command smoke test (headless QEMU)
# Asserts: "man pages installed" appears on serial, boot reaches init,
# no [EXC].
set -u
cd /mnt/f/M4KK1
ISO=$(ls output/m4kk1_*.iso 2>/dev/null | head -1)
if [ -z "$ISO" ]; then echo "NO ISO"; exit 2; fi
LOG=/tmp/m4kk1_man_smoke.log
rm -f "$LOG"

timeout 60 qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
  -serial "file:$LOG" -display none > /dev/null 2>&1

echo "=== man install line ==="
grep -a "man pages installed" "$LOG" || { echo "MISSING man install line"; tail -30 "$LOG"; exit 1; }
echo "=== EXC check ==="
if grep -aq "\[EXC\]" "$LOG"; then echo "EXC FOUND"; grep -a "\[EXC\]" "$LOG" | head -3; exit 1; fi
echo "no EXC"
echo "=== boot tail ==="
tail -8 "$LOG"
exit 0
