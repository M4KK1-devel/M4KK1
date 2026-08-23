#!/bin/bash
# Interactive man command test: boot the full ISO (TEST_AUTOLOGIN),
# then drive the in-desktop terminal? No — simpler: boot cmd ISO.
# Actually the -full ISO autologins into the desktop; the m4sh
# interaction path needs the cmd build. Use the -test ISO variant
# which also has TEST_AUTOLOGIN but check what init runs.
set -u
cd /mnt/f/M4KK1
ISO=$(ls output/m4kk1_*.iso 2>/dev/null | head -1)
[ -z "$ISO" ] && { echo "NO ISO"; exit 2; }
LOG=/tmp/m4kk1_man_interact.log
rm -f "$LOG"

# feed commands after boot delay via a coprocess-style pipe:
{ sleep 25; printf 'man ls\n'; sleep 4; printf 'man man\n'; sleep 4; printf 'man -l\n'; sleep 4; } | \
timeout 55 qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
    -nographic -serial mon:stdio -no-reboot -display none > "$LOG" 2>&1

echo "=== man ls output ==="
grep -a -A2 "列出目录内容" "$LOG" | head -6
echo "=== man man output ==="
grep -a "显示手册页" "$LOG" | head -2
echo "=== man -l output ==="
grep -a -E "section|ls\.1|man\.1" "$LOG" | head -8
echo "=== verdict ==="
grep -aq "列出目录内容" "$LOG" && echo "MAN_LS_OK" || echo "MAN_LS_MISSING"
