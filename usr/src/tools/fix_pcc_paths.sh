#!/bin/bash
# Fix PCC paths - create wrapper scripts for libexec tools

set -e

PREFIX="$HOME/.local"
LIBEXEC="$PREFIX/libexec"

echo "Creating wrapper scripts in PATH..."

# Create wrapper for cpp
cat > "$PREFIX/bin/i386-pc-m4kk1-cpp" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-cpp "\$@"
EOF
chmod +x "$PREFIX/bin/i386-pc-m4kk1-cpp"

# Create wrapper for ccom
cat > "$PREFIX/bin/i386-pc-m4kk1-ccom" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-ccom "\$@"
EOF
chmod +x "$PREFIX/bin/i386-pc-m4kk1-ccom"

# Create wrapper for cc2
cat > "$PREFIX/bin/i386-pc-m4kk1-cc2" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-cc2 "\$@"
EOF
chmod +x "$PREFIX/bin/i386-pc-m4kk1-cc2"

echo "Wrapper scripts created successfully!"
echo ""
echo "Testing PCC..."
export PATH="$PREFIX/bin:$PATH"
cd /tmp
cat > test.c << 'TESTEOF'
int main() { return 0; }
TESTEOF

i386-pc-m4kk1-pcc -v test.c -o test 2>&1
if [ $? -eq 0 ]; then
    echo "PCC test PASSED!"
else
    echo "PCC test FAILED!"
fi
