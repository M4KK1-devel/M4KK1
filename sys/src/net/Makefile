# M4KK1网络协议栈Makefile
# 构建TCP/IP协议栈

# 源代码文件
net_src := $(wildcard *.c */*.c)

# 目标文件名
NET_LIB := net.a

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 网络协议栈对象文件
net_y := $(patsubst %.c,%.o,$(filter %.c,$(net_src)))

# 默认目标
.PHONY: all
all: $(NET_LIB)

# 构建静态库
$(NET_LIB): $(net_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(net_y) $(NET_LIB)

# 安装目标
.PHONY: install
install: $(NET_LIB)
	@echo "安装网络协议栈库..."