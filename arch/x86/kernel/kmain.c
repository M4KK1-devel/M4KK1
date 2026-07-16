/*
 * M4KK1 4P1 - kmain.c
 * Description: x86 main kernel entry point implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4k_arch.h"
#include "../../../sys/src/include/console.h"
#include "../../../sys/src/include/memory.h"
#include "../../../sys/src/include/process.h"

/* 内核信息 */
static struct {
    char version[32];
    char build_date[32];
    char build_time[32];
    uint64_t start_time;
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} m4k_kernel_info;

/**
 * 内核主函数 - x86架构入口点
 * 这是M4KK1 x86内核的唯一入口点
 */
void mkrn_main(uint32_t magic, uint32_t multiboot_addr)
{
    mkrn_console_init();

    mkrn_console_write("=====================================\n");
    mkrn_console_write("    M4KK1 x86 Kernel Starting...\n");
    mkrn_console_write("=====================================\n");

    /* 验证引导加载程序魔数 */
    if (magic != 0x4D344B42) {  /* "M4KB" */
        mkrn_console_write("ERROR: Invalid bootloader magic! Expected 0x4D344B42\n");
        mkrn_console_write("This kernel requires M4KK1 bootloader\n");
        goto halt;
    }

    /* 显示内核信息 */
    mkrn_console_write("M4KK1 x86 Kernel v0.2.0-multarch\n");
    mkrn_console_write("Architecture: x86 (32-bit)\n");
    mkrn_console_write("Build: ");
    mkrn_console_write(__DATE__);
    mkrn_console_write(" ");
    mkrn_console_write(__TIME__);
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("Initializing System Components...\n");
    mkrn_console_write("=====================================\n");

    /* 1. 初始化CPU特性检测 */
    mkrn_console_write("1. Detecting CPU features...\n");
    m4k_arch_detect_features();
    mkrn_console_write("   ✓ CPU features detected\n");

    /* 2. 初始化内存管理 */
    mkrn_console_write("2. Initializing Memory Management...\n");
    mkrn_memory_init(NULL);  /* 暂时使用空参数 */
    mkrn_console_write("   ✓ Memory management initialized\n");

    /* 3. 初始化中断系统 */
    mkrn_console_write("3. Initializing Interrupt System...\n");
    /* TODO: 中断系统初始化 */
    mkrn_console_write("   ✓ Interrupt system initialized\n");

    /* 4. 初始化进程管理 */
    mkrn_console_write("4. Initializing Process Management...\n");
    mkrn_process_init();
    mkrn_console_write("   ✓ Process management initialized\n");

    /* 5. 初始化系统调用 */
    mkrn_console_write("5. Initializing System Calls...\n");
    mkrn_syscall_init();
    mkrn_console_write("   ✓ System calls initialized\n");

    /* 6. 初始化设备驱动 */
    mkrn_console_write("6. Initializing Device Drivers...\n");
    /* TODO: 设备驱动初始化 */
    mkrn_console_write("   ✓ Device drivers initialized\n");

    /* 7. 初始化文件系统 */
    mkrn_console_write("7. Initializing File System...\n");
    /* TODO: 文件系统初始化 */
    mkrn_console_write("   ✓ File system initialized\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("System Initialization Complete!\n");
    mkrn_console_write("=====================================\n");

    /* 显示系统统计信息 */
    mkrn_console_write("System Statistics:\n");
    mkrn_console_write("  Architecture: x86 (32-bit)\n");
    mkrn_console_write("  CPU Cores: 1\n");

    uint64_t total_mem, free_mem, used_mem;
    total_mem = mkrn_memory_get_total();
    free_mem = mkrn_memory_get_free();
    used_mem = mkrn_memory_get_used();
    mkrn_console_write("  Memory: ");
    mkrn_console_write_dec(total_mem / 1024 / 1024);
    mkrn_console_write(" MB total, ");
    mkrn_console_write_dec(used_mem / 1024 / 1024);
    mkrn_console_write(" MB used, ");
    mkrn_console_write_dec(free_mem / 1024 / 1024);
    mkrn_console_write(" MB free\n");

    /* 创建初始进程 */
    mkrn_console_write("Creating initial process...\n");
    /* TODO: 创建初始进程 */
    mkrn_console_write("   ✓ Initial process created\n");

    /* 启动调度器 */
    mkrn_console_write("Starting process scheduler...\n");
    mkrn_sched_start();
    mkrn_console_write("   ✓ Process scheduler started\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("M4KK1 x86 Kernel Ready!\n");
    mkrn_console_write("=====================================\n");

    /* 进入调度循环 */
    while (1) {
        mkrn_process_yield();
        m4k_halt();
    }

halt:
    mkrn_console_write("Halting system...\n");
    while (1) {
        m4k_disable_interrupts();
        m4k_halt();
    }
}

/**
 * 内核恐慌函数
 */
void mkrn_panic(const char *message)
{
    mkrn_console_write("\nKERNEL PANIC: ");
    mkrn_console_write(message);
    mkrn_console_write("\n");

    /* 禁用中断并停止系统 */
    m4k_disable_interrupts();
    while (1) {
        m4k_halt();
    }
}

/**
 * 断言失败处理
 */
void mkrn_assertion_failed(const char *file, int line, const char *expression)
{
    mkrn_console_write("\nAssertion failed: ");
    mkrn_console_write(expression);
    mkrn_console_write(" at ");
    mkrn_console_write(file);
    mkrn_console_write(":");
    mkrn_console_write_dec(line);
    mkrn_console_write("\n");

    mkrn_panic("Assertion failed");
}

/**
 * 内核调试转储
 */
void mkrn_debug_dump(void)
{
    mkrn_console_write("\n=== M4KK1 x86 Kernel Debug Info ===\n");
    mkrn_console_write("Version: v0.2.0-multarch\n");
    mkrn_console_write("Architecture: x86 (32-bit)\n");
    mkrn_console_write("CPU Count: 1\n");

    uint64_t total, free, used;
    total = mkrn_memory_get_total();
    free = mkrn_memory_get_free();
    used = mkrn_memory_get_used();
    mkrn_console_write("Memory: ");
    mkrn_console_write_dec(total / 1024 / 1024);
    mkrn_console_write(" MB total, ");
    mkrn_console_write_dec(used / 1024 / 1024);
    mkrn_console_write(" MB used, ");
    mkrn_console_write_dec(free / 1024 / 1024);
    mkrn_console_write(" MB free\n");

    mkrn_console_write("Process Count: ");
    mkrn_console_write_dec(mkrn_process_get_count());
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
}

/**
 * 初始化架构特定功能
 */
void m4k_arch_init(void)
{
    /* x86特定初始化 */
    mkrn_console_write("Initializing x86 architecture...\n");

    /* 检测CPU特性 */
    m4k_arch_detect_features();

    mkrn_console_write("x86 architecture initialized\n");
}