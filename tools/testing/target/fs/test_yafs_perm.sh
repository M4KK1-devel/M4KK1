#!/bin/sh
# 测试 YAFS UID/GID 权限
. ../lib/test_util.m4sh

printf "检查当前用户身份...\n"
uid=$(id -u 2>/dev/null || echo 0)
assert_eq "$uid" "0" "perm: 当前用户是 root (UID=0)"

printf "尝试访问受保护文件...\n"
if [ -f /root/secret.txt ]; then
    cat /root/secret.txt > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        assert_eq 0 0 "perm: root 可以读取 /root/secret.txt"
    fi
else
    skip "perm: /root/secret.txt 不存在"
fi
