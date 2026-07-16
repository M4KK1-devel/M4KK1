# M4KK1 4P1 - Makefile
# Description: Top-level build system for M4KK1 Multi-Architecture OS.
#
# Copyright (c) 2026 Yaku Makki
# SPDX-License-Identifier: 4P1-Custom

# Default architecture
DEFAULT_ARCH := x86_64

# Supported architectures
SUPPORTED_ARCHES := x86_64 x86 arm arm64 powerpc riscv

# Export variables
export

# Include common build rules
include scripts/Makefile.include

# Default target: show help
.PHONY: all
all: help

# Help target
.PHONY: help
help:
	@echo "========================================"
	@echo "M4KK1 Multi-Architecture Build System"
	@echo "========================================"
	@echo ""
	@echo "Quick build commands:"
	@echo "  make x86_64      - Build x86_64 architecture"
	@echo "  make arm64       - Build ARM64 architecture"
	@echo "  make riscv       - Build RISC-V architecture"
	@echo "  make multarch    - Build all architectures"
	@echo "  make iso         - Create multi-architecture ISO"
	@echo "  make clean       - Clean build files"
	@echo ""
	@echo "Advanced options:"
	@echo "  cd tools/build && make help"
	@echo ""
	@echo "Supported architectures: x86_64, x86, arm, arm64, powerpc, riscv"
	@echo "========================================"

# List supported architectures
.PHONY: list-arch
list-arch:
	@echo "Supported architectures:"
	@for arch in $(SUPPORTED_ARCHES); do \
		echo "  $$arch"; \
	done

# Architecture specific targets - delegate to tools/build
.PHONY: x86_64 x86 arm arm64 powerpc riscv multarch
x86_64 x86 arm arm64 powerpc riscv multarch:
	@$(MAKE) -C tools/build $@

# Clean target
.PHONY: clean
clean:
	@$(MAKE) -C tools/build clean

# ISO image build target
.PHONY: iso
iso:
	@./scripts/build_iso.sh

# Test target
.PHONY: test
test:
	@$(MAKE) -C test all

# Compiler targets
.PHONY: compilers
compilers:
	@$(MAKE) -C usr/opt/langcc all
	@$(MAKE) -C usr/bin/MLang all

# MLang compiler target
.PHONY: mlang
mlang:
	@$(MAKE) -C usr/bin/MLang all

# Package management target
.PHONY: packages
packages:
	@$(MAKE) -C pkg all

# Install target
.PHONY: install
install:
	@./scripts/install.sh

# Other targets
.PHONY: kernel install list-arch
kernel install list-arch:
	@$(MAKE) -C tools/build $@
