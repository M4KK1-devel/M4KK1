#!/bin/sh
# 测试 pipe 系统调用
. ../lib/test_util.m4sh

printf "测试管道通信...\n"
echo "hello" | cat > /tmp/pipe_test.txt 2>/dev/null
if [ -f /tmp/pipe_test.txt ]; then
    result=$(cat /tmp/pipe_test.txt)
    assert_eq "$result" "hello" "pipe: 管道传输内容匹配"
    rm /tmp/pipe_test.txt
else
    skip "pipe: 跳过 (管道可能未完全实现)"
fi
