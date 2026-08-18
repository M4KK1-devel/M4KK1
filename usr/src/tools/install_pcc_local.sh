#!/bin/bash
# PCC安装脚本 for M4KK1 - 用户目录版本

set -e

PCC_DIR="$(cd "$(dirname "$0")" && pwd)/pcc-20220331"
PREFIX="$HOME/.local"

echo "Installing PCC to $PREFIX..."

# 创建目录
mkdir -p "$PREFIX/bin"
mkdir -p "$PREFIX/libexec"
mkdir -p "$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/lib"
mkdir -p "$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/include"

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

echo "PCC installation completed!"
echo ""
echo "Verifying installation..."
ls -lh "$PREFIX/bin/i386-pc-m4kk1-pcc"
ls -lh "$PREFIX/libexec/i386-pc-m4kk1-ccom"
ls -lh "$PREFIX/libexec/i386-pc-m4kk1-cpp"

echo ""
echo "Please add the following to your ~/.bashrc:"
echo "export PATH=\$HOME/.local/bin:\$PATH"
echo ""
echo "Then run: source ~/.bashrc"
