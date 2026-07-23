# M4SH Shell 设计与实现

## 概述

M4SH (M4KK1 Shell) 是系统的 init 进程（PID 1），提供交互式命令行环境。它由 `m4sh/core/m4sh_main.c`（578 行）实现，是一个完全自包含的 freestanding C 程序。

## 架构

```
init (PID 1)
  └── /bin/login ─── 登录会话管理
       └── /bin/m4sh ─── Shell (40+ 内置命令)
            ├── coreutils/  ─── 文件、进程、文本、管理工具
            ├── lib/        ─── 密码和组数据库
            ├── core/       ─── Shell 主循环、正则引擎
            └── login/      ─── 独立登录 ELF
```

## 启动流程

```
_start()
  ├── ser_puts(欢迎信息)
  ├── cwd_init()                     // 初始化工作目录
  ├── m4k_geteuid() == 0?
  │     ├── musr_boot_setup()        // 首次启动设置
  │     └── musr_cmd_login()         // 登录循环
  ├── render_line()                  // 首次提示符
  └── while(1) {
        musr_at_check_jobs()
        ser_getc()                   // 读取按键
        handle_key(ch)               // 处理按键
        render_line()                // 更新显示
      }
```

## 命令行编辑

M4SH 支持字符逐个回显的行编辑模式，包括:
- 字符追加和退格删除
- Tab 命令名补全（从命令表匹配）
- 上下键历史导航（最多 16 条）
- Ctrl+C 取消、Ctrl+D 退出

## 引号处理

引号解析在三个层面发生:

1. **命令列表分割** (`execute_command_list`): 在分割 `;` `&&` `||` 时跳过引号内容
2. **管道解析** (`run_pipeline`): 在分割 `|` 时跳过引号内容
3. **Token 化** (`tokenize_quoted`): 将参数字符串解析为 argv 数组

```c
// 单引号: 逐字保留内容
if (*p == '\'') {
    p++;
    while (*p && *p != '\'') *out++ = *p++;
}
// 双引号: 支持 \" \\ \$ 转义
else if (*p == '"') {
    p++;
    while (*p && *p != '"') {
        if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$'))
            p++;
        *out++ = *p++;
    }
}
```

## 管道实现

使用 `musr_sc_pipe()` 和 `musr_sc_dup2()` 实现，最多 8 个管道段。`out_fd` 抽象层使输出重定向到文件或管道。

## 环境变量

- 最多 16 个变量，每个名值最大 128 字节
- 展开语法: `$VAR` 和 `${VAR}`
- 单引号内不展开变量
- 通过 `export PATH=/bin:/usr/bin` 设置

## 命令注册

命令表定义在 `musr_cmd_table[]` 中：

```c
musr_cmd_t musr_cmd_table[] = {
    {"help",   musr_cmd_help,   "Show this help"},
    {"echo",   musr_cmd_echo,   "Print arguments"},
    {"grep",   musr_cmd_grep,   "Search text"},
    // ... 40+ 命令
    {NULL, NULL, NULL}
};
```

## 运行时支持

- 无动态堆分配（固定大小的全局缓冲区）
- 输出通过 `out_fd` 抽象层（支持串口和文件描述符）
- 内置字符串函数（无 libc）
- 单个主头文件 `m4sh/m4sh.h`（750 行）
