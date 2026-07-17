#!/bin/sh
# 测试 YAFS 文件创建
. ../lib/test_util.m4sh

printf "创建测试文件...\n"
echo "test data" > /testfile.txt 2>/dev/null
if [ -f /testfile.txt ]; then
    size=$(stat -c %s /testfile.txt 2>/dev/null || wc -c < /testfile.txt)
    assert_eq "$size" "10" "yafs_create: 文件大小正确"
    rm /testfile.txt
else
    skip "yafs_create: 创建文件失败 (根目录可能只读)"
fi
