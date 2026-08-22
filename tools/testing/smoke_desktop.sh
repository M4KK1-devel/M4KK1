#!/bin/bash
# M4KK1 desktop-interaction smoke test (headless QEMU)
# Boots the full ISO (TEST_AUTOLOGIN build → session auto-enters),
# waits, then screendumps and asserts the desktop is really up.
set -u
cd /mnt/f/M4KK1
ISO=$(ls output/m4kk1_*.iso 2>/dev/null | head -1)
if [ -z "$ISO" ]; then echo "NO ISO (cron race?)"; exit 2; fi
SMOKE_LOG=/tmp/m4kk1_desk_smoke.log
QMP_SOCK=/tmp/m4kk1_desk_qmp.sock
SHOT=/tmp/m4kk1_desk_shot.ppm
rm -f "$SMOKE_LOG" "$QMP_SOCK" "$SHOT"

qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
  -serial "file:$SMOKE_LOG" -qmp "unix:$QMP_SOCK,server=on,wait=off" \
  -display none &
QPID=$!
sleep 40

python3 tools/test/qmp_client.py "$QMP_SOCK" screendump "$SHOT"
python3 tools/test/assert_desktop.py "$SHOT"
ASSERT=$?

echo "=== serial tail ==="
tail -20 "$SMOKE_LOG"
echo "=== EXC check ==="
grep -c "\[EXC\]" "$SMOKE_LOG" || echo "no EXC"
kill $QPID 2>/dev/null
exit $ASSERT
