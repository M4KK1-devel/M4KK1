# M4KK1内核核心Makefile
# 构建内核核心功能

# 源代码文件
kernel_src := $(wildcard *.c */*.c)

# 目标文件名
KERNEL := m4kk1.kernel

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 内核对象文件
kernel_y := $(patsubst %.c,%.o,$(filter %.c,$(kernel_src)))

# 编译标志
KERNEL_CFLAGS := $(INC_DIRS) -ffreestanding -nostdlib -std=gnu99 -m32 -fno-stack-protector

# 构建目标
.PHONY: all
all: $(kernel_y)

# 构建内核对象文件
%.o: %.c
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# 安装目标
.PHONY: install
install: $(KERNEL)
	@echo "安装内核核心到 boot/"
	cp $(KERNEL) ../../../boot/

# 清理目标
.PHONY: clean
clean:
	@echo "清理内核核心构建文件..."
	rm -f $(KERNEL) *.o