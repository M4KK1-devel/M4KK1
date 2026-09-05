#!/bin/bash
# man suite verification on cmd-only ISO:
#  1) man -l lists sections 1..8 completely (list-cap bug fix)
#  2) man m4k_setuid works (section-2 search fix)
#  3) man musr_getpwnam works (section-3 search fix)
#  4) a new man1 page works (sead)
set -u
cd /mnt/f/M4KK1
LOG=/tmp/m4kk1_man_verify.log
rm -f "$LOG"

ISO=$(ls -t output/*cmd-only*.iso 2>/dev/null | head -1)
if [ -z "$ISO" ]; then
    bash tools/build/build_krn.sh --cmd-only > /tmp/m4kk1_cmdb.log 2>&1 || {
        echo "BUILD FAIL"; grep -m5 "error:" /tmp/m4kk1_cmdb.log; exit 1; }
    ISO=$(ls -t output/*cmd-only*.iso | head -1)
fi
echo "ISO=$ISO"

{
  sleep 16
  printf 'root\n'; sleep 2
  printf '123456\n'; sleep 4
  printf 'man -l\n'; sleep 6
  printf 'man m4k_setuid\n'; sleep 5
  printf 'man musr_getpwnam\n'; sleep 5
  printf 'man sead\n'; sleep 5
  sleep 2
} | timeout 150 qemu-system-i386 -cdrom "$ISO" -m 512 \
    -nographic -serial mon:stdio -no-reboot -display none > "$LOG" 2>&1

echo "=== install ==="
grep -a "man pages installed" "$LOG" || echo "MISSING install line"
echo "=== man -l section 2 (new in list) ==="
grep -a -A3 "section 2" "$LOG" | head -6 || echo "MISSING section 2 in -l"
echo "=== man -l section 3 ==="
grep -a -A3 "section 3" "$LOG" | head -6 || echo "MISSING section 3 in -l"
echo "=== man -l section 1 count ==="
awk '/section 1:/{f=1;next}/section 2:/{f=0}f' "$LOG" | grep -ac "^  " || true
echo "=== man m4k_setuid ==="
grep -a -m1 "SETUID" "$LOG" || echo "MISSING man m4k_setuid"
echo "=== man musr_getpwnam ==="
grep -a -m1 "GETPWNAM" "$LOG" || echo "MISSING man musr_getpwnam"
echo "=== man sead ==="
grep -a -m1 "SEAD" "$LOG" || echo "MISSING man sead"
echo "=== EXC check ==="
grep -ac "EXC" "$LOG" || true
