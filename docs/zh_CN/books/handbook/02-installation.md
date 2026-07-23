# 第 2 章 安装与运行

## 2.1 环境要求

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| GCC | 支持 `-m32` | 编译 C 代码 |
| NASM | ≥ 2.0 | 汇编入口和 GDT/IDT |
| GNU ld | 支持 `-m elf_i386` | 链接内核和 ELF |
| grub-mkrescue | GRUB 2.0+ | 创建可引导 ISO |
| QEMU | ≥ 4.0 | 模拟运行和测试 |
| xxd | 任意 | 二进制到 C 数组转换 |

### Ubuntu/Debian 安装

```bash
sudo apt install build-essential nasm grub-pc-bin grub-common \
                 xorriso qemu-system-x86 xxd
```

## 2.2 构建

```bash
git clone https://github.com/makkichan947/M4KK1.git
cd M4KK1
bash tools/build/build_krn.sh
```

构建过程：
1. 读取 `VERSION` 文件
2. 编译内核 C 源文件（`-m32 -ffreestanding -nostdlib`）
3. 汇编 NASM 入口文件
4. 构建 M4SH 用户空间 ELF
5. 构建 Login ELF 和 Init ELF
6. 通过 `xxd -i` 将 ELF 嵌入内核
7. 链接生成 `m4kk1.krn`
8. 通过 `grub-mkrescue` 创建可引导 ISO

## 2.3 在 QEMU 中运行

```bash
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio
```

> 虽然 QEMU 使用 x86_64 系统配置，但内核本身是 32-bit i386。

### 调试模式

```bash
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio -d cpu_reset
```

QEMU 监控快捷键：`Ctrl+Alt+2`

## 2.4 启动后操作

系统启动后显示登录提示：

```
M4KK1 login: root
Password: 123456
```

默认用户：`root`，默认密码：`123456`

## 2.5 输出产物

| 产物 | 路径 | 大小 |
|------|------|------|
| 内核二进制 | `m4kk1.krn` | ~680 KB |
| M4SH ELF | `m4sh/m4sh.elf` | ~290 KB |
| ISO | `iso/m4kk1_*.iso` | ~33 MB |

## 2.6 常见问题

### `-m32` 不支持

```bash
sudo apt install gcc-multilib
```

### QEMU 无串口输出

确保使用 `-serial stdio` 参数。

### 内核启动时 Panic

检查 `sys/src/init/linker.ld` 中的内存布局，确保堆和栈不重叠。
