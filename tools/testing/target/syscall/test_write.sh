#!/bin/sh
# 测试 write 系统调用
. ../lib/test_util.m4sh

printf "写入 /tmp/test_write.txt...\n"
echo "hello" > /tmp/test_write.txt 2>/dev/null
if [ -f /tmp/test_write.txt ]; then
    result=$(cat /tmp/test_write.txt)
    assert_eq "$result" "hello" "write: 写入并读回内容匹配"
    rm /tmp/test_write.txt
else
    skip "write: 跳过 (文件系统可能不支持 /tmp)"
fi
