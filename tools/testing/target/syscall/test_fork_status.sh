#!/bin/sh
# 测试 fork_status 系统调用
. ../lib/test_util.m4sh

printf "测试选择性 fork...\n"
if [ -x /bin/calc ]; then
    # 通过 calc 计算器调用 fork_status 测试
    result=$(echo "m4k_fork_status(0, RFPROC | RFFDG)" | calc 2>/dev/null)
    assert_ne "$result" "" "fork_status: calc 返回非空结果"
else
    skip "fork_status: /bin/calc 不存在"
fi
