#!/bin/bash
# Drive the cmd-only serial shell: login as root, run man commands.
set -u
cd /mnt/f/M4KK1
ISO=$(ls output/m4kk1_*cmd-only*.iso | head -1)
LOG=/tmp/m4kk1_man_cmd2.log
rm -f "$LOG"
echo "iso: $ISO"

{ sleep 18; printf 'root\n'; sleep 2; printf '123456\n'; sleep 5;
  printf 'man ls\n'; sleep 6;
  printf 'man man\n'; sleep 5;
  printf 'man -l\n'; sleep 5; } | \
timeout 70 qemu-system-i386 -cdrom "$ISO" -m 512 \
    -nographic -serial mon:stdio -no-reboot -display none > "$LOG" 2>&1

echo "=== man ls ==="
grep -a -m1 -A6 "LS (man page)" "$LOG" || echo MISSING_LS
echo "=== man man ==="
grep -a -m1 "MAN (man page)" "$LOG" || echo MISSING_MAN
echo "=== man -l ==="
grep -a -m1 -A4 "Available manual" "$LOG" || echo MISSING_LIST
echo "=== verdict ==="
grep -aq "LS (man page)" "$LOG" && grep -aq "MAN (man page)" "$LOG" && echo ALL_OK || echo INCOMPLETE
