# M4KK1 4P1 - Makefile
# Description: Multi-architecture build system Makefile for M4KK1.
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
include ../../scripts/Makefile.include

# Default target
.PHONY: all
all: kernel

# Help target
.PHONY: help
help:
	@echo "M4KK1 Multi-Architecture Build System"
	@echo "Supported architectures: $(SUPPORTED_ARCHES)"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build default architecture ($(DEFAULT_ARCH))"
	@echo "  kernel           - Build kernel"
	@echo "  clean            - Clean build files"
	@echo "  help             - Show this help"
	@echo "  list-arch        - List supported architectures"
	@echo ""
	@echo "Architecture specific targets:"
	@for arch in $(SUPPORTED_ARCHES); do \
		echo "  $$arch           - Build $$arch architecture"; \
		echo "  $$arch-clean     - Clean $$arch architecture"; \
		echo "  $$arch-install   - Install $$arch architecture"; \
	done

# List supported architectures
.PHONY: list-arch
list-arch:
	@echo "Supported architectures:"
	@for arch in $(SUPPORTED_ARCHES); do \
		echo "  $$arch"; \
	done

# Kernel build target
.PHONY: kernel
kernel: ../../scripts/mkversion
	@echo "Building M4KK1 kernel ($(DEFAULT_ARCH))..."
	$(MAKE) -C ../../sys/src arch-$(DEFAULT_ARCH)

# Architecture specific targets
.PHONY: $(SUPPORTED_ARCHES)
$(SUPPORTED_ARCHES):
	@echo "Building M4KK1 kernel ($@)..."
	$(MAKE) -C ../../sys/src arch-$@

# Architecture specific clean targets
.PHONY: $(addsuffix -clean, $(SUPPORTED_ARCHES))
$(addsuffix -clean, $(SUPPORTED_ARCHES)):
	arch=$(patsubst %-clean,%,$@); \
	echo "Cleaning M4KK1 kernel ($$arch)..."; \
	$(MAKE) -C ../../sys/src $$arch-clean

# Architecture specific install targets
.PHONY: $(addsuffix -install, $(SUPPORTED_ARCHES))
$(addsuffix -install, $(SUPPORTED_ARCHES)):
	arch=$(patsubst %-install,%,$@); \
	echo "Installing M4KK1 kernel ($$arch)..."; \
	$(MAKE) -C ../../sys/src $$arch-install

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning M4KK1 build files..."
	$(MAKE) -C ../../sys/src clean
	rm -f ../../scripts/mkversion

# Install target
.PHONY: install
install: kernel
	@echo "Installing M4KK1 kernel..."
	$(MAKE) -C ../../sys/src install

# Version info generation
../../scripts/mkversion: ../../scripts/mkversion.c
	@echo "Generating version info..."
	gcc -Wall -Wextra -O2 -g -I/usr/include -I/usr/local/include -o $@ $<

# Include subdirectory Makefile
include ../../sys/src/Makefile
