#!/bin/bash
# Full M4KK1 build with net + icons (run inside WSL archlinux).
cd /mnt/f/M4KK1 || exit 1
mkdir -p logs
LOG="logs/build_$(date +%Y%m%d_%H%M%S)_neticons.log"
bash tools/build/build_krn.sh --full-test > "$LOG" 2>&1
rc=$?
echo "BUILD_RC=$rc"
tail -30 "$LOG"
echo "LOG=$LOG"
ls -la output/*.iso 2>/dev/null | tail -3
exit $rc
