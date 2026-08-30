#!/bin/bash
# cron task1: write state result (reset/bump fail counter + result line)
REPO=/mnt/f/M4KK1
STATEDIR="$REPO/logs/state"
RESULT="$1"   # PASS | FAIL
DETAIL="$2"
mkdir -p "$STATEDIR"

if [ "$RESULT" = "PASS" ]; then
	rm -f "$STATEDIR/fail_task1"
else
	N=$(( $(cat "$STATEDIR/fail_task1" 2>/dev/null || echo 0) + 1 ))
	echo "$N" > "$STATEDIR/fail_task1"
fi
echo "[$(date '+%F %T')] task1 $RESULT $DETAIL" >> "$STATEDIR/task1_history.log"
echo "fail_task1 now: $(cat "$STATEDIR/fail_task1" 2>/dev/null || echo 0)"
