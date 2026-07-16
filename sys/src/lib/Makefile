# M4KK1库函数Makefile
# 构建内核库函数

# 源代码文件
lib_src := $(wildcard *.c */*.c)

# 目标文件名
LIB_LIB := lib.a

# 包含通用构建规则
include ../../../../scripts/Makefile.include

# 库函数对象文件
lib_y := $(patsubst %.c,%.o,$(filter %.c,$(lib_src)))

# 默认目标
.PHONY: all
all: $(LIB_LIB)

# 构建静态库
$(LIB_LIB): $(lib_y)
	$(AR) rcs $@ $^

# 内核库函数编译规则
%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIRS) -DKERNEL_VERSION=\"\" -c $< -o $@

# 清理目标
.PHONY: clean
clean:
	rm -f $(lib_y) $(LIB_LIB)

# 安装目标
.PHONY: install
install: $(LIB_LIB)
	@echo "安装库函数..."