#!/bin/bash
# M4KK1 安全修复验证脚本
# 在WSL中运行：bash security_verify.sh

# 切换到项目根目录
cd /mnt/f/M4KK1 || exit 1

echo "=========================================="
echo "M4KK1 安全修复验证脚本"
echo "=========================================="
echo "工作目录: $(pwd)"
echo ""

PASS=0
FAIL=0

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    PASS=$((PASS+1))
}

check_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    FAIL=$((FAIL+1))
}

echo "=== 阶段A：硬编码后门检查 ==="

# 检查 login.c 中是否还有硬编码密码 123456
if grep -q "123456" usr/src/cmd/login.c; then
    check_fail "login.c 中仍存在硬编码密码 123456"
else
    check_pass "login.c 中硬编码密码已移除"
fi

# 检查 login.c 中是否还有 hardcoded 相关注释
if grep -q "hardcoded" usr/src/cmd/login.c; then
    check_fail "login.c 中仍存在 hardcoded 相关代码"
else
    check_pass "login.c 中硬编码回退逻辑已移除"
fi

echo ""
echo "=== 阶段B：不安全字符串函数检查 ==="

# 检查 .c 文件中是否还有 musr_strcpy（排除 musr_strncpy）
STRCPY_COUNT=$(grep -r "musr_strcpy" --include="*.c" | grep -v "musr_strncpy" | wc -l)
if [ "$STRCPY_COUNT" -eq 0 ]; then
    check_pass "所有 .c 文件中 musr_strcpy 已替换为 musr_strncpy"
else
    check_fail "仍有 $STRCPY_COUNT 处 musr_strcpy 未替换"
    grep -rn "musr_strcpy" --include="*.c" | grep -v "musr_strncpy" || true
fi

# 检查 .c 文件中是否还有 musr_strcat（排除 musr_strncat）
STRCAT_COUNT=$(grep -r "musr_strcat" --include="*.c" | grep -v "musr_strncat" | wc -l)
if [ "$STRCAT_COUNT" -eq 0 ]; then
    check_pass "所有 .c 文件中 musr_strcat 已替换为 musr_strncat"
else
    check_fail "仍有 $STRCAT_COUNT 处 musr_strcat 未替换"
    grep -rn "musr_strcat" --include="*.c" | grep -v "musr_strncat" || true
fi

# 检查 m4sh.h 中是否定义了安全函数
if grep -q "musr_strncpy" m4sh/m4sh.h && grep -q "musr_strncat" m4sh/m4sh.h; then
    check_pass "m4sh.h 中已定义 musr_strncpy 和 musr_strncat"
else
    check_fail "m4sh.h 中缺少安全字符串函数定义"
fi

echo ""
echo "=== 阶段C：sprintf 检查 ==="

# 检查 .c 文件中是否还有 sprintf（排除 snprintf）
SPRINTF_COUNT=$(grep -r "sprintf" --include="*.c" | grep -v "snprintf" | wc -l)
if [ "$SPRINTF_COUNT" -eq 0 ]; then
    check_pass "所有 .c 文件中 sprintf 已替换为 snprintf"
else
    check_fail "仍有 $SPRINTF_COUNT 处 sprintf 未替换"
    grep -rn "sprintf" --include="*.c" | grep -v "snprintf" || true
fi

echo ""
echo "=== 阶段D：命令注入防护检查 ==="

# 检查 batch.c 中是否有 validate_command_input 函数
if grep -q "validate_command_input" usr/src/cmd/batch.c; then
    check_pass "batch.c 中已添加命令注入防护"
else
    check_fail "batch.c 中缺少命令注入防护"
fi

# 检查 at.c 中是否有 validate_command_input 函数
if grep -q "validate_command_input" usr/src/cmd/at.c; then
    check_pass "at.c 中已添加命令注入防护"
else
    check_fail "at.c 中缺少命令注入防护"
fi

# 检查是否过滤了危险字符
if grep -q "dangerous.*;" usr/src/cmd/batch.c && grep -q "dangerous.*;" usr/src/cmd/at.c; then
    check_pass "batch.c 和 at.c 中已定义危险字符列表"
else
    check_fail "危险字符过滤不完整"
fi

echo ""
echo "=== 阶段E：密码哈希增强检查 ==="

# 检查 pwd.c 中是否实现了 SHA-256
if grep -q "sha256_ctx_t" m4sh/lib/pwd.c && grep -q "sha256_init" m4sh/lib/pwd.c; then
    check_pass "pwd.c 中已实现 SHA-256 哈希算法"
else
    check_fail "pwd.c 中缺少 SHA-256 实现"
fi

# 检查 pwd.c 中是否有 musr_hash_password 函数
if grep -q "musr_hash_password" m4sh/lib/pwd.c; then
    check_pass "pwd.c 中已定义 musr_hash_password 函数"
else
    check_fail "pwd.c 中缺少 musr_hash_password 函数"
fi

# 检查 pwd.c 中是否有加盐逻辑
if grep -q "SALT_SIZE" m4sh/lib/pwd.c; then
    check_pass "pwd.c 中已实现加盐逻辑"
else
    check_fail "pwd.c 中缺少加盐逻辑"
fi

# 检查 musr_verify_password 是否使用哈希比较
if grep -q "musr_verify_password" m4sh/lib/pwd.c && grep -A 20 "musr_verify_password" m4sh/lib/pwd.c | grep -q "hash_with_salt"; then
    check_pass "musr_verify_password 已使用哈希比较"
else
    check_fail "musr_verify_password 未使用哈希比较"
fi

echo ""
echo "=========================================="
echo "验证结果汇总"
echo "=========================================="
echo -e "通过: ${GREEN}$PASS${NC}"
echo -e "失败: ${RED}$FAIL${NC}"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}所有安全检查通过！${NC}"
    exit 0
else
    echo -e "${RED}存在 $FAIL 项安全检查未通过，请检查上述输出。${NC}"
    exit 1
fi
