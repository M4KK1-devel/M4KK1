#!/bin/bash
# M4KK1 完整功能测试脚本
# 在WSL下运行

set -e

echo "=========================================="
echo "M4KK1 完整功能测试"
echo "=========================================="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0

check_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    PASS=$((PASS+1))
}

check_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    FAIL=$((FAIL+1))
}

# 测试1: 检查ISO文件
echo "=== 测试1: ISO文件检查 ==="
if ls output/m4kk1_*.iso >/dev/null 2>&1; then
    check_pass "ISO文件存在"
else
    check_fail "ISO文件不存在"
    exit 1
fi

# 测试2: 检查ELF文件
echo ""
echo "=== 测试2: ELF文件检查 ==="
ELFS=("m4sh" "login" "mdm" "flip_test" "init")
for elf in "${ELFS[@]}"; do
    if [ -f "usr/bin/$elf" ]; then
        SIZE=$(stat -c%s "usr/bin/$elf" 2>/dev/null || echo "0")
        check_pass "$elf 存在 ($SIZE bytes)"
    else
        check_fail "$elf 不存在"
    fi
done

# mdm_mini 是可选的测试程序
if [ -f "usr/bin/mdm_mini" ]; then
    SIZE=$(stat -c%s "usr/bin/mdm_mini" 2>/dev/null || echo "0")
    check_pass "mdm_mini 存在 ($SIZE bytes)"
else
    echo -e "${YELLOW}[INFO]${NC} mdm_mini 不存在（可选测试程序）"
fi

# 测试3: 检查安全修复
echo ""
echo "=== 测试3: 安全修复验证 ==="

# 检查login.c中是否还有硬编码密码
if grep -q "123456" usr/src/cmd/login.c; then
    check_fail "login.c 中仍存在硬编码密码"
else
    check_pass "login.c 硬编码密码已移除"
fi

# 检查mdm.c中是否使用passwd.db
if grep -q "musr_getpwnam" usr/src/cmd/mdm.c; then
    check_pass "mdm.c 使用 passwd.db 验证"
else
    check_fail "mdm.c 未使用 passwd.db 验证"
fi

# 检查不安全函数替换
STRCPY_COUNT=$(grep -r "musr_strcpy" --include="*.c" | grep -v "musr_strncpy" | wc -l)
if [ "$STRCPY_COUNT" -eq 0 ]; then
    check_pass "所有 musr_strcpy 已替换"
else
    check_fail "仍有 $STRCPY_COUNT 处 musr_strcpy 未替换"
fi

# Third-party trees (vendored pcc/make) are excluded from security
# scans — the review policy skips them and their sprintf usage is
# upstream code we don't audit or rewrite.  m4k_libc/stdio.c hosts
# the sprintf/vsprintf IMPLEMENTATION itself (not a call site).
SPRINTF_COUNT=$(grep -r "sprintf" --include="*.c" \
    --exclude-dir=pcc-20220331 --exclude-dir=make-4.4.1 --exclude-dir=repos \
    --exclude=stdio.c \
    | grep -v "snprintf" | wc -l)
if [ "$SPRINTF_COUNT" -eq 0 ]; then
    check_pass "所有 sprintf 已替换"
else
    check_fail "仍有 $SPRINTF_COUNT 处 sprintf 未替换"
fi

# 测试4: 编译验证
echo ""
echo "=== 测试4: 编译验证 ==="
# Build FULL explicitly: the QEMU boot assertions in test 5 expect
# MDM (graphical login), and a default build reading build.config
# may produce a cmd-only ISO that has no MDM at all.
if ./tools/build/build_krn.sh --full > /tmp/build.log 2>&1; then
    check_pass "编译成功"
else
    check_fail "编译失败"
    echo "查看编译日志: tail -50 /tmp/build.log"
fi

# 测试5: QEMU启动测试（串口模式）
echo ""
echo "=== 测试5: QEMU启动测试 ==="
# Pick the NEWEST ISO (ls -t): a bare `ls | head -1` picks the
# lexicographically first name, which is cmd-only when that mode
# was ever built — it has no MDM and fails the login assertions
# below regardless of the code under test.
TEST_ISO=$(ls -t output/m4kk1_*.iso | head -1)
# Serial → FILE, not stdio: the stdio/mon:stdio backends need stdin
# to be a real tty — under a pipe or /dev/null QEMU either stalls or
# muxes the monitor in, and the boot greps FAIL.  A log file works
# identically in cron, pipe, and interactive-TUI environments.
rm -f /tmp/qemu_test.log
timeout 20 qemu-system-i386 -cdrom "$TEST_ISO" \
    -display none -serial file:/tmp/qemu_test.log -no-reboot &
QEMU_PID=$!
# stream what lands in the log so the console still shows progress
( for i in $(seq 1 40); do
    [ -f /tmp/qemu_test.log ] && cat /tmp/qemu_test.log 2>/dev/null
    sleep 0.5
  done ) &
TAIL_PID=$!

# 等待系统启动
sleep 15

# 检查Init是否启动
if grep -q "Init process started" /tmp/qemu_test.log; then
    check_pass "Init进程成功启动"
else
    check_fail "Init进程未启动"
fi

# 检查MDM是否被尝试启动
if grep -q "Starting MDM" /tmp/qemu_test.log; then
    check_pass "Init尝试启动MDM图形登录"
else
    check_fail "Init未尝试启动MDM"
fi

# 检查MDM是否成功加载
if grep -q "Starting M4KK1 Display Manager" /tmp/qemu_test.log && grep -q "ELF loaded" /tmp/qemu_test.log; then
    check_pass "MDM ELF成功加载并执行"
else
    check_fail "MDM加载失败"
fi

# 清理QEMU进程
kill $QEMU_PID 2>/dev/null || true
kill $TAIL_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true
wait $TAIL_PID 2>/dev/null || true

# 测试6: 检查关键文件
echo ""
echo "=== 测试6: 关键文件检查 ==="
KEY_FILES=(
    "m4sh/m4sh.h"
    "m4sh/lib/pwd.c"
    "usr/src/cmd/login.c"
    "usr/src/cmd/mdm.c"
    "usr/src/lib/libgui.c"
    "init/init.c"
    "sys/src/kernel/kmain.c"
)

for file in "${KEY_FILES[@]}"; do
    if [ -f "$file" ]; then
        check_pass "$file 存在"
    else
        check_fail "$file 不存在"
    fi
done

# 测试7: 检查密码哈希实现
echo ""
echo "=== 测试7: 密码哈希检查 ==="
if grep -q "sha256_ctx_t" m4sh/lib/pwd.c; then
    check_pass "SHA-256 已实现"
else
    check_fail "SHA-256 未实现"
fi

if grep -q "musr_hash_password" m4sh/lib/pwd.c; then
    check_pass "musr_hash_password 函数存在"
else
    check_fail "musr_hash_password 函数不存在"
fi

if grep -q "SALT_SIZE" m4sh/lib/pwd.c; then
    check_pass "加盐逻辑已实现"
else
    check_fail "加盐逻辑未实现"
fi

# 测试8: 检查命令注入防护
echo ""
echo "=== 测试8: 命令注入防护检查 ==="
if grep -q "validate_command_input" usr/src/cmd/batch.c; then
    check_pass "batch.c 有输入验证"
else
    check_fail "batch.c 缺少输入验证"
fi

if grep -q "validate_command_input" usr/src/cmd/at.c; then
    check_pass "at.c 有输入验证"
else
    check_fail "at.c 缺少输入验证"
fi

# 测试9: Copland 协议层单元测试（洁净室阶段1-2）
echo ""
echo "=== 测试9: Copland 协议层单元测试 ==="
if [ -f sys/src/copland/copland_proto.c ]; then
    if gcc -Wall -Wextra -Isys/src/copland \
        -o /tmp/cp_proto_test \
        tools/testing/copland/test_proto_host.c \
        sys/src/copland/copland_proto.c 2>/tmp/cp_proto_err.log; then
        if /tmp/cp_proto_test > /tmp/cp_proto_out.log 2>&1; then
            check_pass "copland 协议层单元测试 (42 asserts)"
        else
            check_fail "copland 协议层单元测试有断言失败"
            cat /tmp/cp_proto_out.log
        fi
    else
        check_fail "copland 协议层单元测试编译失败"
        cat /tmp/cp_proto_err.log
    fi
else
    check_fail "sys/src/copland/copland_proto.c 不存在"
fi

# 测试10: Copland 合成器核心单元测试（洁净室阶段2）
echo ""
echo "=== 测试10: Copland 合成器单元测试 ==="
if [ -f sys/src/copland/compositor.c ]; then
    if gcc -Wall -Wextra -Isys/src/copland \
        -o /tmp/cp_comp_test \
        tools/testing/copland/test_compositor_host.c \
        sys/src/copland/compositor.c \
        sys/src/copland/copland_proto.c 2>/tmp/cp_comp_err.log; then
        if /tmp/cp_comp_test > /tmp/cp_comp_out.log 2>&1; then
            check_pass "copland 合成器单元测试 (51 asserts)"
        else
            check_fail "copland 合成器单元测试有断言失败"
            cat /tmp/cp_comp_out.log
        fi
    else
        check_fail "copland 合成器单元测试编译失败"
        cat /tmp/cp_comp_err.log
    fi
else
    check_fail "sys/src/copland/compositor.c 不存在"
fi

# 汇总
echo ""
echo "=========================================="
echo "测试汇总"
echo "=========================================="
echo -e "通过: ${GREEN}$PASS${NC}"
echo -e "失败: ${RED}$FAIL${NC}"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}所有测试通过！${NC}"
    exit 0
else
    echo -e "${RED}有 $FAIL 项测试失败${NC}"
    exit 1
fi
