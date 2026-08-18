#!/bin/bash
# M4KK1 完善测试脚本
# 测试系统启动、图形界面、键盘鼠标、安全修复等

set -e

echo "=========================================="
echo "M4KK1 完善测试"
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

# 测试1: 编译验证
echo "=== 测试1: 编译验证 ==="
if ./tools/build/build_krn.sh > /tmp/build_test.log 2>&1; then
    check_pass "系统编译成功"
else
    check_fail "系统编译失败"
    tail -20 /tmp/build_test.log
    exit 1
fi

# 测试2: ISO文件检查
echo ""
echo "=== 测试2: ISO文件检查 ==="
if [ -f "output/m4kk1_0.0.1_build1-alpha1.iso" ]; then
    SIZE=$(stat -c%s "output/m4kk1_0.0.1_build1-alpha1.iso" 2>/dev/null || echo "0")
    check_pass "ISO文件存在 ($SIZE bytes)"
else
    check_fail "ISO文件不存在"
    exit 1
fi

# 测试3: ELF文件检查
echo ""
echo "=== 测试3: ELF文件检查 ==="
ELFS=("m4sh" "login" "mdm" "flip_test" "init")
for elf in "${ELFS[@]}"; do
    if [ -f "usr/bin/$elf" ]; then
        SIZE=$(stat -c%s "usr/bin/$elf" 2>/dev/null || echo "0")
        check_pass "$elf 存在 ($SIZE bytes)"
    else
        check_fail "$elf 不存在"
    fi
done

# 测试4: 安全修复验证
echo ""
echo "=== 测试4: 安全修复验证 ==="

# 检查login.c中是否还有硬编码密码
if grep -q "123456" usr/src/cmd/login.c; then
    check_fail "login.c 中仍存在硬编码密码"
else
    check_pass "login.c 硬编码密码已移除"
fi

# 检查mdm.c中是否使用passwd.db
if grep -q "musr_getpwnam" usr/src/cmd/mdm.c && grep -q "musr_verify_password" usr/src/cmd/mdm.c; then
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

SPRINTF_COUNT=$(grep -r "sprintf" --include="*.c" | grep -v "snprintf" | wc -l)
if [ "$SPRINTF_COUNT" -eq 0 ]; then
    check_pass "所有 sprintf 已替换"
else
    check_fail "仍有 $SPRINTF_COUNT 处 sprintf 未替换"
fi

# 测试5: 密码哈希检查
echo ""
echo "=== 测试5: 密码哈希检查 ==="
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

# 测试6: 命令注入防护检查
echo ""
echo "=== 测试6: 命令注入防护检查 ==="
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

# 测试7: 键盘驱动检查
echo ""
echo "=== 测试7: 键盘驱动检查 ==="
if grep -q "pic_unmask_irq(1)" sys/src/drivers/keyboard/keyboard.c; then
    check_pass "键盘 IRQ1 已启用"
else
    check_fail "键盘 IRQ1 未启用"
fi

if grep -q "mkrn_kbd_init" sys/src/kernel/kmain.c; then
    check_pass "键盘驱动在内核中初始化"
else
    check_fail "键盘驱动未在内核中初始化"
fi

# 测试8: 鼠标驱动检查
echo ""
echo "=== 测试8: 鼠标驱动检查 ==="
if grep -q "pic_unmask_irq(12)" sys/src/drivers/mouse/mouse.c; then
    check_pass "鼠标 IRQ12 已启用"
else
    check_fail "鼠标 IRQ12 未启用"
fi

if grep -q "mkrn_mouse_init" sys/src/kernel/kmain.c; then
    check_pass "鼠标驱动在内核中初始化"
else
    check_fail "鼠标驱动未在内核中初始化"
fi

# 测试9: MDM图形界面检查
echo ""
echo "=== 测试9: MDM图形界面检查 ==="
if grep -q "draw_login_form" usr/src/cmd/mdm.c; then
    check_pass "MDM 登录界面绘制函数存在"
else
    check_fail "MDM 登录界面绘制函数不存在"
fi

if grep -q "draw_mouse_cursor" usr/src/cmd/mdm.c; then
    check_pass "MDM 鼠标光标绘制函数存在"
else
    check_fail "MDM 鼠标光标绘制函数不存在"
fi

if grep -q "handle_keyboard" usr/src/cmd/mdm.c && grep -q "handle_mouse" usr/src/cmd/mdm.c; then
    check_pass "MDM 事件处理函数存在"
else
    check_fail "MDM 事件处理函数不存在"
fi

# 测试10: QEMU启动测试
echo ""
echo "=== 测试10: QEMU启动测试 ==="
timeout 20 qemu-system-i386 -cdrom output/m4kk1_0.0.1_build1-alpha1.iso \
    -nographic -serial mon:stdio -no-reboot -display none 2>&1 | \
    tee /tmp/qemu_test.log &
QEMU_PID=$!

# 等待系统启动
sleep 15

# 检查键盘初始化
if grep -q "Keyboard driver initialized" /tmp/qemu_test.log; then
    check_pass "键盘驱动初始化成功"
else
    check_fail "键盘驱动初始化失败"
fi

# 检查鼠标初始化
if grep -q "PS/2 mouse driver initialized" /tmp/qemu_test.log; then
    check_pass "鼠标驱动初始化成功"
else
    check_fail "鼠标驱动初始化失败"
fi

# 检查MDM启动
if grep -q "Starting MDM" /tmp/qemu_test.log; then
    check_pass "MDM启动成功"
else
    check_fail "MDM启动失败"
fi

# 检查MDM事件循环
if grep -q "entering event loop" /tmp/qemu_test.log; then
    check_pass "MDM进入事件循环"
else
    check_fail "MDM未进入事件循环"
fi

# 清理QEMU进程
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

# 测试11: 关键文件检查
echo ""
echo "=== 测试11: 关键文件检查 ==="
KEY_FILES=(
    "m4sh/m4sh.h"
    "m4sh/lib/pwd.c"
    "usr/src/cmd/login.c"
    "usr/src/cmd/mdm.c"
    "usr/src/lib/libgui.c"
    "init/init.c"
    "sys/src/kernel/kmain.c"
    "sys/src/drivers/keyboard/keyboard.c"
    "sys/src/drivers/mouse/mouse.c"
)

for file in "${KEY_FILES[@]}"; do
    if [ -f "$file" ]; then
        check_pass "$file 存在"
    else
        check_fail "$file 不存在"
    fi
done

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
