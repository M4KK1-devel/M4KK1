# M4KK1初始化代码Makefile
# 构建内核初始化代码

# 源代码文件
init_src := $(wildcard *.asm */*.asm *.c */*.c)

# 目标文件名
INIT_OBJS := init.o

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 初始化对象文件
init_y := $(patsubst %.asm,%.o,$(filter %.asm,$(init_src)))
init_y += $(patsubst %.c,%.o,$(filter %.c,$(init_src)))

# 默认目标
.PHONY: all
all: $(INIT_OBJS)

# 构建目标
$(INIT_OBJS): $(init_y)
	$(LD) $(LDFLAGS) -r -o $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(init_y) $(INIT_OBJS)

# 安装目标
.PHONY: install
install: $(INIT_OBJS)
	@echo "安装初始化代码..."