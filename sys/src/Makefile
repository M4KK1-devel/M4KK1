# M4KK1 Kernel Multi-Architecture Makefile
# 多架构内核构建脚本

# 导出所有变量
export

# 包含通用构建规则
include ../../scripts/Makefile.include

# 网络协议栈 (单独构建，不在此处包含)
# include net/Makefile

# 支持的架构
SUPPORTED_ARCHES := x86_64 x86 arm arm64 powerpc riscv

# 默认架构
DEFAULT_ARCH := x86_64

# 默认目标
.PHONY: all clean install help
all: $(SUPPORTED_ARCHES)

# 帮助目标
help:
	@echo "M4KK1多架构内核构建系统"
	@echo "支持的架构: $(SUPPORTED_ARCHES)"
	@echo ""
	@echo "可用目标:"
	@echo "  all              - 构建所有架构"
	@echo "  clean            - 清理所有架构"
	@echo "  install          - 安装所有架构"
	@echo "  help             - 显示此帮助信息"
	@for arch in $(SUPPORTED_ARCHES); do \
		echo "  $$arch           - 构建$$arch架构"; \
		echo "  $$arch-clean     - 清理$$arch架构"; \
		echo "  $$arch-install   - 安装$$arch架构"; \
	done

# 构建所有架构
.PHONY: $(SUPPORTED_ARCHES)
$(SUPPORTED_ARCHES):
	@echo "构建M4KK1内核 ($@)..."
	$(MAKE) -C ../../arch/$@ all

# 构建默认架构
.PHONY: kernel
kernel: $(DEFAULT_ARCH)

# 清理所有架构
.PHONY: clean
clean:
	@echo "清理M4KK1内核构建文件..."
	@for arch in $(SUPPORTED_ARCHES); do \
		$(MAKE) -C ../../arch/$$arch clean; \
	done

# 安装所有架构
.PHONY: install
install: all
	@echo "安装M4KK1内核..."
	@for arch in $(SUPPORTED_ARCHES); do \
		$(MAKE) -C ../../arch/$$arch install; \
	done

# 架构特定目标
.PHONY: $(addsuffix -clean, $(SUPPORTED_ARCHES))
$(addsuffix -clean, $(SUPPORTED_ARCHES)):
	arch=$(patsubst %-clean,%,$@); \
	$(MAKE) -C ../../arch/$$arch clean

.PHONY: $(addsuffix -install, $(SUPPORTED_ARCHES))
$(addsuffix -install, $(SUPPORTED_ARCHES)):
	arch=$(patsubst %-install,%,$@); \
	$(MAKE) -C ../../arch/$$arch install