# M4KK1鼠标驱动Makefile
# 构建PS/2和USB鼠标驱动

# 源代码文件
mouse_src := mouse.c

# 目标文件名
MOUSE_LIB := mouse.a

# 包含通用构建规则
include ../../../../scripts/Makefile.include

# 鼠标对象文件
mouse_y := $(patsubst %.c,%.o,$(filter %.c,$(mouse_src)))

# 默认目标
.PHONY: all
all: $(MOUSE_LIB)

# 构建静态库
$(MOUSE_LIB): $(mouse_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(mouse_y) $(MOUSE_LIB)

# 安装目标
.PHONY: install
install: $(MOUSE_LIB)
	@echo "安装鼠标驱动..."