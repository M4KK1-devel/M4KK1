#!/bin/bash
# qemu_runner.sh — 启动 QEMU 并注入测试
# 用法: ./qemu_runner.sh [test_script]

ISO="${1:-m4kk1-test.iso}"
QEMU="qemu-system-x86_64"

if [ ! -f "$ISO" ]; then
    echo "错误: ISO 文件 $ISO 不存在"
    echo "请先在项目根目录运行: bash tools/build/build_krn.sh"
    exit 1
fi

echo "=== M4KK1 QEMU 测试运行器 ==="
echo "ISO: $ISO"
echo "启动 QEMU (串口输出到 stdout)..."
echo

"$QEMU" -cdrom "$ISO" -serial stdio -m 256M -no-reboot -nographic 2>&1 | head -200
