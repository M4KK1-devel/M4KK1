#!/bin/bash
set -u
cd /mnt/f/M4KK1 || exit 9
bash tools/testing/cron_task1_state.sh PASS "build_20260830_200141 clean (krn 2170232B, ISO 34473984B); qemu_smoke_20260830_200150 no PANIC, boot to MDM login loop; HEAD f13edce"
echo "state_exit=$?"
cat logs/state/fail_task1 2>/dev/null || echo "fail_task1 absent (0)"
tail -2 logs/state/task1_history.log
