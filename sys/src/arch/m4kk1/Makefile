# M4KK1架构相关Makefile
# 构建M4KK1架构相关代码

# 源代码文件
arch_src := $(wildcard *.asm) $(wildcard *.c)

# 目标文件名
ARCH_LIB := arch_m4kk1.a

# 包含通用构建规则
include ../../scripts/Makefile.include

# 架构对象文件
arch_y := $(patsubst %.asm,%.o,$(filter %.asm,$(arch_src)))
arch_y += $(patsubst %.c,%.o,$(filter %.c,$(arch_src)))

# 默认目标
.PHONY: all
all: $(ARCH_LIB)

# 构建静态库
$(ARCH_LIB): $(arch_y)
	$(AR) rcs $@ $^

# 汇编文件优先于C文件编译
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
	@echo "安装架构相关库..."