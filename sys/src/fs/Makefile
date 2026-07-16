# M4KK1文件系统Makefile
# 构建文件系统

# 源代码文件
fs_src := $(wildcard *.c */*.c)

# 目标文件名
FS_LIB := fs.a

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 文件系统对象文件
fs_y := $(patsubst %.c,%.o,$(filter %.c,$(fs_src)))

# 默认目标
.PHONY: all
all: $(FS_LIB)

# 构建静态库
$(FS_LIB): $(fs_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(fs_y) $(FS_LIB)

# 安装目标
.PHONY: install
install: $(FS_LIB)
	@echo "安装文件系统库..."