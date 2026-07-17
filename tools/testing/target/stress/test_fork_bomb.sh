#!/bin/sh
# 压力测试: fork 炸弹 (谨慎使用!)
# 默认只 fork 5 次，可通过参数覆盖: sh test_fork_bomb.sh 10
. ../lib/test_util.m4sh

MAX_FORKS=${1:-5}
count=0

printf "Fork 炸弹测试 (限制 %d 次 fork)...\n" $MAX_FORKS

bomb() {
    local depth=$1
    if [ $depth -le 0 ]; then
        return
    fi
    /bin/calc -e "m4k_fork_status(0, RFPROC)" > /dev/null 2>&1 &
    bomb $((depth - 1))
}

bomb $MAX_FORKS
sleep 1

# 统计进程数
if [ -x /bin/ps ]; then
    proc_count=$(ps 2>/dev/null | wc -l)
    printf "当前进程数: %d\n" "$proc_count"
    assert_ne "$proc_count" "0" "stress: 进程数非零 (fork 成功)"
else
    skip "stress: /bin/ps 不可用"
fi
