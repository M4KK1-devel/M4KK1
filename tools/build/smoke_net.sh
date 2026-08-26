#!/bin/bash
# QEMU smoke test: boot full-test ISO with e1000 NIC + serial console.
cd /mnt/f/M4KK1 || exit 1
ISO=$(ls -t output/m4kk1_*full-test.iso | head -1)
echo "ISO=$ISO"
timeout 75 qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -serial file:logs/smoke_net_$(date +%H%M%S).log -display none \
  -no-reboot 2>&1 | head -5
LOG=$(ls -t logs/smoke_net_*.log | head -1)
echo "--- serial tail ($LOG) ---"
tail -60 "$LOG"
