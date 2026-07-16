/*
 * M4KK1 4P1 - syscall.c
 * Description: x86_64 system call handling implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../../include/m4k_arch.h"
#include "../../../include/m4k_syscall.h"
#include "../../../include/console.h"
#include "../../../include/string.h"
#include "../../../include/process.h"

typedef uint64_t (*mkrn_syscall_handler_t)(
    uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5, uint64_t arg6);

typedef struct {
    mkrn_syscall_handler_t handler;
    uint32_t permission_mask;
    const char *name;
    bool registered;
} mkrn_syscall_entry_t;

static mkrn_syscall_entry_t mkrn_syscall_table[256];

typedef struct {
    uint64_t total_calls;
    uint64_t failed_calls;
    uint64_t permission_denied;
    uint64_t calls_by_type[256];
} mkrn_syscall_stats_t;

static mkrn_syscall_stats_t mkrn_syscall_stats;

#define M4K_PERMISSION_KERNEL    0xFFFFFFFF
#define M4K_PERMISSION_SYSTEM    0x000000FF
#define M4K_PERMISSION_USER      0x00000001

/**
 * mkrn_syscall_table_init - Initialize system call table
 *
 * Zero out the system call table and statistics structure.
 */
static void mkrn_syscall_table_init(void)
{
    mkrn_memset(mkrn_syscall_table, 0, sizeof(mkrn_syscall_table));
    mkrn_memset(&mkrn_syscall_stats, 0, sizeof(mkrn_syscall_stats));

    mkrn_console_write("M4KK1 x86_64 system call table initialized\n");
}

/**
 * mkrn_syscall_check_permission - Check syscall permission
 * @syscall_num: System call number
 * @current_permission: Current privilege level
 *
 * Verify that the caller has sufficient permission to invoke
 * the given system call.
 *
 * Return: true if permitted, false otherwise
 */
static bool mkrn_syscall_check_permission(uint32_t syscall_num,
                                          uint32_t current_permission)
{
    if (syscall_num >= 256
        || !mkrn_syscall_table[syscall_num].registered) {
        return false;
    }

    if (current_permission == M4K_PERMISSION_KERNEL) {
        return true;
    }

    return (current_permission
            & mkrn_syscall_table[syscall_num].permission_mask) != 0;
}

/**
 * m4k_syscall_handler - M4KK1 syscall ABI handler (interrupt 0x4D)
 *
 * Dispatch system calls invoked via interrupt 0x4D using the
 * M4KK1-specific ABI. Arguments are passed in RDI, RSI, RDX,
 * RCX, R8, R9; syscall number in RAX; return value via RAX.
 */
void m4k_syscall_handler(void)
{
    uint64_t syscall_num;
    uint64_t result = 0xM4K00000;
    uint32_t current_permission;

    __asm__ volatile ("movq %%rax, %0" : "=r"(syscall_num));

    mkrn_syscall_stats.total_calls++;

    if (syscall_num >= 256) {
        mkrn_syscall_stats.failed_calls++;
        goto syscall_return;
    }

    if (!mkrn_syscall_table[syscall_num].registered) {
        mkrn_syscall_stats.failed_calls++;
        goto syscall_return;
    }

    current_permission = M4K_PERMISSION_USER;

    if (!mkrn_syscall_check_permission(syscall_num,
                                       current_permission)) {
        mkrn_syscall_stats.permission_denied++;
        result = 0xM4K00001;
        goto syscall_return;
    }

    mkrn_syscall_handler_t handler =
        mkrn_syscall_table[syscall_num].handler;
    if (handler != NULL) {
        uint64_t arg1, arg2, arg3, arg4, arg5, arg6;
        __asm__ volatile (
            "movq %%rdi, %0\n"
            "movq %%rsi, %1\n"
            "movq %%rdx, %2\n"
            "movq %%rcx, %3\n"
            "movq %%r8, %4\n"
            "movq %%r9, %5\n"
            : "=r"(arg1), "=r"(arg2), "=r"(arg3),
              "=r"(arg4), "=r"(arg5), "=r"(arg6)
        );

        result = handler(arg1, arg2, arg3, arg4, arg5, arg6);
        mkrn_syscall_stats.calls_by_type[syscall_num]++;
    } else {
        result = 0xM4K00002;
    }

syscall_return:
    __asm__ volatile ("movq %0, %%rax" : : "r"(result));
}

/**
 * mkrn_syscall_init - Initialize the system call subsystem
 *
 * Set up the syscall table and register all built-in handlers.
 */
void mkrn_syscall_init(void)
{
    mkrn_syscall_table_init();
    mkrn_syscall_init_handlers();

    mkrn_console_write("M4KK1 x86_64 system call system initialized\n");
}

/**
 * mkrn_syscall_register - Register a system call handler
 * @num: System call number (0-255)
 * @handler: Pointer to the handler function
 *
 * Register a system call handler with default user-level
 * permission and look up its ABI name.
 */
void mkrn_syscall_register(uint32_t num, void *handler)
{
    if (num >= 256) {
        mkrn_console_write("Invalid M4KK1 system call number: 0x");
        mkrn_console_write_hex(num);
        mkrn_console_write("\n");
        return;
    }

    if (handler == NULL) {
        mkrn_console_write("Cannot register NULL handler "
                      "for M4KK1 system call 0x");
        mkrn_console_write_hex(num);
        mkrn_console_write("\n");
        return;
    }

    mkrn_syscall_table[num].handler =
        (mkrn_syscall_handler_t)handler;
    mkrn_syscall_table[num].registered = true;
    mkrn_syscall_table[num].permission_mask =
        M4K_PERMISSION_USER;
    mkrn_syscall_table[num].name =
        mkrn_syscall_get_name(num);

    mkrn_console_write("M4KK1 system call 0x");
    mkrn_console_write_hex(num);
    mkrn_console_write(" registered: ");
    mkrn_console_write(
        mkrn_syscall_table[num].name
        ? mkrn_syscall_table[num].name : "Unknown");
    mkrn_console_write("\n");
}

/**
 * mkrn_syscall_get_name - Get human-readable syscall name
 * @num: System call number
 *
 * Return: String name of the system call, or "unknown"
 */
const char *mkrn_syscall_get_name(uint32_t num)
{
    switch (num) {
        case M4K_SYS_EXIT: return "m4k_exit";
        case M4K_SYS_FORK: return "m4k_fork";
        case M4K_SYS_READ: return "m4k_read";
        case M4K_SYS_WRITE: return "m4k_write";
        case M4K_SYS_OPEN: return "m4k_open";
        case M4K_SYS_CLOSE: return "m4k_close";
        case M4K_SYS_EXEC: return "m4k_exec";
        case M4K_SYS_MMAP: return "m4k_mmap";
        case M4K_SYS_MUNMAP: return "m4k_munmap";
        case M4K_SYS_IOCTL: return "m4k_ioctl";
        case M4K_SYS_FCNTL: return "m4k_fcntl";
        case M4K_SYS_SELECT: return "m4k_select";
        case M4K_SYS_POLL: return "m4k_poll";
        case M4K_SYS_EPOLL: return "m4k_epoll";
        default: return "unknown";
    }
}

/**
 * mkrn_syscall_exit_impl - Exit current process
 * @arg1: Exit status
 * @arg2: Unused
 * @arg3: Unused
 * @arg4: Unused
 * @arg5: Unused
 * @arg6: Unused
 *
 * Terminate the calling process with the given status code.
 *
 * Return: Does not return on success
 */
static uint64_t mkrn_syscall_exit_impl(
    uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint32_t status = (uint32_t)arg1;

    mkrn_console_write("M4KK1 process exit called with status: ");
    mkrn_console_write_dec(status);
    mkrn_console_write("\n");

    return 0;
}

/**
 * mkrn_syscall_read_impl - Read from file descriptor
 * @arg1: File descriptor
 * @arg2: Destination buffer
 * @arg3: Number of bytes to read
 * @arg4: Unused
 * @arg5: Unused
 * @arg6: Unused
 *
 * Return: Number of bytes read, or error code
 */
static uint64_t mkrn_syscall_read_impl(
    uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint32_t fd = (uint32_t)arg1;
    void *buf = (void *)arg2;
    uint64_t count = arg3;

    mkrn_console_write("M4KK1 Read system call: fd=");
    mkrn_console_write_dec(fd);
    mkrn_console_write(", count=");
    mkrn_console_write_dec(count);
    mkrn_console_write("\n");

    return 0xM4K00003;
}

/**
 * mkrn_syscall_write_impl - Write to file descriptor
 * @arg1: File descriptor
 * @arg2: Source buffer
 * @arg3: Number of bytes to write
 * @arg4: Unused
 * @arg5: Unused
 * @arg6: Unused
 *
 * If fd is 1 (stdout), output characters to the console.
 *
 * Return: Number of bytes written, or error code
 */
static uint64_t mkrn_syscall_write_impl(
    uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint32_t fd = (uint32_t)arg1;
    const void *buf = (const void *)arg2;
    uint64_t count = arg3;

    mkrn_console_write("M4KK1 Write system call: fd=");
    mkrn_console_write_dec(fd);
    mkrn_console_write(", count=");
    mkrn_console_write_dec(count);
    mkrn_console_write("\n");

    if (fd == 1 && buf) {
        char *str = (char *)buf;
        uint64_t i;

        for (i = 0; i < count && str[i]; i++) {
            mkrn_console_put_char(str[i]);
        }

        return i;
    }

    return 0xM4K00003;
}

/**
 * mkrn_syscall_init_handlers - Register all system call handlers
 *
 * Register the built-in syscall implementations (exit, read, write).
 */
void mkrn_syscall_init_handlers(void)
{
    mkrn_syscall_register(
        M4K_SYS_EXIT, mkrn_syscall_exit_impl);
    mkrn_syscall_register(
        M4K_SYS_READ, mkrn_syscall_read_impl);
    mkrn_syscall_register(
        M4K_SYS_WRITE, mkrn_syscall_write_impl);

    mkrn_console_write("M4KK1 system call handlers registered\n");
}

/**
 * mkrn_syscall_get_stats - Get system call statistics
 * @total_calls: Output pointer for total call count
 * @failed_calls: Output pointer for failed call count
 * @permission_denied: Output pointer for permission denied count
 *
 * Return statistics about system call usage.
 */
void mkrn_syscall_get_stats(uint64_t *total_calls,
                            uint64_t *failed_calls,
                            uint64_t *permission_denied)
{
    if (total_calls)
        *total_calls = mkrn_syscall_stats.total_calls;
    if (failed_calls)
        *failed_calls = mkrn_syscall_stats.failed_calls;
    if (permission_denied)
        *permission_denied =
            mkrn_syscall_stats.permission_denied;
}

/**
 * mkrn_syscall_print_status - Print system call status
 *
 * Display current syscall statistics and list registered handlers.
 */
void mkrn_syscall_print_status(void)
{
    uint32_t i, registered_count = 0;

    mkrn_console_write("=== M4KK1 System Call Status ===\n");

    mkrn_console_write("Statistics:\n");
    mkrn_console_write("  Total calls: ");
    mkrn_console_write_dec(mkrn_syscall_stats.total_calls);
    mkrn_console_write("\n");

    mkrn_console_write("  Failed calls: ");
    mkrn_console_write_dec(mkrn_syscall_stats.failed_calls);
    mkrn_console_write("\n");

    mkrn_console_write("  Permission denied: ");
    mkrn_console_write_dec(mkrn_syscall_stats.permission_denied);
    mkrn_console_write("\n");

    mkrn_console_write("Registered system calls:\n");
    for (i = 0; i < 256; i++) {
        if (mkrn_syscall_table[i].registered) {
            mkrn_console_write("  0x");
            mkrn_console_write_hex(i);
            mkrn_console_write(" - ");
            mkrn_console_write(mkrn_syscall_get_name(i));
            mkrn_console_write(" (calls: ");
            mkrn_console_write_dec(
                mkrn_syscall_stats.calls_by_type[i]);
            mkrn_console_write(")\n");
            registered_count++;
        }
    }

    mkrn_console_write("Total registered system calls: ");
    mkrn_console_write_dec(registered_count);
    mkrn_console_write("\n");
    mkrn_console_write("=================================\n");
}

/**
 * mkrn_arch_syscall_init - Initialize x86_64 syscall subsystem
 *
 * Set up the M4KK1 system call table and register built-in
 * handler implementations.
 */
void mkrn_arch_syscall_init(void)
{
    mkrn_syscall_init();
    mkrn_console_write(
        "M4KK1 x86_64 system call system initialized\n");
}
