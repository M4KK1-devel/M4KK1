/*
 * M4KK1 4P1 - syscall.c
 * Description: Standard syscall handler table and dispatch.
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
#include <syscall.h>
#include <idt.h>
#include <ldso.h>
#include <vfs.h>
#include <memory.h>
#include <yafs.h>

extern uint64_t root_yafs_tree;

typedef u32 (*mkrn_syscall_handler_t)(
    u32 arg1, u32 arg2, u32 arg3,
    u32 arg4, u32 arg5);

typedef struct {
    mkrn_syscall_handler_t handler;
    u32 permission_mask;
    const char *pName;
    b registered;
} mkrn_syscall_entry_t;

static const char *mkrn_syscall_get_name(u32 num);

typedef struct {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 esi;
    u32 edi;
    u32 ebp;
    u32 esp;
    u32 eflags;
    u32 cr3;
} mkrn_syscall_context_t;

static mkrn_syscall_entry_t g_syscall_table[256];

static struct {
    u32 total_calls;
    u32 failed_calls;
    u32 permission_denied;
} g_syscall_stats;

#define PERM_LEVEL_KERNEL  0xFFFFFFFF
#define PERM_LEVEL_USER    0x00000001
#define PERM_LEVEL_SYSTEM  0x000000FF

static void mkrn_syscall_table_init(void)
{
    mkrn_memset(g_syscall_table, 0, sizeof(g_syscall_table));
    mkrn_memset(&g_syscall_stats, 0, sizeof(g_syscall_stats));

    M4K_LOG_INFO("System call table initialized");
}

static b mkrn_syscall_check_permission(u32 uNum, u32 uCurPerm)
{
    if (uNum >= 256 || !g_syscall_table[uNum].registered) {
        return false;
    }

    if (uCurPerm == PERM_LEVEL_KERNEL) {
        return true;
    }

    return (uCurPerm & g_syscall_table[uNum].permission_mask) != 0;
}

static void mkrn_syscall_save_registers(u32 *pRegs)
{
    __asm__ volatile (
        "movl %%ebx, 0(%0)\n"
        "movl %%ecx, 4(%0)\n"
        "movl %%edx, 8(%0)\n"
        "movl %%esi, 12(%0)\n"
        "movl %%edi, 16(%0)\n"
        "movl %%ebp, 20(%0)\n"
        : : "r"(pRegs) : "memory"
    );
}

static void mkrn_syscall_restore_registers(u32 *pRegs)
{
    __asm__ volatile (
        "movl 0(%0), %%ebx\n"
        "movl 4(%0), %%ecx\n"
        "movl 8(%0), %%edx\n"
        "movl 12(%0), %%esi\n"
        "movl 16(%0), %%edi\n"
        "movl 20(%0), %%ebp\n"
        : : "r"(pRegs) : "memory"
    );
}

void mkrn_syscall_handler(void)
{
    u32 uSyscallNum;
    u32 uArg1, uArg2, uArg3, uArg4, uArg5;
    u32 uResult = M4K_SC_ERROR;
    u32 uSavedRegs[6];

    __asm__ volatile (
        "movl %%eax, %0\n"
        "movl %%ebx, %1\n"
        "movl %%ecx, %2\n"
        "movl %%edx, %3\n"
        "movl %%esi, %4\n"
        "movl %%edi, %5\n"
        : "=m"(uSyscallNum), "=m"(uArg1), "=m"(uArg2),
          "=m"(uArg3), "=m"(uArg4), "=m"(uArg5)
    );

    g_syscall_stats.total_calls++;

    if (uSyscallNum >= 256) {
        M4K_LOG_WARN("Invalid system call number: 0x");
        mkrn_console_write_hex(uSyscallNum);
        mkrn_console_write("\n");
        g_syscall_stats.failed_calls++;
        goto syscall_return;
    }

    if (!g_syscall_table[uSyscallNum].registered) {
        M4K_LOG_WARN("Unregistered system call: 0x");
        mkrn_console_write_hex(uSyscallNum);
        mkrn_console_write("\n");
        g_syscall_stats.failed_calls++;
        goto syscall_return;
    }

    mkrn_process_t *pProcess = mkrn_process_get_current();
    u32 uCurPerm = (pProcess != NULL) ?
        PERM_LEVEL_USER : PERM_LEVEL_KERNEL;

    if (!mkrn_syscall_check_permission(uSyscallNum, uCurPerm)) {
        M4K_LOG_WARN("Permission denied for system call 0x");
        mkrn_console_write_hex(uSyscallNum);
        mkrn_console_write(" (process: ");
        if (pProcess) {
            mkrn_console_write_hex(pProcess->pid);
        } else {
            mkrn_console_write("kernel");
        }
        mkrn_console_write(")\n");

        g_syscall_stats.permission_denied++;
        uResult = M4K_SC_ERROR;
        goto syscall_return;
    }

    mkrn_syscall_save_registers(uSavedRegs);

    mkrn_syscall_handler_t pHandler =
        g_syscall_table[uSyscallNum].handler;
    if (pHandler != NULL) {
        uResult = pHandler(uArg1, uArg2, uArg3, uArg4, uArg5);
    } else {
        M4K_LOG_ERROR("System call handler is NULL for 0x");
        mkrn_console_write_hex(uSyscallNum);
        mkrn_console_write("\n");
        g_syscall_stats.failed_calls++;
        uResult = M4K_SC_ERROR;
    }

syscall_return:
    __asm__ volatile ("movl %0, %%eax" : : "r"(uResult));
    mkrn_syscall_restore_registers(uSavedRegs);
}

static void mkrn_syscall_init_handlers(void);

extern void isr_syscall(void);

void mkrn_syscall_init(void)
{
    mkrn_syscall_table_init();

    mkrn_idt_set_gate(0x80, (u32)isr_syscall, 0x08,
        M4K_IDT_PRESENT | M4K_IDT_DPL_3 |
        M4K_IDT_INTERRUPT_GATE_32);

    mkrn_idt_register_handler(0x80, mkrn_syscall_handler);

    mkrn_syscall_init_handlers();

    M4K_LOG_INFO("System call system initialized");
}

void mkrn_syscall_register(u32 uNum, void *pHandler)
{
    if (uNum >= 256) {
        M4K_LOG_ERROR(
            "Invalid system call number for registration: 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
        return;
    }

    if (pHandler == NULL) {
        M4K_LOG_ERROR(
            "Cannot register NULL handler for system call 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
        return;
    }

    g_syscall_table[uNum].handler =
        (mkrn_syscall_handler_t)pHandler;
    g_syscall_table[uNum].registered = true;
    g_syscall_table[uNum].permission_mask = PERM_LEVEL_USER;
    g_syscall_table[uNum].pName = mkrn_syscall_get_name(uNum);

    M4K_LOG_INFO("System call 0x");
    mkrn_console_write_hex(uNum);
    mkrn_console_write(" registered: ");
    mkrn_console_write(
        g_syscall_table[uNum].pName
            ? g_syscall_table[uNum].pName : "Unknown");
    mkrn_console_write("\n");
}

void mkrn_syscall_unregister(u32 uNum)
{
    if (uNum >= 256) {
        M4K_LOG_ERROR(
            "Invalid system call number for "
            "unregistration: 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
        return;
    }

    if (!g_syscall_table[uNum].registered) {
        M4K_LOG_WARN("System call 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write(" is not registered\n");
        return;
    }

    g_syscall_table[uNum].handler = NULL;
    g_syscall_table[uNum].registered = false;
    g_syscall_table[uNum].pName = NULL;
    g_syscall_table[uNum].permission_mask = 0;

    M4K_LOG_INFO("System call 0x");
    mkrn_console_write_hex(uNum);
    mkrn_console_write(" unregistered\n");
}

u32 mkrn_syscall_execute(u32 uNum, u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uResult;

    if (uNum >= 256 || !g_syscall_table[uNum].registered) {
        M4K_LOG_ERROR(
            "Cannot execute unregistered system call: 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
        return M4K_SC_ERROR;
    }

    __asm__ volatile (
        "int $0x80\n"
        : "=a"(uResult)
        : "a"(uNum), "b"(uArg1), "c"(uArg2), "d"(uArg3),
          "S"(uArg4), "D"(uArg5)
    );

    return uResult;
}

static const char *mkrn_syscall_get_name(u32 uNum)
{
    switch (uNum) {
        case M4K_SC_EXIT: return "exit";
        case M4K_SC_FORK: return "fork";
        case M4K_SC_READ: return "read";
        case M4K_SC_WRITE: return "write";
        case M4K_SC_OPEN: return "open";
        case M4K_SC_CLOSE: return "close";
        case M4K_SC_WAITPID: return "waitpid";
        case M4K_SC_EXECVE: return "execve";
        case M4K_SC_GETPID: return "getpid";
        case M4K_SC_GETPPID: return "getppid";
        case M4K_SC_BRK: return "brk";
        case M4K_SC_MMAP: return "mmap";
        case M4K_SC_MUNMAP: return "munmap";
        case M4K_SC_MPROTECT: return "mprotect";
        case M4K_SC_MSYNC: return "msync";
        case M4K_SC_GETCWD: return "getcwd";
        case M4K_SC_CHDIR: return "chdir";
        case M4K_SC_MKDIR: return "mkdir";
        case M4K_SC_RMDIR: return "rmdir";
        case M4K_SC_LINK: return "link";
        case M4K_SC_UNLINK: return "unlink";
        case M4K_SC_RENAME: return "rename";
        case M4K_SC_STAT: return "stat";
        case M4K_SC_FSTAT: return "fstat";
        case M4K_SC_LSTAT: return "lstat";
        case M4K_SC_ACCESS: return "access";
        case M4K_SC_CHMOD: return "chmod";
        case M4K_SC_CHOWN: return "chown";
        case M4K_SC_UTIME: return "utime";
        case M4K_SC_TIME: return "time";
        case M4K_SC_TIMES: return "times";
        case M4K_SC_GETUID: return "getuid";
        case M4K_SC_GETGID: return "getgid";
        case M4K_SC_SETUID: return "setuid";
        case M4K_SC_SETGID: return "setgid";
        case M4K_SC_GETEUID: return "geteuid";
        case M4K_SC_GETEGID: return "getegid";
        case M4K_SC_SETEUID: return "seteuid";
        case M4K_SC_SETEGID: return "setegid";
        case M4K_SC_PIPE: return "pipe";
        case M4K_SC_DUP: return "dup";
        case M4K_SC_DUP2: return "dup2";
        case M4K_SC_SELECT: return "select";
        case M4K_SC_POLL: return "poll";
        case M4K_SC_EPOLL_CREATE: return "epoll_create";
        case M4K_SC_EPOLL_CTL: return "epoll_ctl";
        case M4K_SC_EPOLL_WAIT: return "epoll_wait";
        case M4K_SC_SOCKET: return "socket";
        case M4K_SC_BIND: return "bind";
        case M4K_SC_LISTEN: return "listen";
        case M4K_SC_ACCEPT: return "accept";
        case M4K_SC_CONNECT: return "connect";
        case M4K_SC_SEND: return "send";
        case M4K_SC_RECV: return "recv";
        case M4K_SC_SENDTO: return "sendto";
        case M4K_SC_RECVFROM: return "recvfrom";
        case M4K_SC_SHUTDOWN: return "shutdown";
        case M4K_SC_SETSOCKOPT: return "setsockopt";
        case M4K_SC_GETSOCKOPT: return "getsockopt";
        case M4K_SC_IOCTL: return "ioctl";
        case M4K_SC_FCNTL: return "fcntl";
        case M4K_SC_READDIR: return "readdir";
        case M4K_SC_TELLDIR: return "telldir";
        case M4K_SC_SEEKDIR: return "seekdir";
        case M4K_SC_CLOSEDIR: return "closedir";
        case M4K_SC_OPENDIR: return "opendir";
        case M4K_SC_MKNOD: return "mknod";
        case M4K_SC_MKFIFO: return "mkfifo";
        case M4K_SC_TRUNCATE: return "truncate";
        case M4K_SC_FTRUNCATE: return "ftruncate";
        case M4K_SC_GETDENTS: return "getdents";
        case M4K_SC_SYNC: return "sync";
        case M4K_SC_FSYNC: return "fsync";
        case M4K_SC_FDATASYNC: return "fdatasync";
        case M4K_SC_MLOCK: return "mlock";
        case M4K_SC_MUNLOCK: return "munlock";
        case M4K_SC_MLOCKALL: return "mlockall";
        case M4K_SC_MUNLOCKALL: return "munlockall";
        case M4K_SC_NANOSLEEP: return "nanosleep";
        case M4K_SC_CLOCK_GETTIME: return "clock_gettime";
        case M4K_SC_CLOCK_SETTIME: return "clock_settime";
        case M4K_SC_CLOCK_GETRES: return "clock_getres";
        case M4K_SC_SCHED_YIELD: return "sched_yield";
        case M4K_SC_SCHED_SETSCHEDULER:
            return "sched_setscheduler";
        case M4K_SC_SCHED_GETSCHEDULER:
            return "sched_getscheduler";
        case M4K_SC_SCHED_SETPARAM:
            return "sched_setparam";
        case M4K_SC_SCHED_GETPARAM:
            return "sched_getparam";
        case M4K_SC_SCHED_SETAFFINITY:
            return "sched_setaffinity";
        case M4K_SC_SCHED_GETAFFINITY:
            return "sched_getaffinity";
        case M4K_SC_PRLIMIT64: return "prlimit64";
        case M4K_SC_GETRUSAGE: return "getrusage";
        case M4K_SC_GETTIMEOFDAY: return "gettimeofday";
        case M4K_SC_SETTIMEOFDAY: return "settimeofday";
        case M4K_SC_ADJTIMEX: return "adjtimex";
        case M4K_SC_TIMER_CREATE: return "timer_create";
        case M4K_SC_TIMER_DELETE: return "timer_delete";
        case M4K_SC_TIMER_SETTIME: return "timer_settime";
        case M4K_SC_TIMER_GETTIME: return "timer_gettime";
        case M4K_SC_TIMER_GETOVERRUN:
            return "timer_getoverrun";
        case M4K_SC_KILL: return "kill";
        case M4K_SC_TKILL: return "tkill";
        case M4K_SC_TGKILL: return "tgkill";
        case M4K_SC_SIGACTION: return "sigaction";
        case M4K_SC_SIGPROCMASK: return "sigprocmask";
        case M4K_SC_SIGPENDING: return "sigpending";
        case M4K_SC_SIGSUSPEND: return "sigsuspend";
        case M4K_SC_SIGTIMEDWAIT: return "sigtimedwait";
        case M4K_SC_SIGRETURN: return "sigreturn";
        case M4K_SC_REBOOT: return "reboot";
        case M4K_SC_KEXEC_LOAD: return "kexec_load";
        case M4K_SC_EXIT_GROUP: return "exit_group";
        case M4K_SC_WAIT4: return "wait4";
        case M4K_SC_CLONE: return "clone";
        case M4K_SC_VFORK: return "vfork";
        case M4K_SC_UNAME: return "uname";
        case M4K_SC_SEMGET: return "semget";
        case M4K_SC_SEMOP: return "semop";
        case M4K_SC_SEMCTL: return "semctl";
        case M4K_SC_SEMTIMEDOP: return "semtimedop";
        case M4K_SC_MSGGET: return "msgget";
        case M4K_SC_MSGSND: return "msgsnd";
        case M4K_SC_MSGRCV: return "msgrcv";
        case M4K_SC_MSGCTL: return "msgctl";
        case M4K_SC_SHMGET: return "shmget";
        case M4K_SC_SHMAT: return "shmat";
        case M4K_SC_SHMDT: return "shmdt";
        case M4K_SC_SHMCTL: return "shmctl";
        case M4K_SC_DL_LOAD_LIBRARY:
            return "dl_load_library";
        case M4K_SC_DL_UNLOAD_LIBRARY:
            return "dl_unload_library";
        case M4K_SC_DL_FIND_SYMBOL:
            return "dl_find_symbol";
        case M4K_SC_DL_GET_ERROR: return "dl_get_error";
        case M4K_SC_SYSINFO: return "sysinfo";
        case M4K_SC_GETPROCS: return "getprocs";
        case M4K_SC_STATFS: return "statfs";
        case M4K_SC_MOUNT: return "mount";
        case M4K_SC_UMOUNT: return "umount";
        case M4K_SC_MOUNTINFO: return "mountinfo";
        default: return "unknown";
    }
}

void mkrn_syscall_get_stats(u32 *pTotal, u32 *pFailed,
    u32 *pDenied)
{
    if (pTotal)  *pTotal = g_syscall_stats.total_calls;
    if (pFailed) *pFailed = g_syscall_stats.failed_calls;
    if (pDenied) *pDenied =
        g_syscall_stats.permission_denied;
}

void mkrn_syscall_set_permission(u32 uNum,
    u32 uPermMask)
{
    if (uNum >= 256) {
        M4K_LOG_ERROR(
            "Invalid system call number for "
            "permission setting: 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
        return;
    }

    if (!g_syscall_table[uNum].registered) {
        M4K_LOG_WARN(
            "Setting permission for unregistered "
            "system call 0x");
        mkrn_console_write_hex(uNum);
        mkrn_console_write("\n");
    }

    g_syscall_table[uNum].permission_mask = uPermMask;

    M4K_LOG_INFO("Permission mask set for system call 0x");
    mkrn_console_write_hex(uNum);
    mkrn_console_write(" to 0x");
    mkrn_console_write_hex(uPermMask);
    mkrn_console_write("\n");
}

b mkrn_syscall_is_registered(u32 uNum)
{
    return (uNum < 256) && g_syscall_table[uNum].registered;
}

mkrn_syscall_handler_t mkrn_syscall_get_handler(u32 uNum)
{
    if (uNum >= 256 ||
        !g_syscall_table[uNum].registered) {
        return NULL;
    }
    return g_syscall_table[uNum].handler;
}

void mkrn_syscall_print_status(void)
{
    u32 uI;
    u32 uRegCount = 0;

    M4K_LOG_INFO("=== System Call Status ===");

    M4K_LOG_INFO("Statistics:");
    M4K_LOG_INFO("  Total calls: ");
    mkrn_console_write_dec(g_syscall_stats.total_calls);
    mkrn_console_write("\n");

    M4K_LOG_INFO("  Failed calls: ");
    mkrn_console_write_dec(g_syscall_stats.failed_calls);
    mkrn_console_write("\n");

    M4K_LOG_INFO("  Permission denied: ");
    mkrn_console_write_dec(
        g_syscall_stats.permission_denied);
    mkrn_console_write("\n");

    M4K_LOG_INFO("Registered system calls:");
    for (uI = 0; uI < 256; uI++) {
        if (g_syscall_table[uI].registered) {
            M4K_LOG_INFO("  0x");
            mkrn_console_write_hex(uI);
            mkrn_console_write(" - ");
            mkrn_console_write(
                mkrn_syscall_get_name(uI));
            mkrn_console_write(" (handler: 0x");
            mkrn_console_write_hex(
                (u32)g_syscall_table[uI].handler);
            mkrn_console_write(")\n");
            uRegCount++;
        }
    }

    M4K_LOG_INFO(
        "Total registered system calls: ");
    mkrn_console_write_dec(uRegCount);
    mkrn_console_write("\n");
    M4K_LOG_INFO("=========================");
}

static u32 mkrn_syscall_exit_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uStatus = uArg1;

    M4K_LOG_INFO("Process exit called with status: ");
    mkrn_console_write_dec(uStatus);
    mkrn_console_write("\n");

    mkrn_process_exit();

    return 0;
}

static u32 mkrn_syscall_fork_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg1; (void)uArg2; (void)uArg3;
    (void)uArg4; (void)uArg5;
    M4K_LOG_INFO(
        "Fork system call invoked - not implemented\n");
    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_getpid_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uPid = mkrn_process_get_pid();

    M4K_LOG_DEBUG("GetPID system call: returning ");
    mkrn_console_write_dec(uPid);
    mkrn_console_write("\n");

    return uPid;
}

static u32 mkrn_syscall_getppid_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg1; (void)uArg2; (void)uArg3;
    (void)uArg4; (void)uArg5;
    u32 uPPid = 0;
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (pCur) uPPid = pCur->ppid;

    M4K_LOG_DEBUG("GetPPID system call: returning ");
    mkrn_console_write_dec(uPPid);
    mkrn_console_write("\n");

    return uPPid;
}

#define COM1_DATA 0x3F8
#define COM1_LSR  0x3FD
#define LSR_DR    0x01

static inline u8 inb(u16 pPort)
{
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(pPort));
    return r;
}

static u32 mkrn_syscall_read_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uFd = uArg1;
    void *pBuf = (void *)uArg2;
    u32 uCount = uArg3;

    if (uFd == 0 && pBuf && uCount > 0 &&
        !mkrn_vfs_fd_in_use(0)) {
        u8 *pByte = (u8 *)pBuf;
        u32 uI;
        for (uI = 0; uI < uCount; uI++) {
            while (!(inb(COM1_LSR) & LSR_DR));
            pByte[uI] = inb(COM1_DATA);
        }
        return uI;
    }

    if (pBuf && uCount > 0) {
        return (u32)mkrn_vfs_read(
            (int)uFd, pBuf, (size_t)uCount);
    }

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_write_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uFd = uArg1;
    const void *pBuf = (const void *)uArg2;
    u32 uCount = uArg3;

    if (uFd == 1 && pBuf &&
        !mkrn_vfs_fd_in_use(1)) {
        char *pStr = (char *)pBuf;
        u32 uI;
        for (uI = 0; uI < uCount && pStr[uI]; uI++) {
            mkrn_console_put_char(pStr[uI]);
        }
        return uI;
    }

    if (pBuf && uCount > 0) {
        return (u32)mkrn_vfs_write(
            (int)uFd, pBuf, (size_t)uCount);
    }

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_open_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg3; (void)uArg4; (void)uArg5;
    const char *pPath = (const char *)uArg1;
    int iFlags = (int)uArg2;

    int iFd = mkrn_vfs_open(pPath, iFlags);
    return (iFd >= 0) ? (u32)iFd : M4K_SC_ERROR;
}

static u32 mkrn_syscall_close_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    int iFd = (int)uArg1;

    return (mkrn_vfs_close(iFd) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_getdents_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg4; (void)uArg5;
    int iFd = (int)uArg1;
    struct mkrn_vfs_dirent *pBuf =
        (struct mkrn_vfs_dirent *)uArg2;
    u32 uCount = uArg3;

    int iN = mkrn_vfs_getdents(iFd, pBuf, uCount);
    return (iN >= 0) ? (u32)iN : M4K_SC_ERROR;
}

static u32 mkrn_syscall_execve_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pFilename = (const char *)uArg1;
    char *const *pArgv = (char *const *)uArg2;
    char *const *pEnvp = (char *const *)uArg3;

    M4K_LOG_DEBUG(
        "Execve system call: filename=");
    if (pFilename) {
        mkrn_console_write(pFilename);
    } else {
        mkrn_console_write("(null)");
    }
    mkrn_console_write("\n");

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_waitpid_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uPid = uArg1;
    void *pStatus = (void *)uArg2;
    u32 uOptions = uArg3;

    M4K_LOG_DEBUG("Waitpid system call: pid=");
    mkrn_console_write_dec(uPid);
    mkrn_console_write(", options=");
    mkrn_console_write_hex(uOptions);
    mkrn_console_write("\n");

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_brk_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uAddr = uArg1;

    M4K_LOG_DEBUG("Brk system call: addr=0x");
    mkrn_console_write_hex(uAddr);
    mkrn_console_write("\n");

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_getcwd_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    char *pBuf = (char *)uArg1;
    u32 uSize = uArg2;
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (!pBuf || !pCur || uSize == 0) return M4K_SC_ERROR;
    int iI;
    for (iI = 0;
         iI < (int)uSize - 1 && pCur->cwd[iI]; iI++)
        pBuf[iI] = pCur->cwd[iI];
    pBuf[iI] = '\0';
    return iI;
}

static void mkrn_resolve_path(const char *pCwd,
    const char *pIn, char *pOut, int iOSize)
{
    if (!pIn || !*pIn) {
        int iI;
        for (iI = 0;
             pCwd[iI] && iI < iOSize - 1; iI++)
            pOut[iI] = pCwd[iI];
        pOut[iI] = '\0';
        return;
    }

    char tmp[512];
    int ti = 0;
    if (pIn[0] == '/') {
        for (int iI = 0;
             pIn[iI] && ti < 510; iI++)
            tmp[ti++] = pIn[iI];
    } else {
        for (int iI = 0;
             pCwd[iI] && ti < 510; iI++)
            tmp[ti++] = pCwd[iI];
        if (ti > 0 && tmp[ti - 1] != '/'
            && ti < 510)
            tmp[ti++] = '/';
        for (int iI = 0;
             pIn[iI] && ti < 510; iI++)
            tmp[ti++] = pIn[iI];
    }
    tmp[ti] = '\0';

    char *parts[64];
    int np = 0;
    char *pP = tmp;
    while (*pP) {
        while (*pP == '/') pP++;
        if (!*pP) break;
        char *pStart = pP;
        while (*pP && *pP != '/') pP++;
        int saved = *pP;
        *pP = '\0';
        if (pStart[0] == '.' && pStart[1] == '\0') {
            if (saved) pP++;
            continue;
        }
        if (pStart[0] == '.' && pStart[1] == '.'
            && pStart[2] == '\0') {
            if (np > 0) np--;
            if (saved) pP++;
            continue;
        }
        if (np < 64) parts[np++] = pStart;
        if (saved) pP++;
    }

    int oi = 0;
    pOut[oi++] = '/';
    for (int iI = 0;
         iI < np && oi < iOSize - 1; iI++) {
        char *pS = parts[iI];
        while (*pS && oi < iOSize - 1)
            pOut[oi++] = *pS++;
        if (iI < np - 1 && oi < iOSize - 1)
            pOut[oi++] = '/';
    }
    pOut[oi] = '\0';
}

static u32 mkrn_syscall_chdir_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pPath = (const char *)uArg1;
    mkrn_process_t *pCur = mkrn_process_get_current();
    if (!pPath || !pCur) return M4K_SC_ERROR;
    if (root_yafs_tree != 0) {
        char resolved[256];
        mkrn_resolve_path(
            pCur->cwd, pPath, resolved, 256);
        uint64_t uInode =
            mkrn_yafs_lookup_path(
                root_yafs_tree, resolved);
        if (uInode == 0) return M4K_SC_ERROR;
        int iI;
        for (iI = 0;
             iI < 254 && resolved[iI]; iI++)
            pCur->cwd[iI] = resolved[iI];
        pCur->cwd[iI] = '\0';
        return M4K_SC_SUCCESS;
    }
    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_mkdir_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pPath = (const char *)uArg1;
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    return (mkrn_vfs_mkdir(pPath) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_unlink_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pPath = (const char *)uArg1;
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    return (mkrn_vfs_unlink(pPath) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_rmdir_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pPath = (const char *)uArg1;
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    return (mkrn_vfs_rmdir(pPath) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_rename_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    const char *pOld = (const char *)uArg1;
    const char *pNew = (const char *)uArg2;
    (void)uArg3; (void)uArg4; (void)uArg5;
    return (mkrn_vfs_rename(pOld, pNew) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_pipe_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    int *piFds = (int *)uArg1;
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    if (!piFds) return M4K_SC_ERROR;
    int iFd[2];
    if (mkrn_vfs_pipe(iFd) != 0) return M4K_SC_ERROR;
    piFds[0] = iFd[0];
    piFds[1] = iFd[1];
    return M4K_SC_SUCCESS;
}

static u32 mkrn_syscall_dup2_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    int iOldfd = (int)uArg1;
    int iNewfd = (int)uArg2;
    (void)uArg3; (void)uArg4; (void)uArg5;
    int iR = mkrn_vfs_dup2(iOldfd, iNewfd);
    return (iR >= 0) ? (u32)iR : M4K_SC_ERROR;
}

static u32 mkrn_syscall_time_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    void *pTloc = (void *)uArg1;

    M4K_LOG_DEBUG("Time system call\n");

    u32 uCurTime = 1234567890;

    if (pTloc) {
        *(u32 *)pTloc = uCurTime;
    }

    return uCurTime;
}

struct mkrn_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static u32 mkrn_syscall_uname_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    struct mkrn_utsname *pU =
        (struct mkrn_utsname *)uArg1;
    if (!pU) return M4K_SC_ERROR;
    const char *pSysname = "M4KK1";
    const char *pNodename = "m4kk1";
    const char *pRelease = "1.0.0";
    const char *pVersion = "M4KK1-Y4KU";
    const char *pMachine = "i386";
    int iI;
    for (iI = 0; pSysname[iI] && iI < 64; iI++)
        pU->sysname[iI] = pSysname[iI];
    pU->sysname[iI] = '\0';
    for (iI = 0; pNodename[iI] && iI < 64; iI++)
        pU->nodename[iI] = pNodename[iI];
    pU->nodename[iI] = '\0';
    for (iI = 0; pRelease[iI] && iI < 64; iI++)
        pU->release[iI] = pRelease[iI];
    pU->release[iI] = '\0';
    for (iI = 0; pVersion[iI] && iI < 64; iI++)
        pU->version[iI] = pVersion[iI];
    pU->version[iI] = '\0';
    for (iI = 0; pMachine[iI] && iI < 64; iI++)
        pU->machine[iI] = pMachine[iI];
    pU->machine[iI] = '\0';
    return M4K_SC_SUCCESS;
}

static u32 mkrn_syscall_reboot_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    u32 uMagic1 = uArg1;
    u32 uMagic2 = uArg2;
    u32 uCmd = uArg3;

    M4K_LOG_INFO("Reboot system call: magic1=0x");
    mkrn_console_write_hex(uMagic1);
    mkrn_console_write(", magic2=0x");
    mkrn_console_write_hex(uMagic2);
    mkrn_console_write(", cmd=");
    mkrn_console_write_dec(uCmd);
    mkrn_console_write("\n");

    if (uMagic1 == 0x01234567 &&
        uMagic2 == 0x89ABCDEF) {
        M4K_LOG_INFO("Rebooting system...\n");

        __asm__ volatile (
            "movl $0x64, %eax\n"
            "outb %al, $0xFE\n"
        );

        __asm__ volatile (
            "movb $0x02, %al\n"
            "outb %al, $0x64\n"
            "movb $0xFE, %al\n"
            "outb %al, $0x64\n"
        );

        return 0;
    }

    return M4K_SC_ERROR;
}

static u32 mkrn_syscall_sysinfo_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    struct mkrn_sysinfo *pInfo =
        (struct mkrn_sysinfo *)uArg1;
    if (!pInfo) return M4K_SC_ERROR;
    pInfo->total_ram = mkrn_memory_get_total();
    pInfo->free_ram = mkrn_memory_get_free();
    pInfo->used_ram = mkrn_memory_get_used();
    pInfo->process_count = mkrn_process_get_count();
    return M4K_SC_SUCCESS;
}

static u32 mkrn_syscall_getprocs_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg3; (void)uArg4; (void)uArg5;
    struct procinfo *pBuf =
        (struct procinfo *)uArg1;
    u32 uMax = uArg2;
    if (!pBuf || uMax == 0) return M4K_SC_ERROR;
    return (u32)mkrn_process_fill_info(pBuf, (int)uMax);
}

static u32 mkrn_syscall_statfs_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    struct mkrn_statfs *pBuf =
        (struct mkrn_statfs *)uArg1;
    if (!pBuf) return M4K_SC_ERROR;
    u32 uBs, uTb, uFb, uUb;
    if (mkrn_yafs_fs_stats(
            &uBs, &uTb, &uFb, &uUb) != 0)
        return M4K_SC_ERROR;
    pBuf->block_size = uBs;
    pBuf->total_blocks = uTb;
    pBuf->free_blocks = uFb;
    pBuf->used_blocks = uUb;
    return M4K_SC_SUCCESS;
}

static u32 mkrn_syscall_mount_impl(u32 uArg1, u32 uArg2,
    u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg4; (void)uArg5;
    const char *pSource = (const char *)uArg1;
    const char *pTarget = (const char *)uArg2;
    const char *pFstype = (const char *)uArg3;
    if (!pSource || !pTarget || !pFstype)
        return M4K_SC_ERROR;
    return (mkrn_vfs_mount(
                pSource, pTarget, pFstype) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_umount_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    const char *pTarget = (const char *)uArg1;
    if (!pTarget) return M4K_SC_ERROR;
    return (mkrn_vfs_umount(pTarget) == 0)
        ? M4K_SC_SUCCESS : M4K_SC_ERROR;
}

static u32 mkrn_syscall_mountinfo_impl(u32 uArg1,
    u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg3; (void)uArg4; (void)uArg5;
    mkrn_mount_ent_t *pBuf =
        (mkrn_mount_ent_t *)uArg1;
    u32 uMax = uArg2;
    if (!pBuf || uMax == 0) return M4K_SC_ERROR;
    return (u32)mkrn_vfs_get_mount_info(pBuf, (int)uMax);
}

static void mkrn_syscall_init_handlers(void)
{
    mkrn_syscall_register(
        M4K_SC_EXIT, mkrn_syscall_exit_impl);
    mkrn_syscall_register(
        M4K_SC_FORK, mkrn_syscall_fork_impl);
    mkrn_syscall_register(
        M4K_SC_READ, mkrn_syscall_read_impl);
    mkrn_syscall_register(
        M4K_SC_WRITE, mkrn_syscall_write_impl);
    mkrn_syscall_register(
        M4K_SC_OPEN, mkrn_syscall_open_impl);
    mkrn_syscall_register(
        M4K_SC_CLOSE, mkrn_syscall_close_impl);
    mkrn_syscall_register(
        M4K_SC_WAITPID, mkrn_syscall_waitpid_impl);
    mkrn_syscall_register(
        M4K_SC_EXECVE, mkrn_syscall_execve_impl);
    mkrn_syscall_register(
        M4K_SC_GETPID, mkrn_syscall_getpid_impl);
    mkrn_syscall_register(
        M4K_SC_GETPPID, mkrn_syscall_getppid_impl);
    mkrn_syscall_register(
        M4K_SC_BRK, mkrn_syscall_brk_impl);
    mkrn_syscall_register(
        M4K_SC_GETCWD, mkrn_syscall_getcwd_impl);
    mkrn_syscall_register(
        M4K_SC_CHDIR, mkrn_syscall_chdir_impl);
    mkrn_syscall_register(
        M4K_SC_MKDIR, mkrn_syscall_mkdir_impl);
    mkrn_syscall_register(
        M4K_SC_UNLINK, mkrn_syscall_unlink_impl);
    mkrn_syscall_register(
        M4K_SC_RMDIR, mkrn_syscall_rmdir_impl);
    mkrn_syscall_register(
        M4K_SC_RENAME, mkrn_syscall_rename_impl);
    mkrn_syscall_register(
        M4K_SC_TIME, mkrn_syscall_time_impl);
    mkrn_syscall_register(
        M4K_SC_UNAME, mkrn_syscall_uname_impl);
    mkrn_syscall_register(
        M4K_SC_REBOOT, mkrn_syscall_reboot_impl);
    mkrn_syscall_register(
        M4K_SC_GETDENTS, mkrn_syscall_getdents_impl);
    mkrn_syscall_register(
        M4K_SC_SYSINFO, mkrn_syscall_sysinfo_impl);
    mkrn_syscall_register(
        M4K_SC_GETPROCS, mkrn_syscall_getprocs_impl);
    mkrn_syscall_register(
        M4K_SC_STATFS, mkrn_syscall_statfs_impl);
    mkrn_syscall_register(
        M4K_SC_PIPE, mkrn_syscall_pipe_impl);
    mkrn_syscall_register(
        M4K_SC_DUP2, mkrn_syscall_dup2_impl);
    mkrn_syscall_register(
        M4K_SC_MOUNT, mkrn_syscall_mount_impl);
    mkrn_syscall_register(
        M4K_SC_UMOUNT, mkrn_syscall_umount_impl);
    mkrn_syscall_register(
        M4K_SC_MOUNTINFO, mkrn_syscall_mountinfo_impl);

    M4K_LOG_INFO("System call handlers registered");
}

static u32 mkrn_syscall_dl_load_library_impl(
    u32 uArg1, u32 uArg2, u32 uArg3,
    u32 uArg4, u32 uArg5)
{
    const char *pFilename = (const char *)uArg1;

    M4K_LOG_DEBUG("DL Load Library system call");
    if (pFilename) {
        mkrn_console_write(pFilename);
    } else {
        mkrn_console_write("(null)");
    }
    mkrn_console_write("\n");

    if (!pFilename) {
        return M4K_SC_ERROR;
    }

    mkrn_ll_library_t *pLib;
    if (mkrn_ll_load_library(pFilename, &pLib) < 0) {
        M4K_LOG_ERROR("Failed to load library");
        return M4K_SC_ERROR;
    }

    return (u32)pLib;
}

static u32 mkrn_syscall_dl_unload_library_impl(
    u32 uArg1, u32 uArg2, u32 uArg3,
    u32 uArg4, u32 uArg5)
{
    mkrn_ll_library_t *pLib =
        (mkrn_ll_library_t *)uArg1;

    M4K_LOG_DEBUG("DL Unload Library system call");
    mkrn_console_write_hex((u32)pLib);
    mkrn_console_write("\n");

    if (!pLib) {
        return M4K_SC_ERROR;
    }

    if (mkrn_ll_unload_library(pLib) < 0) {
        M4K_LOG_ERROR("Failed to unload library");
        return M4K_SC_ERROR;
    }

    return M4K_SC_SUCCESS;
}

static u32 mkrn_syscall_dl_find_symbol_impl(
    u32 uArg1, u32 uArg2, u32 uArg3,
    u32 uArg4, u32 uArg5)
{
    const char *pSymbol = (const char *)uArg1;

    M4K_LOG_DEBUG("DL Find Symbol system call");
    if (pSymbol) {
        mkrn_console_write(pSymbol);
    } else {
        mkrn_console_write("(null)");
    }
    mkrn_console_write("\n");

    if (!pSymbol) {
        return M4K_SC_ERROR;
    }

    void *pAddr = mkrn_ll_find_symbol(pSymbol);
    if (!pAddr) {
        M4K_LOG_DEBUG("Symbol not found");
        return M4K_SC_ERROR;
    }

    return (u32)pAddr;
}

static u32 mkrn_syscall_dl_get_error_impl(
    u32 uArg1, u32 uArg2, u32 uArg3,
    u32 uArg4, u32 uArg5)
{
    char *pBuf = (char *)uArg1;
    u32 uSize = uArg2;

    M4K_LOG_DEBUG("DL Get Error system call");
    mkrn_console_write_hex((u32)pBuf);
    mkrn_console_write(", size=");
    mkrn_console_write_dec(uSize);
    mkrn_console_write("\n");

    if (!pBuf || uSize == 0) {
        return M4K_SC_ERROR;
    }

    size_t uErrLen = mkrn_strlen(mkrn_ll_error_msg);
    if (uErrLen >= uSize) {
        uErrLen = uSize - 1;
    }

    mkrn_memcpy(pBuf, mkrn_ll_error_msg, uErrLen);
    pBuf[uErrLen] = '\0';

    return uErrLen;
}
