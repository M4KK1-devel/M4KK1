#!/bin/bash
# Fix PCC components - create wrapper scripts for cpp, ccom, and cc2

set -e

PREFIX="$HOME/.local"
LIBEXEC="$PREFIX/libexec"
BIN="$PREFIX/bin"

echo "Creating wrapper scripts for PCC components..."

# Create wrapper for cpp
cat > "$BIN/i386-pc-m4kk1-cpp" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-cpp "\$@"
EOF
chmod +x "$BIN/i386-pc-m4kk1-cpp"

# Create wrapper for ccom
cat > "$BIN/i386-pc-m4kk1-ccom" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-ccom "\$@"
EOF
chmod +x "$BIN/i386-pc-m4kk1-ccom"

# Create wrapper for cc2 (even though it's empty for i386)
cat > "$BIN/i386-pc-m4kk1-cc2" << EOF
#!/bin/bash
exec $LIBEXEC/i386-pc-m4kk1-cc2 "\$@"
EOF
chmod +x "$BIN/i386-pc-m4kk1-cc2"

echo "PCC component wrappers created successfully"
echo "Verifying installation..."
ls -lh "$BIN/i386-pc-m4kk1-cpp"
ls -lh "$BIN/i386-pc-m4kk1-ccom"
ls -lh "$BIN/i386-pc-m4kk1-cc2"
