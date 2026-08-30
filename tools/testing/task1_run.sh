#!/bin/bash
# task1 run helper: build with lock, archive log
set -u
cd /mnt/f/M4KK1 || exit 9
LOG=/tmp/task1_build.log
flock -w 1800 /tmp/m4kk1_cron.lock ./tools/build/build_krn.sh --full > "$LOG" 2>&1
RC=$?
echo "build_exit=$RC"
echo "error_lines=$(grep -c 'error:' "$LOG" || true)"
tail -5 "$LOG"
if [ "$RC" -eq 0 ]; then
  STAMP=$(date +%Y%m%d_%H%M%S)
  cp "$LOG" "logs/build_${STAMP}.log"
  echo "archived=logs/build_${STAMP}.log"
fi
