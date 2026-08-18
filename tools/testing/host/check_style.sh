#!/bin/bash
# check_style.sh — 检查 C 代码风格
# 使用 sead 工具检查 sys/src/ 下的 C 代码

STYLE_FILE="docs/style/README.md"
if [ ! -f "$STYLE_FILE" ]; then
    echo "WARN: 风格文档 $STYLE_FILE 不存在，跳过风格检查"
    exit 0
fi

echo "=== C 代码风格检查 ==="
errors=0

for f in $(find sys/src -name '*.c' -o -name '*.h' | sort); do
    # 检查 tab 缩进
    if grep -nP '^    ' "$f" > /dev/null 2>&1; then
        echo "WARN: $f 使用了空格缩进 (应为 tab)"
        errors=$((errors + 1))
    fi

    # 检查尾随空格
    if grep -nP '[ \t]+$' "$f" > /dev/null 2>&1; then
        echo "WARN: $f 包含尾随空格"
        errors=$((errors + 1))
    fi

    # 检查缺少标准头部
    head -1 "$f" | grep -q 'M4KK1 4P1'
    if [ $? -ne 0 ]; then
        echo "WARN: $f 缺少标准头部"
        errors=$((errors + 1))
    fi
done

if [ $errors -eq 0 ]; then
    echo "PASS: 所有文件风格正确"
else
    echo "FAIL: 发现 $errors 个风格问题"
fi
exit $((errors > 0 ? 1 : 0))
