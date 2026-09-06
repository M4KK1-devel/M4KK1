#!/bin/bash
# wait for in-flight builds, then build full-test ISO for the resize probe
cd /mnt/f/M4KK1 || exit 1
for i in $(seq 1 40); do
    pgrep -f build_krn >/dev/null || break
    sleep 15
done
bash tools/build/build_krn.sh --full-test > logs/build_resize.log 2>&1
echo BUILD_RC=$?
grep -iE "error|错误" logs/build_resize.log | grep -v warning | head -5
ls -t output/*.iso | head -1
