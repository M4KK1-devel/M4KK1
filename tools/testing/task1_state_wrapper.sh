#!/bin/bash
set -u
cd /mnt/f/M4KK1 || exit 9
BL=$(ls -t logs/build_*.log 2>/dev/null | head -1)
SM=$(ls -t logs/qemu_smoke_*.log 2>/dev/null | head -1)
BL=${BL##*/}; SM=${SM##*/}
ISO=$(ls -t output/*-full.iso 2>/dev/null | head -1)
ISOSZ=$(stat -c %s "$ISO" 2>/dev/null)
HEAD=$(git rev-parse --short HEAD 2>/dev/null)
bash tools/testing/cron_task1_state.sh PASS "$BL clean (ISO $ISOSZ B); $SM no PANIC, boot to MDM login loop; HEAD $HEAD"
echo "state_exit=$?"
cat logs/state/fail_task1 2>/dev/null || echo "fail_task1 absent (0)"
tail -2 logs/state/task1_history.log
