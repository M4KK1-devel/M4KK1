#!/bin/bash
# Atomic build + probe chain (avoid ISO race with the 10h cron)
cd /mnt/f/M4KK1 || exit 9
mkdir -p logs
LOG="logs/build_$(date +%Y%m%d_%H%M%S).log"

# bail out if another build is already running
if pgrep -f build_krn.sh > /dev/null; then
    echo "BUSY: another build_krn.sh running"
    exit 8
fi

./tools/build/build_krn.sh --full-test > "$LOG" 2>&1
rc=$?
echo "EXIT=$rc" >> "$LOG"
if [ $rc -ne 0 ]; then
    echo "BUILD FAILED rc=$rc see $LOG"
    exit $rc
fi
echo "BUILD OK: $LOG"

python3 tools/build/heap_probe.py
hrc=$?
echo "HEAP_PROBE rc=$hrc"

python3 tools/build/wmenu_repro.py
wrc=$?
echo "WMENU_PROBE rc=$wrc"

python3 tools/build/wmenu_term_probe.py
wtrc=$?
echo "WMENU_TERM_PROBE rc=$wtrc"

python3 tools/build/sysmon_probe.py 2>/dev/null
src=$?
echo "SYSMON_PROBE rc=$src"

# host unit tests
make test 2>&1 | tail -4
exit $(( hrc | src | wrc | wtrc ))
