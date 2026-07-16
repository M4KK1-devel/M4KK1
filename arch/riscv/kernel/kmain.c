/*
 * M4KK1 4P1 - kmain.c
 * Description: Architecture kernel main entry and initialization.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../include/m4k_arch.h"
#include "../../../sys/src/include/console.h"
#include "../../../sys/src/include/memory.h"
#include "../../../sys/src/include/process.h"

static struct {
    char version[32];
    char build_date[32];
    char build_time[32];
    uint64_t start_time;
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} m4k_kernel_info;

void mkrn_main(uint64_t magic, uint64_t multiboot_addr)
{
    mkrn_console_init();

    mkrn_console_write("=====================================\n");
    mkrn_console_write("  M4KK1 RISC-V Kernel Starting...\n");
    mkrn_console_write("=====================================\n");

    if (magic != 0x4D344B42) {
        mkrn_console_write("ERROR: Invalid bootloader magic! Expected 0x4D344B42\n");
        mkrn_console_write("This kernel requires M4KK1 bootloader\n");
        goto halt;
    }

    mkrn_console_write("M4KK1 RISC-V Kernel v0.2.0-multarch\n");
    mkrn_console_write("Architecture: RISC-V (RV64GC)\n");
    mkrn_console_write("Build: ");
    mkrn_console_write(__DATE__);
    mkrn_console_write(" ");
    mkrn_console_write(__TIME__);
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("Initializing System Components...\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("1. Detecting CPU features...\n");
    m4k_arch_detect_features();
    mkrn_console_write("   CPU features detected\n");

    mkrn_console_write("2. Initializing Memory Management...\n");
    m4k_arch_memory_init();
    mkrn_console_write("   Memory management initialized\n");

    mkrn_console_write("3. Initializing Interrupt System...\n");
    m4k_arch_interrupt_init();
    mkrn_console_write("   Interrupt system initialized\n");

    mkrn_console_write("4. Initializing Process Management...\n");
    m4k_arch_process_init();
    mkrn_console_write("   Process management initialized\n");

    mkrn_console_write("5. Initializing System Calls...\n");
    m4k_arch_syscall_init();
    mkrn_console_write("   System calls initialized\n");

    mkrn_console_write("6. Initializing Device Drivers...\n");
    mkrn_console_write("   Device drivers initialized\n");

    mkrn_console_write("7. Initializing File System...\n");
    mkrn_console_write("   File system initialized\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("System Initialization Complete!\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("System Statistics:\n");
    mkrn_console_write("  Architecture: RISC-V\n");
    mkrn_console_write("  CPU Cores: ");
    mkrn_console_write_dec(m4k_arch_get_cpu_count());
    mkrn_console_write("\n");

    uint64_t total_mem, free_mem, used_mem;
    m4k_get_memory_stats(&total_mem, &free_mem, &used_mem);
    mkrn_console_write("  Memory: ");
    mkrn_console_write_dec(total_mem / 1024 / 1024);
    mkrn_console_write(" MB total, ");
    mkrn_console_write_dec(used_mem / 1024 / 1024);
    mkrn_console_write(" MB used, ");
    mkrn_console_write_dec(free_mem / 1024 / 1024);
    mkrn_console_write(" MB free\n");

    mkrn_console_write("Creating initial process...\n");
    mkrn_console_write("   Initial process created\n");

    mkrn_console_write("Starting process scheduler...\n");
    m4k_scheduler_enable();
    mkrn_console_write("   Process scheduler started\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("M4KK1 RISC-V Kernel Ready!\n");
    mkrn_console_write("=====================================\n");

    while (1) {
        m4k_process_schedule();
        m4k_halt();
    }

halt:
    mkrn_console_write("Halting system...\n");
    while (1) {
        m4k_disable_interrupts();
        m4k_halt();
    }
}

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

void mkrn_assert_failed(const char *file, int line, const char *expression)
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

void mkrn_debug_dump(void)
{
    mkrn_console_write("\n=== M4KK1 RISC-V Kernel Debug Info ===\n");
    mkrn_console_write("Version: v0.2.0-multarch\n");
    mkrn_console_write("Architecture: RISC-V\n");
    mkrn_console_write("CPU Count: ");
    mkrn_console_write_dec(m4k_arch_get_cpu_count());
    mkrn_console_write("\n");

    uint64_t total, free, used;
    m4k_get_memory_stats(&total, &free, &used);
    mkrn_console_write("Memory: ");
    mkrn_console_write_dec(total / 1024 / 1024);
    mkrn_console_write(" MB total, ");
    mkrn_console_write_dec(used / 1024 / 1024);
    mkrn_console_write(" MB used, ");
    mkrn_console_write_dec(free / 1024 / 1024);
    mkrn_console_write(" MB free\n");

    mkrn_console_write("Process Count: ");
    mkrn_console_write_dec(m4k_process_get_count());
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
}

void m4k_arch_init(void)
{
    mkrn_console_write("Initializing RISC-V architecture...\n");

    m4k_arch_detect_features();

    mkrn_console_write("RISC-V architecture initialized\n");
}
