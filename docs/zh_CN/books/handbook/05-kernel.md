# 第 5 章 内核

## 5.1 内核概述

Y4KU 是 M4KK1 的宏内核，运行于 i386 保护模式。

| 属性 | 值 |
|------|-----|
| 名称 | Y4KU (Yaku Kernel Universe) |
| 架构 | i386 (32-bit x86) |
| 引导 | GRUB multiboot |
| 语言 | Freestanding C + NASM 汇编 |
| 内存模型 | 单地址空间（无 MMU） |
| 特权级 | 全部 Ring 0 |
| 调度频率 | PIT 定时器 1000 Hz |

### 源代码结构

```
sys/src/
├── kernel/        # 内核核心 (kmain.c, process.c, syscall.c, signal.c, ...)
├── arch/m4kk1/    # 架构相关 (gdt.c, idt.c, timer.c)
├── drivers/       # 驱动 (console.c)
├── fs/            # 文件系统 (vfs.c, procfs.c, yafs/)
├── mm/            # 内存管理 (memory.c)
├── lib/           # 工具库 (string.c, debug.c)
└── include/       # 头文件
```

## 5.2 进程模型

### 进程控制块

每个进程由 `mkrn_process_t` 表示，包含 PID、PPID、状态标签位掩码、寄存器上下文、文件描述符表等。

### 状态标签 (State Tag)

使用 `uint64_t` 位掩码表示进程状态，支持组合状态：

| 标签 | 位 | 说明 |
|------|----|------|
| `SCHED_READY` | 0 | 就绪 |
| `SCHED_RUNNING` | 1 | 运行中 |
| `SCHED_SLEEPING` | 2 | 睡眠 |
| `WAIT_FS` | 8 | 等待文件系统 |
| `WAIT_PIPE` | 9 | 等待管道 |
| `WAIT_TIMER` | 10 | 等待定时器 |
| `ZOMBIE` | 12 | 僵尸进程 |

### 进程生命周期

```
创建 → 就绪 → 运行 → 睡眠/停止 → 就绪 → 退出 → 僵尸
```

### 调度器

协作式轮转调度，遍历进程列表选择第一个 `SCHED_READY` 进程运行。

## 5.3 系统调用

M4KK1 支持两套系统调用 ABI：

### int 0x80（兼容路径）

```
%eax = 系统调用号, %ebx/%ecx/%edx = 参数
返回值: %eax
```

### int 0x4D（M4KK1 原生路径）

系统调用号格式: `0x4D00XXXX`

```c
// 包装函数
m4k_sc0(n)          // 0 参数
m4k_sc1(n, a)       // 1 参数
m4k_sc2(n, a, b)    // 2 参数
m4k_sc3(n, a, b, c) // 3 参数
m4k_sc4(n, a,b,c,d) // 4 参数 (使用 esi)
```

### 主要系统调用

| 编号 | 函数 | 功能 |
|------|------|------|
| `0x4D000001` | `m4k_exit` | 退出进程 |
| `0x4D000002` | `m4k_spawn` | 创建进程 |
| `0x4D000003` | `m4k_waitpid` | 等待进程 |
| `0x4D000004` | `m4k_getpid` | 获取 PID |
| `0x4D000005` | `m4k_kill` | 发送信号 |
| `0x4D000007` | `m4k_fork_status` | 选择性状态继承 |
| `0x4D000008` | `m4k_setns` | 设置命名空间 |

## 5.4 定时器与 RTC

### PIT 定时器

- 通道 0，模式 3（方波），1000 Hz
- 内核 API: `mkrn_timer_init()`、`mkrn_timer_get_ticks()`、`mkrn_timer_wait()` 等

### RTC (CMOS)

- 通过 CMOS RAM（端口 `0x70`/`0x71`）读取/设置实时时钟
- 支持 BCD 自动转换

### 闹钟系统

最大 256 个闹钟，支持一次性或周期性触发。

## 5.5 内存管理

### 物理内存分配器

伙伴系统 (Buddy System)，最小 4 KB 块，最大阶 4（64 KB）。

| 函数 | 说明 |
|------|------|
| `mkrn_memory_init(mb_info)` | 从 multiboot 信息初始化 |
| `mkrn_memory_alloc_page()` | 分配一页 |
| `mkrn_memory_free_page(addr)` | 释放一页 |

### 内核堆

- 位置: 内核 BSS 段末尾
- 大小: 4 MB（硬编码在 `linker.ld`）

### 链接脚本

```lds
. = 0x100000;       // 加载地址 1 MB
.text : { *(.text) }
.rodata : { *(.rodata) }
.data : { *(.data) }
.bss : { *(.bss) }
_heap_start = .;
. += 4M;            // 4 MB 堆
_heap_end = .;
_kernel_stack = . + 16K;  // 16 KB 内核栈
```

### 限制

- 无分页（物理地址 == 虚拟地址）
- 无 MMU 内存保护
- 无页缓存
- 无交换

## 5.6 信号

10 个信号的定义：

| 信号 | 编号 | 默认动作 |
|------|------|---------|
| SIGABRT | 1 | 终止 + 核心转储 |
| SIGKILL | 2 | 立即终止（PID 1 免疫） |
| SIGTERM | 3 | 优雅终止 |
| SIGSTOP | 4 | 暂停执行 |
| SIGCONT | 5 | 恢复执行 |
| SIGTRAP | 6 | 调试追踪 |
| SIGCHLD | 7 | 子进程状态变化 |
| SIGPIPE | 8 | 管道断裂 |
| SIGUSR1 | 9 | 用户自定义 1 |
| SIGUSR2 | 10 | 用户自定义 2 |

信号不排队，同一信号号在进程中已挂起时，后续发送无效。

## 5.7 命名空间

每个进程持有 `struct m4k_namespace`，定义其私有文件系统视图：

- 每个进程 16 个挂载点
- 支持 bind mount、只读挂载
- 路径解析优先级: 进程命名空间 → 全局 VFS 根
