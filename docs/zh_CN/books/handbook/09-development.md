# 第 9 章 开发

## 9.1 开发环境

### 必需工具

| 工具 | 用途 |
|------|------|
| GCC（支持 `-m32`） | 编译 C 代码 |
| NASM | 汇编入口和 GDT/IDT |
| GNU ld（支持 `-m elf_i386`） | 链接内核和 ELF |
| grub-mkrescue | 创建可引导 ISO |
| QEMU | 模拟运行和测试 |
| xxd | 二进制到 C 数组转换 |

### 安装 (Debian/Ubuntu)

```bash
sudo apt install build-essential nasm grub-pc-bin grub-common \
                 xorriso qemu-system-x86 xxd
```

## 9.2 构建系统

构建入口: `tools/build/build_krn.sh`

### 编译标志

内核:
```bash
CFLAGS="-Wall -Wextra -O2 -g -ffreestanding -nostdlib -nostdinc"
CFLAGS="$CFLAGS -m32 -mno-sse -mno-sse2 -std=gnu99 -fno-stack-protector"
```

用户空间:
```bash
M4SH_CFLAGS="-ffreestanding -nostdlib -nostdinc -m32 -std=gnu99"
```

### 链接

内核加载地址: `0x100000` (1 MB)
用户空间加载地址: `0x800000`

## 9.3 日常开发循环

```bash
# 1. 修改代码
vim sys/src/kernel/kmain.c

# 2. 构建
bash tools/build/build_krn.sh

# 3. 测试 (QEMU)
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio
```

## 9.4 添加新命令

1. 在 `m4sh/coreutils/` 下创建 `.c` 文件
2. 实现 `void musr_cmd_xxx(int ac, char **av)`
3. 在 `m4sh/m4sh.h` 中添加前向声明
4. 在 `m4sh/core/m4sh_main.c` 的命令表中添加条目

### 示例

```c
// m4sh/coreutils/hello.c
#include "../m4sh.h"

void musr_cmd_hello(int ac, char **av)
{
    out_puts("Hello, M4KK1!\n");
}
```

命令表项: `{"hello", musr_cmd_hello, "Say hello"}`

## 9.5 调试

### 内核调试

```c
mkrn_console_write("debug message\n");
mkrn_console_write_hex(value);
mkrn_console_write_dec(number);
```

### 日志宏

```c
M4K_LOG_INFO("message");
M4K_LOG_WARN("warning");
M4K_LOG_ERROR("error");
```

### 内核 Panic

```c
mkrn_panic("Something went wrong");  // 停止所有执行
```

## 9.6 测试

### QEMU 自动化测试

```bash
bash tools/build/build_krn.sh
python3 /tmp/qemu_acceptance.py
```

### 手动验证清单

每次修改后建议验证:
- [ ] 构建成功（无错误）
- [ ] 内核启动到登录提示
- [ ] 使用 root/123456 成功登录
- [ ] 基本命令 (help, pwd, ls, date)
- [ ] Shell 引号处理
- [ ] 命令操作符 (; && ||)
- [ ] 修改的关键功能

## 9.7 版本管理

版本号格式: **MAJOR.MINOR.PATCH**

ISO 命名:
```
m4kk1_{MAJOR}.{MINOR}.{PATCH}_build{BUILD}-{CLASSIFIER}.iso
```

分类器: `alpha` → `beta` → `rc` → (正式发布)

## 9.8 编码规范

遵循 4P1 编码风格:

- 命名: `mkrn_`（内核）、`musr_`（用户空间）、`m4k_`（系统调用 ABI）
- 缩进: 4 空格，K&R 括号风格
- 行宽: 80 字符
- 指针嵌套: 最多 2 级（不允许 `***`）
- 函数长度: 最多 100 行
- 注释: `/* */` 格式，不使用 `//`
