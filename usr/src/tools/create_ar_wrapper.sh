#!/bin/bash
# Create ar wrapper for M4KK1 cross-compilation

set -e

PREFIX="$HOME/.local/bin"

echo "Creating ar wrapper script..."

# Create wrapper for ar
cat > "$PREFIX/i386-pc-m4kk1-ar" << 'EOF'
#!/bin/bash
# i386-pc-m4kk1-ar wrapper script
exec /usr/sbin/ar "$@"
EOF
chmod +x "$PREFIX/i386-pc-m4kk1-ar"

echo "ar wrapper created successfully"
ls -lh "$PREFIX/i386-pc-m4kk1-ar"
