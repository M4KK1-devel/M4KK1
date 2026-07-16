# M4KK1 Security Framework Makefile
# 构建安全增强框架

# 源代码文件
security_src := $(wildcard *.c */*.c)

# 目标文件名
SECURITY_LIB := security.a

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 安全框架对象文件
security_y := $(patsubst %.c,%.o,$(filter %.c,$(security_src)))

# 默认目标
.PHONY: all
all: $(SECURITY_LIB)

# 构建静态库
$(SECURITY_LIB): $(security_y)
	$(AR) rcs $@ $^

# 清理目标
.PHONY: clean
clean:
	rm -f $(security_y) $(SECURITY_LIB)

# 安装目标
.PHONY: install
install: $(SECURITY_LIB)
	@echo "安装安全框架库..."