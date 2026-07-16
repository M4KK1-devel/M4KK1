# M4KK1键盘驱动Makefile
# 构建PS/2和USB键盘驱动

# 源代码文件
keyboard_src := keyboard.c

# 目标文件名
KEYBOARD_LIB := keyboard.a

# 包含通用构建规则
include ../../../../scripts/Makefile.include

# 键盘对象文件
keyboard_y := $(patsubst %.c,%.o,$(filter %.c,$(keyboard_src)))

# 默认目标
.PHONY: all
all: $(KEYBOARD_LIB)

# 构建静态库
$(KEYBOARD_LIB): $(keyboard_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(keyboard_y) $(KEYBOARD_LIB)

# 安装目标
.PHONY: install
install: $(KEYBOARD_LIB)
	@echo "安装键盘驱动..."