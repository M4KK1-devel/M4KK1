/*
 * M4KK1 4P1 - syscall_m4k.c
 * Description: M4KK1 system call dispatch (int 0x4D handlers)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <m4k_syscall.h>
#include <vfs.h>
#include <video.h>
#include <mouse.h>
#include <console.h>
#include <idt.h>
#include <kernel.h>
#include <ldso.h>
#include <process.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>

extern char mkrn_keyboard_get_char(void);
extern bool mkrn_keyboard_has_char(void);

extern uint32_t m4k_syscall_register_session_impl(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
extern uint32_t m4k_syscall_get_session_list_impl(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);


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
    if (syscall_num >= 256 || !m4k_syscall_table[syscall_num].registered)
        return false;
    if (current_permission == M4K_PERMISSION_KERNEL)
        return true;
    return (current_permission & m4k_syscall_table[syscall_num].permission_mask) != 0;
}

uint32_t m4k_syscall_handler(u32 syscall_num, u32 *saved_regs)
{
    uint32_t idx;
    uint32_t result = 0x4D000000;
    uint32_t arg1, arg2, arg3, arg4, arg5;

    arg1 = saved_regs[0];
    arg2 = saved_regs[1];
    arg3 = saved_regs[2];
    arg4 = saved_regs[3];
    arg5 = saved_regs[4];

    m4k_syscall_stats.total_calls++;

    idx = syscall_num & 0xFF;

    if (idx >= 256 || !m4k_syscall_table[idx].registered) {
        M4K_LOG_WARN("Unregistered M4KK1 system call");
        m4k_syscall_stats.failed_calls++;
        return result;
    }

    mkrn_process_t *current_process = mkrn_process_get_current();
    uint32_t current_permission = (current_process != NULL) ?
        M4K_PERMISSION_USER : M4K_PERMISSION_KERNEL;

    if (!m4k_syscall_check_permission(idx, current_permission)) {
        m4k_syscall_stats.permission_denied++;
        result = 0x4D000001;
        return result;
    }

    m4k_syscall_handler_t handler = m4k_syscall_table[idx].handler;
    if (handler != NULL) {
        result = handler(arg1, arg2, arg3, arg4, arg5);
    } else {
        m4k_syscall_stats.failed_calls++;
        result = 0x4D000002;
    }

    return result;
}

extern void isr_m4k_syscall(void);

void m4k_syscall_init(void)
{
    m4k_syscall_table_init();
    mkrn_idt_set_gate(0x4D, (uint32_t)isr_m4k_syscall, 0x08,
        M4K_IDT_PRESENT | M4K_IDT_DPL_3 | M4K_IDT_INTERRUPT_GATE_32);
    m4k_syscall_init_handlers();
    M4K_LOG_INFO("M4KK1 system call system initialized");
}

void m4k_syscall_register(uint32_t num, void *handler)
{
    uint32_t idx = num & 0xFF;
    if (idx >= 256) {
        M4K_LOG_ERROR("Invalid M4KK1 system call number for registration");
        return;
    }
    if (handler == NULL) {
        M4K_LOG_ERROR("Cannot register NULL handler for M4KK1 system call");
        return;
    }
    m4k_syscall_table[idx].handler = (m4k_syscall_handler_t)handler;
    m4k_syscall_table[idx].registered = true;
    m4k_syscall_table[idx].permission_mask = M4K_PERMISSION_USER;
    m4k_syscall_table[idx].name = m4k_syscall_get_name(num);
    M4K_LOG_INFO("M4KK1 system call registered");
}

const char *m4k_syscall_get_name(uint32_t num)
{
    switch (num) {
        case M4K_SYS_EXIT: return "m4k_exit";
        case M4K_SYS_SPAWN: return "m4k_spawn";
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
        case M4K_SYS_FORK_ST: return "m4k_fork_status";
        case M4K_SYS_WAIT: return "m4k_waitpid";
        case M4K_SYS_KILL: return "m4k_kill";
        case M4K_SYS_GETPPID: return "m4k_getppid";
        case M4K_SYS_SETNS: return "m4k_setns";
        case M4K_SYS_GETPID: return "m4k_getpid";
        case M4K_SYS_GETPROCS: return "m4k_getprocs";
        case M4K_SYS_GETUID: return "m4k_getuid";
        case M4K_SYS_GETEUID: return "m4k_geteuid";
        case M4K_SYS_GETGID: return "m4k_getgid";
        case M4K_SYS_GETEGID: return "m4k_getegid";
        case M4K_SYS_SETUID: return "m4k_setuid";
        case M4K_SYS_SETGID: return "m4k_setgid";
        case M4K_SYS_GETGROUPS: return "m4k_getgroups";
        case M4K_SYS_SETGROUPS: return "m4k_setgroups";
        case M4K_SYS_CHMOD: return "m4k_chmod";
        case M4K_SYS_CHOWN: return "m4k_chown";
        case M4K_SYS_ACCESS: return "m4k_access";
        case M4K_SYS_SETRLIMIT: return "m4k_setrlimit";
        case M4K_SYS_GETRLIMIT: return "m4k_getrlimit";
        case M4K_SYS_BRK: return "m4k_brk";
        case M4K_SYS_REGISTER_SESSION: return "m4k_register_session";
        case M4K_SYS_GET_SESSION_LIST: return "m4k_get_session_list";
        case M4K_SYS_GET_FRAMEBUFFER_INFO: return "m4k_get_framebuffer_info";
        case M4K_SYS_DRAW_TEST_PATTERN: return "m4k_draw_test_pattern";
        case M4K_SYS_GET_MOUSE_EVENT: return "m4k_get_mouse_event";
        case M4K_SYS_FLIP: return "m4k_flip";
        case M4K_SYS_DRAW_RECT: return "m4k_draw_rect";
        case M4K_SYS_DRAW_TEXT: return "m4k_draw_text";
        case M4K_SYS_GET_KEYBOARD_EVENT: return "m4k_get_keyboard_event";
        case M4K_SYS_GFX_BLIT: return "m4k_gfx_blit";
        default: return "unknown";
    }
}

/* -- Syscall: get mouse event -- */
static uint32_t m4k_syscall_mouse_event_impl(
    uint32_t buf_ptr, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    struct m4k_mouse_event *ev = (struct m4k_mouse_event *)buf_ptr;
    if (!ev)
        return (uint32_t)-1;

    if (mkrn_mouse_get_event(ev))
        return 1;
    return 0;
}

/* -- Syscall: get keyboard event -- */
static uint32_t m4k_syscall_keyboard_event_impl(
    uint32_t buf_ptr, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    struct m4k_keyboard_event *ev = (struct m4k_keyboard_event *)buf_ptr;
    if (!ev)
        return (uint32_t)-1;

    if (mkrn_keyboard_has_char()) {
        char ch = mkrn_keyboard_get_char();
        ev->ascii_char = (uint8_t)ch;
        ev->keycode = 0;
        ev->modifiers = 0;
        ev->reserved = 0;
        return 1;
    }
    return 0;
}

/* -- Handler implementations -- */

static uint32_t m4k_syscall_exit_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    uint32_t status = arg1;
    mkrn_process_exit((int)status);
    return 0;
}

static uint32_t m4k_syscall_read_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    (void)arg4; (void)arg5;
    uint32_t fd = arg1;
    void *buf = (void *)arg2;
    uint32_t count = arg3;

    if (fd == 0 && buf) {
        /* Stub: read from console */
        return 0;
    }
    return 0x4D000003;
}

static uint32_t m4k_syscall_write_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                       uint32_t arg4, uint32_t arg5)
{
    (void)arg4; (void)arg5;
    uint32_t fd = arg1;
    const void *buf = (const void *)arg2;
    uint32_t count = arg3;

    if (fd == 1 && buf) {
        const char *str = (const char *)buf;
        for (uint32_t i = 0; i < count && str[i]; i++)
            mkrn_console_put_char(str[i]);
        return count;
    }
    return 0x4D000003;
}

static uint32_t m4k_syscall_fork_st_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    uint64_t inherit_mask = ((uint64_t)arg2 << 32) | arg1;
    uint32_t flags = arg2;
    /* Note: properly the 64-bit mask should come in two 32-bit args */
    return (uint32_t)mkrn_fork_status(inherit_mask, flags);
}

static uint32_t m4k_syscall_wait_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    (void)arg4; (void)arg5;
    pid_t pid = (pid_t)arg1;
    int *status = (int *)arg2;
    int options = (int)arg3;
    return (uint32_t)mkrn_waitpid(pid, status, options);
}

static uint32_t m4k_syscall_kill_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    pid_t pid = (pid_t)arg1;
    int sig = (int)arg2;
    return (uint32_t)mkrn_kill(pid, sig);
}

static uint32_t m4k_syscall_getppid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_ppid();
}

static uint32_t m4k_syscall_getpid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                        uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_pid();
}

static uint32_t m4k_syscall_setns_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                       uint32_t arg4, uint32_t arg5)
{
    (void)arg4; (void)arg5;
    const char *path = (const char *)arg1;
    const char *target = (const char *)arg2;
    uint32_t flags = arg3;
    return (uint32_t)mkrn_setns(path, target, flags);
}

static uint32_t m4k_syscall_getprocs_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                          uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    struct mkrn_procinfo *buf = (struct mkrn_procinfo *)arg1;
    uint32_t max = arg2;
    if (!buf || max == 0) return (uint32_t)-1;
    return (uint32_t)mkrn_process_fill_info(buf, max);
}

static uint32_t m4k_syscall_brk_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                      uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    /* FIXME: implement brk */
    return (uint32_t)-1;
}

static uint32_t m4k_syscall_getuid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_uid();
}

static uint32_t m4k_syscall_geteuid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                          uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_euid();
}

static uint32_t m4k_syscall_setuid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    uint32_t uid = arg1;
    return (uint32_t)mkrn_process_set_uid(uid);
}

static uint32_t m4k_syscall_chmod_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                        uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    const char *path = (const char *)arg1;
    uint32_t mode = arg2;
    return (uint32_t)(int32_t)mkrn_vfs_chmod(path, mode);
}

static uint32_t m4k_syscall_chown_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                        uint32_t arg4, uint32_t arg5)
{
    (void)arg4; (void)arg5;
    const char *path = (const char *)arg1;
    uint32_t uid = arg2;
    uint32_t gid = arg3;
    return (uint32_t)(int32_t)mkrn_vfs_chown(path, uid, gid);
}

static uint32_t m4k_syscall_getgid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_gid();
}

static uint32_t m4k_syscall_getegid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                          uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return mkrn_process_get_egid();
}

static uint32_t m4k_syscall_setgid_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return (uint32_t)mkrn_process_set_gid(arg1);
}

static uint32_t m4k_syscall_getgroups_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                            uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    int size = (int)arg1;
    uint32_t *list = (uint32_t *)arg2;
    return (uint32_t)mkrn_process_get_groups(list, size);
}

static uint32_t m4k_syscall_setgroups_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                            uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    int size = (int)arg1;
    const uint32_t *list = (const uint32_t *)arg2;
    return (uint32_t)(int32_t)mkrn_process_set_groups(size, list);
}

static uint32_t m4k_syscall_access_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                         uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    const char *path = (const char *)arg1;
    int mode = (int)arg2;
    return (uint32_t)(int32_t)mkrn_vfs_access(path, mode);
}

static uint32_t m4k_syscall_setrlimit_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                            uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    int resource = (int)arg1;
    struct m4k_rlimit *rlp = (struct m4k_rlimit *)arg2;
    if (!rlp) return (uint32_t)-M4K_EINVAL;
    return (uint32_t)(int32_t)mkrn_process_set_rlimit(resource, rlp);
}

static uint32_t m4k_syscall_getrlimit_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                            uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    int resource = (int)arg1;
    struct m4k_rlimit *rlp = (struct m4k_rlimit *)arg2;
    if (!rlp) return (uint32_t)-M4K_EINVAL;
    return (uint32_t)(int32_t)mkrn_process_get_rlimit(resource, rlp);
}

static uint32_t m4k_syscall_spawn_impl(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                       uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    const char *path = (const char *)arg1;
    if (!path)
        return (uint32_t)-1;

    int fd = mkrn_vfs_open(path, M4K_O_RDONLY);
    if (fd < 0) {
        mkrn_console_write("spawn: open failed\n");
        return (uint32_t)-1;
    }

    uint8_t *elf_buf = NULL;
    uint32_t total = 0;
    uint32_t cap = 4096;
    int ret = -1;

    elf_buf = (uint8_t *)mkrn_alloc(cap);
    if (!elf_buf) {
        mkrn_vfs_close(fd);
        return (uint32_t)-1;
    }

    for (;;) {
        int n = mkrn_vfs_read(fd, elf_buf + total, cap - total);
        if (n < 0)
            goto out;
        if (n == 0)
            break;
        total += (uint32_t)n;
        if (total + 256 >= cap) {
            uint32_t new_cap = cap * 2;
            uint8_t *tmp = (uint8_t *)mkrn_alloc(new_cap);
            if (!tmp)
                goto out;
            mkrn_memcpy(tmp, elf_buf, total);
            mkrn_free(elf_buf);
            elf_buf = tmp;
            cap = new_cap;
        }
    }

    if (total == 0)
        goto out;

    const char *slash = path;
    const char *last_slash = path;
    while (*slash) {
        if (*slash == '/')
            last_slash = slash + 1;
        slash++;
    }
    ret = mkrn_execve(elf_buf, total, last_slash);

out:
    if (elf_buf)
        mkrn_free(elf_buf);
    mkrn_vfs_close(fd);
    if (ret != 0)
        return (uint32_t)-1;

    mkrn_process_t *pCur = mkrn_process_get_current();
    pCur->state_tags = M4K_SCHED_RUNNING;
    __asm__ volatile(
        "movl %0, %%esp\n"
        "popl %%ebx\n"
        "popl %%esi\n"
        "popl %%edi\n"
        "popl %%ebp\n"
        "ret\n"
        : : "r"(pCur->thread_esp) : "memory"
    );
    __builtin_unreachable();
}

void m4k_syscall_init_handlers(void)
{
    m4k_syscall_register(M4K_SYS_SPAWN, m4k_syscall_spawn_impl);
    m4k_syscall_register(M4K_SYS_EXIT, m4k_syscall_exit_impl);
    m4k_syscall_register(M4K_SYS_READ, m4k_syscall_read_impl);
    m4k_syscall_register(M4K_SYS_WRITE, m4k_syscall_write_impl);
    m4k_syscall_register(M4K_SYS_FORK_ST, m4k_syscall_fork_st_impl);
    m4k_syscall_register(M4K_SYS_WAIT, m4k_syscall_wait_impl);
    m4k_syscall_register(M4K_SYS_KILL, m4k_syscall_kill_impl);
    m4k_syscall_register(M4K_SYS_GETPPID, m4k_syscall_getppid_impl);
    m4k_syscall_register(M4K_SYS_GETPID, m4k_syscall_getpid_impl);
    m4k_syscall_register(M4K_SYS_SETNS, m4k_syscall_setns_impl);
    m4k_syscall_register(M4K_SYS_GETPROCS, m4k_syscall_getprocs_impl);
    m4k_syscall_register(M4K_SYS_GETUID, m4k_syscall_getuid_impl);
    m4k_syscall_register(M4K_SYS_GETEUID, m4k_syscall_geteuid_impl);
    m4k_syscall_register(M4K_SYS_GETGID, m4k_syscall_getgid_impl);
    m4k_syscall_register(M4K_SYS_GETEGID, m4k_syscall_getegid_impl);
    m4k_syscall_register(M4K_SYS_SETUID, m4k_syscall_setuid_impl);
    m4k_syscall_register(M4K_SYS_SETGID, m4k_syscall_setgid_impl);
    m4k_syscall_register(M4K_SYS_GETGROUPS, m4k_syscall_getgroups_impl);
    m4k_syscall_register(M4K_SYS_SETGROUPS, m4k_syscall_setgroups_impl);
    m4k_syscall_register(M4K_SYS_CHMOD, m4k_syscall_chmod_impl);
    m4k_syscall_register(M4K_SYS_CHOWN, m4k_syscall_chown_impl);
    m4k_syscall_register(M4K_SYS_ACCESS, m4k_syscall_access_impl);
    m4k_syscall_register(M4K_SYS_SETRLIMIT, m4k_syscall_setrlimit_impl);
    m4k_syscall_register(M4K_SYS_GETRLIMIT, m4k_syscall_getrlimit_impl);
    m4k_syscall_register(M4K_SYS_BRK, m4k_syscall_brk_impl);
    m4k_syscall_register(M4K_SYS_REGISTER_SESSION, m4k_syscall_register_session_impl);
    m4k_syscall_register(M4K_SYS_GET_SESSION_LIST, m4k_syscall_get_session_list_impl);
    m4k_syscall_register(M4K_SYS_GET_FRAMEBUFFER_INFO, m4k_syscall_get_framebuffer_info_impl);
    m4k_syscall_register(M4K_SYS_DRAW_TEST_PATTERN, m4k_syscall_draw_test_pattern_impl);
    m4k_syscall_register(M4K_SYS_GET_MOUSE_EVENT, m4k_syscall_mouse_event_impl);
    m4k_syscall_register(M4K_SYS_FLIP, m4k_syscall_flip_impl);
    m4k_syscall_register(M4K_SYS_DRAW_RECT, m4k_syscall_draw_rect_impl);
    m4k_syscall_register(M4K_SYS_DRAW_TEXT, m4k_syscall_draw_text_impl);
    m4k_syscall_register(M4K_SYS_GET_KEYBOARD_EVENT, m4k_syscall_keyboard_event_impl);
    m4k_syscall_register(M4K_SYS_GFX_BLIT, m4k_syscall_gfx_blit_impl);

    M4K_LOG_INFO("M4KK1 system call handlers registered");
}
