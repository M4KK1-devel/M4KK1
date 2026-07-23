# M4KK1 操作系统文档中心

**M4KK1** — 基于 4P1 独立标准的 i386 操作系统，从零开发，采用 freestanding C + NASM 汇编。

## 快速启动

```bash
git clone https://github.com/makkichan947/M4KK1.git
cd M4KK1
bash tools/build/build_krn.sh
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio
```

## 文档导航

| 路径 | 语言 | 内容 |
|------|------|------|
| [`zh_CN/books/handbook/`](zh_CN/books/handbook/) | 简体中文 | 完整系统手册（9 章） |
| [`zh_CN/articles/`](zh_CN/articles/) | 简体中文 | 专题文章 |
| [`zh_CN/man/`](zh_CN/man/) | 简体中文 | BSD 格式手册页 |
| [`en_US/books/handbook/`](en_US/books/handbook/) | English | Handbook (TODO) |
| [`en_US/articles/`](en_US/articles/) | English | Articles (TODO) |
| [`en_US/man/`](en_US/man/) | English | Man pages |
| [`share/examples/`](share/examples/) | — | 配置文件模板 |

## 关键概念

- **内核**: Y4KU，宏内核，i386 保护模式，GRUB 引导
- **用户空间**: M4SH 单进程 Shell（PID 1），40+ 内置命令
- **文件系统**: YAFS — COW B+Tree RAM 磁盘（16 MB）
- **文档标准**: BSD 风格手册页 + i18n（zh_CN / en_US）
- **API 标准**: 4P1 — 不兼容 POSIX，无 `errno`，无 MMU

---

# M4KK1 Documentation Center

**M4KK1** — An i386 operating system built from scratch under the 4P1 independent standard, written in freestanding C and NASM assembly.

## Quick Start

```bash
git clone https://github.com/makkichan947/M4KK1.git
cd M4KK1
bash tools/build/build_krn.sh
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio
```

## Doc Navigation

| Path | Lang | Content |
|------|------|---------|
| `en_US/man/` | English | BSD-style man pages |
| `zh_CN/books/handbook/` | Chinese | Full system handbook (9 chapters) |
| `zh_CN/articles/` | Chinese | Feature articles |
| `zh_CN/man/` | Chinese | Man pages in Chinese |

## Key Facts

- **Kernel**: Y4KU, monolithic, i386 protected mode, GRUB-booted
- **Userspace**: M4SH single-process shell (PID 1), 40+ built-in commands
- **FS**: YAFS — COW B+Tree RAM disk (16 MB)
- **Standard**: 4P1 — not POSIX-compatible, no `errno`, no MMU
- **Doc**: BSD man page format + i18n
