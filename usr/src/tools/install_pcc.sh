#!/bin/bash
# PCC安装脚本 for M4KK1

set -e

PCC_DIR="$(cd "$(dirname "$0")" && pwd)/pcc-20220331"
PREFIX="/usr/local"

echo "Installing PCC to $PREFIX..."

# 创建目录
mkdir -p "$PREFIX/bin"
mkdir -p "$PREFIX/libexec"
mkdir -p "$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/lib"
mkdir -p "$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/include"
mkdir -p "$PREFIX/share/man/man1"

# 安装编译器驱动程序
echo "Installing compiler drivers..."
cp "$PCC_DIR/cc/cc/cc" "$PREFIX/bin/i386-pc-m4kk1-pcc"
chmod +x "$PREFIX/bin/i386-pc-m4kk1-pcc"

# 创建别名
ln -sf i386-pc-m4kk1-pcc "$PREFIX/bin/i386-pc-m4kk1-pcpp"
ln -sf i386-pc-m4kk1-pcc "$PREFIX/bin/i386-pc-m4kk1-p++"

# 安装编译器后端
echo "Installing compiler backend..."
cp "$PCC_DIR/cc/ccom/i386-pc-m4kk1-ccom" "$PREFIX/libexec/"
chmod +x "$PREFIX/libexec/i386-pc-m4kk1-ccom"

cp "$PCC_DIR/cc/ccom/i386-pc-m4kk1-cc2" "$PREFIX/libexec/"
chmod +x "$PREFIX/libexec/i386-pc-m4kk1-cc2"

# 安装预处理器
echo "Installing preprocessor..."
cp "$PCC_DIR/cc/cpp/i386-pc-m4kk1-cpp" "$PREFIX/libexec/"
chmod +x "$PREFIX/libexec/i386-pc-m4kk1-cpp"

# 安装手册页（如果存在）
if [ -f "$PCC_DIR/cc/cc/cc.1" ]; then
    cp "$PCC_DIR/cc/cc/cc.1" "$PREFIX/share/man/man1/pcc.1"
fi

if [ -f "$PCC_DIR/cc/ccom/ccom.1" ]; then
    cp "$PCC_DIR/cc/ccom/ccom.1" "$PREFIX/share/man/man1/i386-pc-m4kk1-ccom.1"
fi

if [ -f "$PCC_DIR/cc/cpp/cpp.1" ]; then
    cp "$PCC_DIR/cc/cpp/cpp.1" "$PREFIX/share/man/man1/i386-pc-m4kk1-cpp.1"
fi

echo "PCC installation completed!"
echo ""
echo "Verifying installation..."
ls -lh "$PREFIX/bin/i386-pc-m4kk1-pcc"
ls -lh "$PREFIX/libexec/i386-pc-m4kk1-ccom"
ls -lh "$PREFIX/libexec/i386-pc-m4kk1-cpp"

echo ""
echo "Testing compiler..."
i386-pc-m4kk1-pcc --version || echo "Compiler installed but may need PATH update"
