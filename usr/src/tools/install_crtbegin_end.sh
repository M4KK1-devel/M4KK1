#!/bin/bash
# Install crtbegin.o and crtend.o for PCC

set -e

export PATH="$HOME/.local/bin:$PATH"

LIBC_DIR="/mnt/f/M4KK1/usr/src/lib/m4k_libc"
PREFIX="$HOME/.local"
LIB_DIR="$PREFIX/lib/pcc/i386-pc-m4kk1/1.2.0.DEVEL/lib"

echo "Installing crtbegin.o and crtend.o..."

cd "$LIBC_DIR"

# Compile crtbegin.s
echo "Compiling crtbegin.s..."
i386-pc-m4kk1-as crtbegin.s -o crtbegin.o

# Compile crtend.s
echo "Compiling crtend.s..."
i386-pc-m4kk1-as crtend.s -o crtend.o

# Install to PCC lib directory
echo "Installing to $LIB_DIR..."
cp -v crtbegin.o "$LIB_DIR/"
cp -v crtend.o "$LIB_DIR/"

echo "crtbegin.o and crtend.o installed successfully"
ls -lh "$LIB_DIR"/crt*.o
