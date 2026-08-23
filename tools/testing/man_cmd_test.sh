#!/bin/bash
# cmd-only build + serial-shell man verification.
# The cmd-only ISO boots into the console m4sh where stdin/stdout is
# the serial line, so we can actually drive `man ls` and read output.
set -u
cd /mnt/f/M4KK1
LOG=/tmp/m4kk1_man_cmd.log
rm -f "$LOG"

bash tools/build/build_krn.sh --cmd-only > /tmp/m4kk1_cmd_build.log 2>&1 || {
    echo "BUILD FAIL"; grep -m5 "error:" /tmp/m4kk1_cmd_build.log; exit 1; }
ISO=$(ls output/m4kk1_*.iso | head -1)
echo "built: $ISO"

{ sleep 18; printf 'root\n'; sleep 2; printf '123456\n'; sleep 4; printf 'man ls\n'; sleep 5; printf 'man man\n'; sleep 5; printf 'man -l\n'; sleep 5; } | \
timeout 50 qemu-system-i386 -cdrom "$ISO" -m 512 \
    -nographic -serial mon:stdio -no-reboot -display none > "$LOG" 2>&1

echo "=== man install ==="
grep -a "man pages installed" "$LOG" || echo "MISSING install line"
echo "=== man ls ==="
grep -a -m1 -A3 "LS (man page)" "$LOG" || echo "MISSING man ls output"
echo "=== man man ==="
grep -a -m1 "MAN (man page)" "$LOG" || echo "MISSING man man output"
echo "=== man -l ==="
grep -a -m1 -A4 "section" "$LOG" || echo "MISSING man -l output"
echo "=== verdict ==="
grep -aq "LS (man page)" "$LOG" && grep -aq "MAN (man page)" "$LOG" && echo ALL_OK || echo INCOMPLETE
