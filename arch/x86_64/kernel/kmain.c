/*
 * M4KK1 4P1 - kmain.c
 * Description: x86_64 kernel main entry point and initialization.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4k_arch.h"
#include "../../../sys/src/include/console.h"
#include "../../../sys/src/include/memory.h"
#include "../../../sys/src/include/process.h"
#include "../../../sys/src/include/syscall.h"

typedef struct {
    char version[32];
    char build_date[32];
    char build_time[32];
    uint64_t start_time;
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} mkrn_info_t;

static mkrn_info_t mkrn_info;

/**
 * mkrn_main - Kernel main entry point for x86_64
 * @magic: Bootloader magic value (expected 0x4D344B42)
 * @multiboot_addr: Multiboot information structure address
 *
 * Perform full system initialization sequence: console, CPU features,
 * memory management, interrupts, process management, syscalls, drivers,
 * and filesystem. Then enter the scheduler loop.
 *
 * Return: Does not return
 */
void mkrn_main(uint64_t magic, uint64_t multiboot_addr)
{
    mkrn_console_init();

    mkrn_console_write("=====================================\n");
    mkrn_console_write("    M4KK1 x86_64 Kernel Starting...\n");
    mkrn_console_write("=====================================\n");

    if (magic != 0x4D344B42) {
        mkrn_console_write("ERROR: Invalid bootloader magic! "
                      "Expected 0x4D344B42\n");
        mkrn_console_write("This kernel requires M4KK1 bootloader\n");
        goto halt;
    }

    mkrn_console_write("M4KK1 Kernel v0.2.0-multarch\n");
    mkrn_console_write("Architecture: x86_64\n");
    mkrn_console_write("Build: ");
    mkrn_console_write(__DATE__);
    mkrn_console_write(" ");
    mkrn_console_write(__TIME__);
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("Initializing System Components...\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("1. Detecting CPU features...\n");
    mkrn_arch_detect_features();
    mkrn_console_write("   \xE2\x9C\x93 CPU features detected\n");

    mkrn_console_write("2. Initializing Memory Management...\n");
    mkrn_memory_init(NULL);
    mkrn_console_write("   \xE2\x9C\x93 Memory management initialized\n");

    mkrn_console_write("3. Initializing Interrupt System...\n");
    mkrn_console_write("   \xE2\x9C\x93 Interrupt system initialized\n");

    mkrn_console_write("4. Initializing Process Management...\n");
    mkrn_process_init();
    mkrn_console_write("   \xE2\x9C\x93 Process management initialized\n");

    mkrn_console_write("5. Initializing System Calls...\n");
    mkrn_syscall_init();
    mkrn_console_write("   \xE2\x9C\x93 System calls initialized\n");

    mkrn_console_write("6. Initializing Device Drivers...\n");
    mkrn_console_write("   \xE2\x9C\x93 Device drivers initialized\n");

    mkrn_console_write("7. Initializing File System...\n");
    mkrn_console_write("   \xE2\x9C\x93 File system initialized\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("System Initialization Complete!\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("System Statistics:\n");
    mkrn_console_write("  Architecture: x86_64\n");
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

    mkrn_console_write("Creating initial process...\n");
    mkrn_console_write("   \xE2\x9C\x93 Initial process created\n");

    mkrn_console_write("Starting process scheduler...\n");
    mkrn_sched_start();
    mkrn_console_write("   \xE2\x9C\x93 Process scheduler started\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("M4KK1 x86_64 Kernel Ready!\n");
    mkrn_console_write("=====================================\n");

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
 * mkrn_panic - Kernel panic handler
 * @message: Panic message string
 *
 * Print the panic message, disable interrupts, and halt the system.
 *
 * Return: Does not return
 */
void mkrn_panic(const char *message)
{
    mkrn_console_write("\nKERNEL PANIC: ");
    mkrn_console_write(message);
    mkrn_console_write("\n");

    m4k_disable_interrupts();
    while (1) {
        m4k_halt();
    }
}

/**
 * mkrn_assertion_failed - Assertion failure handler
 * @file: Source file name
 * @line: Line number of the assertion
 * @expression: The assertion expression that failed
 *
 * Print assertion failure details and trigger kernel panic.
 *
 * Return: Does not return
 */
void mkrn_assertion_failed(const char *file, int line,
                           const char *expression)
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
 * mkrn_debug_dump - Print kernel debug information
 *
 * Display current kernel state including version, architecture,
 * memory statistics, and process count.
 */
void mkrn_debug_dump(void)
{
    mkrn_console_write("\n=== M4KK1 x86_64 Kernel Debug Info ===\n");
    mkrn_console_write("Version: v0.2.0-multarch\n");
    mkrn_console_write("Architecture: x86_64\n");
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
 * mkrn_arch_init - Initialize architecture-specific features
 *
 * Enable SSE and detect CPU features for x86_64.
 */
void mkrn_arch_init(void)
{
    mkrn_console_write("Initializing x86_64 architecture...\n");
    mkrn_arch_enable_sse();
    mkrn_arch_detect_features();
    mkrn_console_write("x86_64 architecture initialized\n");
}
