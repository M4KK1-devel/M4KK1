#!/bin/sh
# 测试 read 系统调用
. ../lib/test_util.m4sh

printf "读取 /etc/motd...\n"
if cat /etc/motd > /dev/null 2>&1; then
    assert_eq 0 0 "read: 成功读取 /etc/motd"
else
    assert_eq 1 0 "read: 读取 /etc/motd 失败"
fi

printf "读取不存在的文件...\n"
if cat /nonexistent 2>&1 | grep -q "error"; then
    assert_eq 0 0 "read: 正确拒绝不存在的文件"
else
    assert_eq 1 0 "read: 未正确拒绝不存在的文件"
fi
