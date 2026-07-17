#!/bin/sh
# run_all.sh — 执行所有目标机测试
cd /tools/testing/target || exit 1

. ./lib/test_util.m4sh

printf "=== M4KK1 测试套件 ===\n\n"

for dir in syscall fs proc stress; do
    for test in "$dir"/test_*.sh; do
        [ -f "$test" ] || continue
        printf "--- 运行 %s ---\n" "$test"
        if sh "$test"; then
            :
        else
            printf "测试 %s 返回错误\n" "$test"
        fi
        printf "\n"
    done
done

print_summary
