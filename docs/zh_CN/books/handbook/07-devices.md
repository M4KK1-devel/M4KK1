# 第 7 章 设备模型

## 7.1 设备模型概述

M4KK1 采用双层设备模型: **物理层 `/device`** + **逻辑层 `/dev`**。

- `/device` — 物理设备发现与枚举
- `/dev` — 逻辑设备节点，供用户空间使用

## 7.2 当前支持的设备

| 设备 | 路径 | 状态 | 说明 |
|------|------|------|------|
| 串口控制台 | `/dev/console` | ✅ | COM1, 38400 8N1 |
| 空设备 | `/dev/null` | ✅ | 丢弃写入，返回空读取 |
| 零设备 | `/dev/zero` | ✅ | 返回 \\0 字节 |

## 7.3 串口控制台

文件: `sys/src/drivers/console.c`

### 配置

- 端口: COM1 (0x3F8)
- 波特率: 38400
- 数据位: 8, 停止位: 1, 校验: 无, 流控: 无

### 寄存器

| 端口偏移 | 功能 |
|---------|------|
| +0 | 数据 (R/W) |
| +1 | 中断使能 |
| +5 | 线路状态 |

### 核心函数

```c
void ser_putc(char c);           // 发送字符
void ser_puts(const char *s);    // 发送字符串
int  ser_getc(void);             // 非阻塞读取 (-1 = 无数据)
char ser_getchar(void);          // 阻塞读取
```

### 控制台 API

```c
void mkrn_console_init(void);           // 初始化
void mkrn_console_write(const char *s); // 写字符串
void mkrn_console_write_hex(u32 v);     // 写十六进制
void mkrn_console_write_dec(u32 v);     // 写十进制
```

## 7.4 ANSI 颜色支持

| 函数 | ANSI 码 | 颜色 |
|------|---------|------|
| `c_grn()` | `\x1B[32m` | 绿色 |
| `c_red()` | `\x1B[31m` | 红色 |
| `c_ylw()` | `\x1B[33m` | 黄色 |
| `c_cyn()` | `\x1B[36m` | 青色 |
| `c_rst()` | `\x1B[0m` | 重置 |

## 7.5 驱动状态总览

| 驱动 | 状态 |
|------|------|
| 串口 (COM1) | ✅ 完成 |
| PS/2 键盘 | 📋 存根 |
| PS/2 鼠标 | 📋 存根 |
| ATA/PATA | ❌ 未开始 |
| AHCI | ❌ 未开始 |
| NE2000 / RTL8139 | ❌ 未开始 |
| 帧缓冲 | ❌ 未开始 |
