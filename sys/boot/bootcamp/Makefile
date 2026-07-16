# M4KK1 Bootcamp Makefile
# 构建引导加载程序

# 工具链
NASM = nasm
DD = dd
QEMU = qemu-system-i386

# 标志
NASM_FLAGS = -f bin
QEMU_FLAGS = -drive file=$(DISK_IMG),format=raw,index=0,media=disk

# 包含通用构建规则
include ../../../scripts/Makefile.include

# 文件
STAGE1_SRC = stage1.asm
STAGE2_SRC = stage2.asm
STAGE1_BIN = stage1.bin
STAGE2_BIN = stage2.bin
DISK_IMG = m4kk1.img

# 默认目标
.PHONY: all clean run

all: $(DISK_IMG)

# 构建第一阶段引导加载程序
$(STAGE1_BIN): $(STAGE1_SRC)
	$(NASM) $(NASM_FLAGS) $< -o $@

# 构建第二阶段引导加载程序
$(STAGE2_BIN): $(STAGE2_SRC)
	$(NASM) $(NASM_FLAGS) $< -o $@

# 创建磁盘映像
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN)
	# 创建1.44MB软盘映像
	$(DD) if=/dev/zero of=$(DISK_IMG) bs=1024 count=1440
	# 写入第一阶段引导加载程序（第一个扇区）
	$(DD) if=$(STAGE1_BIN) of=$(DISK_IMG) bs=512 count=1 conv=notrunc
	# 写入第二阶段引导加载程序（从第二个扇区开始）
	$(DD) if=$(STAGE2_BIN) of=$(DISK_IMG) bs=512 seek=1 conv=notrunc

# 运行模拟器
run: $(DISK_IMG)
	$(QEMU) $(QEMU_FLAGS)

# 调试运行
debug: $(DISK_IMG)
	$(QEMU) $(QEMU_FLAGS) -s -S

# 清理构建文件
clean:
	rm -f $(STAGE1_BIN) $(STAGE2_BIN) $(DISK_IMG)

# 安装引导加载程序（需要root权限）
install: $(DISK_IMG)
	sudo $(DD) if=$(DISK_IMG) of=/dev/sda bs=512 count=1 conv=notrunc

# 帮助信息
help:
	@echo "M4KK1 Bootcamp Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build bootcamp disk image"
	@echo "  run       - Run in QEMU"
	@echo "  debug     - Run in QEMU with debugging"
	@echo "  clean     - Clean build files"
	@echo "  install   - Install to /dev/sda (DANGEROUS!)"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Note: 'install' target is dangerous and will overwrite your boot sector!"