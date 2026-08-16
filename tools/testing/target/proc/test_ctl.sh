#!/bin/sh
# 测试 /sys/proc/<PID>/ctl
. ../lib/test_util.m4sh

printf "测试进程控制 (状态标签)...\n"
if [ -f /sys/proc/1/ctl ]; then
    echo "state +WAIT_TIMER" > /sys/proc/1/ctl 2>/dev/null
    if [ $? -eq 0 ]; then
        status=$(cat /sys/proc/1/status 2>/dev/null)
        echo "$status" | grep -q "WAIT_TIMER"
        assert_eq $? 0 "proc/ctl: WAIT_TIMER 标签已添加"
    else
        skip "proc/ctl: 写入失败 (可能只读)"
    fi
else
    skip "proc/ctl: /sys/proc/1/ctl 不存在"
fi
