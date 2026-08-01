#!/bin/bash
# 创建binutils包装脚本 for M4KK1

set -e

PREFIX="$HOME/.local/bin"

echo "Creating binutils wrapper scripts..."

# 创建汇编器包装脚本
cat > "$PREFIX/i386-pc-m4kk1-as" << 'EOF'
#!/bin/bash
# i386-pc-m4kk1-as wrapper script
exec /usr/sbin/as --32 "$@"
EOF
chmod +x "$PREFIX/i386-pc-m4kk1-as"

# 创建链接器包装脚本
cat > "$PREFIX/i386-pc-m4kk1-ld" << 'EOF'
#!/bin/bash
# i386-pc-m4kk1-ld wrapper script
exec /usr/sbin/ld -m elf_i386 "$@"
EOF
chmod +x "$PREFIX/i386-pc-m4kk1-ld"

# 创建strip包装脚本
cat > "$PREFIX/i386-pc-m4kk1-strip" << 'EOF'
#!/bin/bash
# i386-pc-m4kk1-strip wrapper script
exec /usr/sbin/strip "$@"
EOF
chmod +x "$PREFIX/i386-pc-m4kk1-strip"

echo "Binutils wrappers created successfully!"
echo ""
echo "Verifying wrappers..."
ls -lh "$PREFIX/i386-pc-m4kk1-as"
ls -lh "$PREFIX/i386-pc-m4kk1-ld"
ls -lh "$PREFIX/i386-pc-m4kk1-strip"
