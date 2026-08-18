#!/bin/sh
# 测试 /sys/proc/<PID>/status
. ../lib/test_util.m4sh

printf "读取 PID 1 状态...\n"
if [ -f /sys/proc/1/status ]; then
    content=$(cat /sys/proc/1/status 2>/dev/null)
    echo "$content" | grep -q "PID: 1"
    assert_eq $? 0 "proc/status: PID 1 存在"
    echo "$content" | grep -q "State:"
    assert_eq $? 0 "proc/status: 包含 State 字段"
    echo "$content" | grep -q "CMD:"
    assert_eq $? 0 "proc/status: 包含 CMD 字段"
else
    skip "proc/status: /sys/proc 未挂载"
fi
