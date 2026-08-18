#!/bin/bash
# Install M4KK1 runtime files for PCC

set -e

PREFIX="$HOME/.local"
LIB_DIR="$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/lib"
INCLUDE_DIR="$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/include"
LIBC_DIR="$(cd "$(dirname "$0")" && pwd)/../lib/m4k_libc"

echo "Installing M4KK1 runtime files..."

# Create directories
mkdir -p "$LIB_DIR"
mkdir -p "$INCLUDE_DIR"

# Compile pcc_runtime.c
echo "Compiling pcc_runtime.c..."
cd "$LIBC_DIR"
i386-pc-m4kk1-pcc -Iinclude -c pcc_runtime.c -o pcc_runtime.o

# Create libpcc.a
echo "Creating libpcc.a..."
i386-pc-m4kk1-ar rcs libpcc.a pcc_runtime.o

# Install libraries
echo "Installing libraries..."
cp -v "$LIBC_DIR/libc.a" "$LIB_DIR/"
cp -v "$LIBC_DIR/libpcc.a" "$LIB_DIR/"

# Install runtime objects
echo "Installing runtime objects..."
cp -v "$LIBC_DIR/crt0.o" "$LIB_DIR/"
cp -v "$LIBC_DIR/crti.o" "$LIB_DIR/"
cp -v "$LIBC_DIR/crtn.o" "$LIB_DIR/"

# Install headers
echo "Installing headers..."
cp -v "$LIBC_DIR/include/"*.h "$INCLUDE_DIR/"

echo "Runtime files installed successfully"
echo "Libraries:"
ls -lh "$LIB_DIR"
echo "Headers:"
ls -lh "$INCLUDE_DIR"

echo ""
echo "=== Verifying PCC self-host toolchain ==="
i386-pc-m4kk1-pcc -v 2>&1 | head -1
cat > /tmp/m4kk1_pcc_check.c << 'CEOF'
int main(void) { return 0; }
CEOF
i386-pc-m4kk1-pcc -ffreestanding -nostdlib -nostdinc -I"$INCLUDE_DIR" \
    -c /tmp/m4kk1_pcc_check.c -o /tmp/m4kk1_pcc_check.o \
    && file /tmp/m4kk1_pcc_check.o \
    && echo "PCC self-host toolchain OK" \
    || echo "PCC toolchain check FAILED"
