# M4KK1 ARM64 Architecture Makefile
# 独特的M4KK1 ARM64架构构建

# 源代码文件
arch_src := $(wildcard boot/*.asm kernel/*.asm mm/*.asm)
arch_src += $(wildcard boot/*.c kernel/*.c mm/*.c)

# 目标文件名
ARCH_LIB := arch_arm64.a

# 包含通用构建规则
include ../../scripts/Makefile.include

# 架构对象文件
arch_y := $(patsubst %.asm,%.o,$(filter %.asm,$(arch_src)))
arch_y += $(patsubst %.c,%.o,$(filter %.c,$(arch_src)))

# 汇编标志
ASFLAGS := -f elf64

# C编译标志
CFLAGS := -Wall -Wextra -O2 -g -ffreestanding -nostdlib
CFLAGS += -I$(srctree)/sys/src/include -I$(srctree)/sys/src/arch/arm64/include
CFLAGS += -DKERNEL_VERSION=\"$(KERNEL_VERSION)\"

# 默认目标
.PHONY: all
all: $(ARCH_LIB)

# 构建静态库
$(ARCH_LIB): $(arch_y)
	$(AR) rcs $@ $^

# 汇编文件编译规则
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# C文件编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
.PHONY: clean
clean:
	rm -f $(arch_y) $(ARCH_LIB)

# 安装目标
.PHONY: install
install: $(ARCH_LIB)
	@echo "安装ARM64架构相关库..."