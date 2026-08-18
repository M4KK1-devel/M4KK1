#!/bin/sh
# 测试 open 系统调用
. ../lib/test_util.m4sh

printf "打开 /etc/...\n"
exec 3</etc 2>/dev/null
if [ $? -eq 0 ]; then
    assert_eq 0 0 "open: 成功打开目录"
    exec 3>&-
else
    assert_eq 1 0 "open: 打开目录失败"
fi
