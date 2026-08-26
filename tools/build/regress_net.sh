#!/bin/bash
# Regression: make test (26P/0F baseline) on the net+icons ISO.
cd /mnt/f/M4KK1 || exit 1
export PATH="$HOME/.local/bin:$PATH"
LOG="logs/test_$(date +%Y%m%d_%H%M%S)_neticons.log"
make test > "$LOG" 2>&1
rc=$?
echo "TEST_RC=$rc"
sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -E 'PASS|FAIL|pass|fail' | tail -15
echo "LOG=$LOG"
exit $rc
