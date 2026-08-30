#!/bin/bash
# task1 wrapper verification: run kernel regression suite (make test)
set -u
cd /mnt/f/M4KK1 || exit 9
LOG=/tmp/task1_make_test.log
make test > "$LOG" 2>&1
RC=$?
echo "make_test_exit=$RC"
# strip ANSI before judging (green criteria: 失败: 0 AND 所有测试通过)
sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -E "失败:|所有测试通过|PASS|FAIL" | tail -10
echo "---tail---"
sed 's/\x1b\[[0-9;]*m//g' "$LOG" | tail -5
