# M4KK1内存管理Makefile
# 构建内存管理和交换系统

# 源代码文件
mm_src := $(wildcard *.c */*.c)

# 目标文件名
MM_LIB := mm.a

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 内存管理对象文件
mm_y := $(patsubst %.c,%.o,$(filter %.c,$(mm_src)))

# 默认目标
.PHONY: all
all: $(MM_LIB)

# 构建静态库
$(MM_LIB): $(mm_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(mm_y) $(MM_LIB)

# 安装目标
.PHONY: install
install: $(MM_LIB)
	@echo "安装内存管理库..."