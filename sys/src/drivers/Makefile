# M4KK1设备驱动Makefile
# 构建设备驱动程序

# 源代码文件
drivers_src := console.c

# 子目录
subdirs := keyboard mouse

# 目标文件名
DRIVERS_LIB := drivers.a

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 驱动程序对象文件
drivers_y := $(patsubst %.c,%.o,$(filter %.c,$(drivers_src)))

# 构建子目录
.PHONY: subdirs $(subdirs)
subdirs: $(subdirs)

$(subdirs):
	$(MAKE) -C $@ all

# 默认目标
.PHONY: all
all: $(DRIVERS_LIB)

# 构建静态库
$(DRIVERS_LIB): $(drivers_y) subdirs
	$(AR) rcs $@ $(drivers_y) keyboard/keyboard.a mouse/mouse.a

# 清理目标
.PHONY: clean
clean: subdirs_clean
	rm -f $(drivers_y) $(DRIVERS_LIB)

.PHONY: subdirs_clean
subdirs_clean:
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir clean; \
	done

# 安装目标
.PHONY: install
install: $(DRIVERS_LIB) subdirs_install
	@echo "安装设备驱动库..."

.PHONY: subdirs_install
subdirs_install:
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir install; \
	done