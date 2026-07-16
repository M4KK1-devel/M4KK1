# M4KK1包管理系统Makefile
# 构建PkgMgr包管理器

# 编译器和标志
CC := gcc
CFLAGS := -Wall -Wextra -O2 -g -Iinclude -I../include
LDFLAGS := -lstdc++

# 源代码文件
pkg_src := $(wildcard src/*.c)

# 目标文件名
PKG_TOOL := pkgmgr

# 包管理器对象文件
pkg_y := $(patsubst %.c,%.o,$(pkg_src))

# 默认目标
.PHONY: all
all: $(PKG_TOOL)

# 构建包管理器
$(PKG_TOOL): $(pkg_y)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
.PHONY: clean
clean:
	rm -f $(pkg_y) $(PKG_TOOL)

# 安装目标
.PHONY: install
install: $(PKG_TOOL)
	@echo "安装包管理器到系统..."