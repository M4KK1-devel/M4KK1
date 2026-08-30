#!/bin/bash
# cron task1: build log save + qemu smoke test + state write
set -u
REPO=/mnt/f/M4KK1
cd "$REPO" || exit 1

TS=$(date +%Y%m%d_%H%M%S)
BUILD_LOG="logs/build_${TS}.log"
cp /tmp/m4kk1_build.out "$BUILD_LOG"
echo "saved: $BUILD_LOG"

ERRCNT=$(grep -ciE 'error:' "$BUILD_LOG" || true)
echo "error lines: $ERRCNT"

# --- QEMU minimal smoke test ---
ISO=$(ls -t output/m4kk1_*full*.iso | head -1)
SMOKE_LOG="logs/qemu_smoke_${TS}.log"
echo "ISO: $ISO"
echo "smoke log: $SMOKE_LOG"

timeout 120 qemu-system-i386 -cdrom "$ISO" -m 512 -vga std \
	-serial file:"$REPO/$SMOKE_LOG" -display none -no-reboot \
	>/tmp/qemu_err.out 2>&1
echo "qemu exit: $?"
head -5 /tmp/qemu_err.out

echo "--- serial tail ---"
tail -30 "$SMOKE_LOG"

echo "--- panic check ---"
if grep -qiE 'PANIC|Exception|General Protection' "$SMOKE_LOG"; then
	echo "SMOKE_RESULT=FAIL"
else
	echo "SMOKE_RESULT=PASS"
fi
