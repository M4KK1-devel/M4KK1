/*
 * M4KK1 4P1 - syscall_m4k.c
 * Description: M4K ABI system call implementation —
 *              handler dispatch, permission checks,
 *              syscall table management.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <kernel.h>
#include <process.h>
#include <m4k_syscall.h>
#include <idt.h>
#include <ldso.h>

typedef uint32_t (*m4k_syscall_handler_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                          uint32_t arg4, uint32_t arg5);

typedef struct {
    m4k_syscall_handler_t handler;
    uint32_t permission_mask;
    const char *name;
    bool registered;
} m4k_syscall_entry_t;

static m4k_syscall_entry_t m4k_syscall_table[256];

static struct {
    uint32_t total_calls;
    uint32_t failed_calls;
    uint32_t permission_denied;
} m4k_syscall_stats;

#define M4K_PERMISSION_KERNEL    0xFFFFFFFF
#define M4K_PERMISSION_USER      0x00000001
#define M4K_PERMISSION_SYSTEM    0x000000FF

const char *m4k_syscall_get_name(uint32_t num);
void m4k_syscall_init_handlers(void);

static void m4k_syscall_table_init(void)
{
    mkrn_memset(m4k_syscall_table, 0, sizeof(m4k_syscall_table));
    mkrn_memset(&m4k_syscall_stats, 0, sizeof(m4k_syscall_stats));

    M4K_LOG_INFO("M4KK1 system call table initialized");
}

static bool m4k_syscall_check_permission(uint32_t syscall_num, uint32_t current_permission)
{
    if (syscall_num >= 256 || !m4k_syscall_table[syscall_num].registered) {
        return false;
    }

    if (current_permission == M4K_PERMISSION_KERNEL) {
        return true;
    }

    return (current_permission & m4k_syscall_table[syscall_num].permission_mask) != 0;
}

void m4k_syscall_handler(void)
{
    uint32_t syscall_num;
    uint32_t result = 0x4D000000;
    uint32_t saved_registers[6];

    __asm__ volatile ("movl %%eax, %0" : "=r"(syscall_num));

    m4k_syscall_stats.total_calls++;

    M4K_LOG_DEBUG("M4KK1 system call invoked: 0x");
    mkrn_console_write_hex(syscall_num);
    mkrn_console_write("\n");

    if (syscall_num >= 256) {
        M4K_LOG_WARN("Invalid M4KK1 system call number: 0x");
        mkrn_console_write_hex(syscall_num);
        mkrn_console_write("\n");
        m4k_syscall_stats.failed_calls++;
        goto m4k_syscall_return;
    }

    if (!m4k_syscall_table[syscall_num].registered) {
        M4K_LOG_WARN("Unregistered M4KK1 system call: 0x");
        mkrn_console_write_hex(syscall_num);
        mkrn_console_write("\n");
        m4k_syscall_stats.failed_calls++;
        goto m4k_syscall_return;
    }

    mkrn_process_t *current_process = mkrn_process_get_current();
    uint32_t current_permission = (current_process != NULL) ?
        M4K_PERMISSION_USER : M4K_PERMISSION_KERNEL;

    if (!m4k_syscall_check_permission(syscall_num, current_permission)) {
        M4K_LOG_WARN("Permission denied for M4KK1 system call 0x");
        mkrn_console_write_hex(syscall_num);
        mkrn_console_write(" (process: ");
        if (current_process) {
            mkrn_console_write_hex(current_process->pid);
        } else {
            mkrn_console_write("kernel");
        }
        mkrn_console_write(")\n");

        m4k_syscall_stats.permission_denied++;
        result = 0x4D000001;
        goto m4k_syscall_return;
    }

    __asm__ volatile (
        "movl %%ebx, 0(%0)\n"
        "movl %%ecx, 4(%0)\n"
        "movl %%edx, 8(%0)\n"
        "movl %%esi, 12(%0)\n"
        "movl %%edi, 16(%0)\n"
        "movl %%ebp, 20(%0)\n"
        : : "r"(saved_registers) : "memory"
    );

    m4k_syscall_handler_t handler = m4k_syscall_table[syscall_num].handler;
    if (handler != NULL) {
        uint32_t arg1, arg2, arg3, arg4, arg5;
        __asm__ volatile (
            "movl %%ebx, %0\n"
            "movl %%ecx, %1\n"
            "movl %%edx, %2\n"
            "movl %%esi, %3\n"
            "movl %%edi, %4\n"
            : "=r"(arg1), "=r"(arg2), "=r"(arg3), "=r"(arg4), "=r"(arg5)
        );

        result = handler(arg1, arg2, arg3, arg4, arg5);

        M4K_LOG_DEBUG("M4KK1 system call 0x");
        mkrn_console_write_hex(syscall_num);
        mkrn_console_write(" returned: 0x");
        mkrn_console_write_hex(result);
        mkrn_console_write("\n");
    } else {
        M4K_LOG_ERROR("M4KK1 system call handler is NULL for 0x");
        mkrn_console_write_hex(syscall_num);
        mkrn_console_write("\n");
        m4k_syscall_stats.failed_calls++;
        result = 0x4D000002;
    }

m4k_syscall_return:
    __asm__ volatile ("movl %0, %%eax" : : "r"(result));

    __asm__ volatile (
        "movl 0(%0), %%ebx\n"
        "movl 4(%0), %%ecx\n"
        "movl 8(%0), %%edx\n"
        "movl 12(%0), %%esi\n"
        "movl 16(%0), %%edi\n"
        "movl 20(%0), %%ebp\n"
        : : "r"(saved_registers) : "memory"
    );
}

void m4k_syscall_init(void)
{
    m4k_syscall_table_init();

    mkrn_idt_register_handler(0x4D, m4k_syscall_handler);

    m4k_syscall_init_handlers();

    M4K_LOG_INFO("M4KK1 system call system initialized");
}

void m4k_syscall_register(uint32_t num, void *handler)
{
    if (num >= 256) {
        M4K_LOG_ERROR("Invalid M4KK1 system call number for registration: 0x");
        mkrn_console_write_hex(num);
        mkrn_console_write("\n");
        return;
    }

    if (handler == NULL) {
        M4K_LOG_ERROR("Cannot register NULL handler for M4KK1 system call 0x");
        mkrn_console_write_hex(num);
        mkrn_console_write("\n");
        return;
    }

    m4k_syscall_table[num].handler = (m4k_syscall_handler_t)handler;
    m4k_syscall_table[num].registered = true;

    m4k_syscall_table[num].permission_mask = M4K_PERMISSION_USER;

    m4k_syscall_table[num].name = m4k_syscall_get_name(num);

    M4K_LOG_INFO("M4KK1 system call 0x");
    mkrn_console_write_hex(num);
    mkrn_console_write(" registered: ");
    mkrn_console_write(m4k_syscall_table[num].name ? m4k_syscall_table[num].name : "Unknown");
    mkrn_console_write("\n");
}

const char *m4k_syscall_get_name(uint32_t num)
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

static uint32_t m4k_syscall_exit_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    uint32_t status = arg1;

    M4K_LOG_INFO("M4KK1 process exit called with status: ");
    mkrn_console_write_dec(status);
    mkrn_console_write("\n");

    mkrn_process_exit();

    return 0;
}

static uint32_t m4k_syscall_read_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    uint32_t fd = arg1;
    void *buf = (void *)arg2;
    uint32_t count = arg3;

    M4K_LOG_DEBUG("M4KK1 Read system call: fd=");
    mkrn_console_write_dec(fd);
    mkrn_console_write(", count=");
    mkrn_console_write_dec(count);
    mkrn_console_write("\n");

    return 0x4D000003;
}

static uint32_t m4k_syscall_write_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                       uint32_t arg4, uint32_t arg5)
{
    uint32_t fd = arg1;
    const void *buf = (const void *)arg2;
    uint32_t count = arg3;

    M4K_LOG_DEBUG("M4KK1 Write system call: fd=");
    mkrn_console_write_dec(fd);
    mkrn_console_write(", count=");
    mkrn_console_write_dec(count);
    mkrn_console_write("\n");

    if (fd == 1 && buf) {
        char *str = (char *)buf;
        uint32_t i;

        for (i = 0; i < count && str[i]; i++) {
            mkrn_console_put_char(str[i]);
        }

        return i;
    }

    return 0x4D000003;
}

void m4k_syscall_init_handlers(void)
{
    m4k_syscall_register(M4K_SYS_EXIT, m4k_syscall_exit_impl);
    m4k_syscall_register(M4K_SYS_READ, m4k_syscall_read_impl);
    m4k_syscall_register(M4K_SYS_WRITE, m4k_syscall_write_impl);

    M4K_LOG_INFO("M4KK1 system call handlers registered");
}
