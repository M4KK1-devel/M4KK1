/*
 * M4KK1 4P1 - m4k_syscall.h
 * Description: M4KK1 native system call numbers (int 0x4D ABI)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Syscall numbers MUST match docs/4P1/standard.md §4.7.2 and §6.12.
 */

#pragma once

#include <stdint.h>

/* ── Process Management (§4.7.2) ── */

#define M4K_SYS_EXIT        0x4D000001
#define M4K_SYS_SPAWN       0x4D000002
#define M4K_SYS_WAIT        0x4D000003
#define M4K_SYS_GETPID      0x4D000004
#define M4K_SYS_KILL        0x4D000005
#define M4K_SYS_GETPPID     0x4D000006
#define M4K_SYS_FORK_ST     0x4D000007
#define M4K_SYS_SETNS       0x4D000008

/* ── File I/O (unallocated range) ── */

#define M4K_SYS_READ        0x4D000009
#define M4K_SYS_WRITE       0x4D00000A
#define M4K_SYS_OPEN        0x4D00000B
#define M4K_SYS_CLOSE       0x4D00000C
#define M4K_SYS_EXEC        0x4D00000D
#define M4K_SYS_MMAP        0x4D00000E
#define M4K_SYS_MUNMAP      0x4D00000F
#define M4K_SYS_MEMINFO     0x4D000010
#define M4K_SYS_IOCTL       0x4D000011
#define M4K_SYS_FCNTL       0x4D000012
#define M4K_SYS_SELECT      0x4D000013
#define M4K_SYS_POLL        0x4D000014
#define M4K_SYS_EPOLL       0x4D000015
#define M4K_SYS_GETPROCS    0x4D000016

/* ── User & Identity Model (§6.12) ── */

#define M4K_SYS_GETUID      0x4D000020
#define M4K_SYS_GETEUID     0x4D000021
#define M4K_SYS_GETGID      0x4D000022
#define M4K_SYS_GETEGID     0x4D000023
#define M4K_SYS_SETUID      0x4D000024
#define M4K_SYS_SETGID      0x4D000025
#define M4K_SYS_GETGROUPS   0x4D000026
#define M4K_SYS_SETGROUPS   0x4D000027
#define M4K_SYS_CHMOD       0x4D000028
#define M4K_SYS_CHOWN       0x4D000029
#define M4K_SYS_ACCESS      0x4D00002A
#define M4K_SYS_SETRLIMIT   0x4D000030
#define M4K_SYS_GETRLIMIT   0x4D000031

/* ── Process cloning flags ── */

#define M4K_CLONE_VM        0x00000100
#define M4K_CLONE_FS        0x00000200
#define M4K_CLONE_FILES     0x00000400
#define M4K_CLONE_SIGHAND   0x00000800
#define M4K_CLONE_THREAD    0x00010000

/* ── File flags ── */

#define M4K_O_RDONLY        0x0001
#define M4K_O_WRONLY        0x0002
#define M4K_O_RDWR          0x0004
#define M4K_O_CREAT         0x0100
#define M4K_O_EXCL          0x0200
#define M4K_O_TRUNC         0x1000
#define M4K_O_APPEND        0x2000
#define M4K_O_NONBLOCK      0x4000

/* ── Memory protection flags (§3.4) ── */

#define M4K_PROT_NONE       0x00
#define M4K_PROT_READ       0x01
#define M4K_PROT_WRITE      0x02
#define M4K_PROT_EXEC       0x04

/* ── Memory mapping flags (§3.5) ── */

#define M4K_MAP_PRIVATE     0x01
#define M4K_MAP_SHARED      0x02
#define M4K_MAP_ANONYMOUS   0x04
#define M4K_MAP_FIXED       0x08
#define M4K_MAP_GROWSDOWN   0x10

/* ── Syscall wrappers ── */

long m4k_syscall0(long syscall_num);
long m4k_syscall1(long syscall_num, long arg1);
long m4k_syscall2(long syscall_num, long arg1, long arg2);
long m4k_syscall3(long syscall_num, long arg1, long arg2, long arg3);
long m4k_syscall4(long syscall_num, long arg1, long arg2, long arg3, long arg4);
long m4k_syscall5(long syscall_num, long arg1, long arg2, long arg3, long arg4, long arg5);
long m4k_syscall6(long syscall_num, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

static inline long m4k_exit(int status) {
    return m4k_syscall1(M4K_SYS_EXIT, status);
}
static inline long m4k_read(int fd, void *buf, unsigned long count) {
    return m4k_syscall3(M4K_SYS_READ, fd, (long)buf, count);
}
static inline long m4k_write(int fd, const void *buf, unsigned long count) {
    return m4k_syscall3(M4K_SYS_WRITE, fd, (long)buf, count);
}
static inline int m4k_open(const char *pathname, int flags) {
    return m4k_syscall2(M4K_SYS_OPEN, (long)pathname, flags);
}
static inline long m4k_close(int fd) {
    return m4k_syscall1(M4K_SYS_CLOSE, fd);
}
static inline void *m4k_mmap(void *addr, unsigned long length, int prot, int flags, int fd, long offset) {
    return (void *)m4k_syscall6(M4K_SYS_MMAP, (long)addr, length, prot, flags, fd, offset);
}
static inline long m4k_munmap(void *addr, unsigned long length) {
    return m4k_syscall2(M4K_SYS_MUNMAP, (long)addr, length);
}

long m4k_clone(unsigned long flags, void *child_stack, void *ptid, void *ctid);
long m4k_fcntl(int fd, int cmd, long arg);
long m4k_ioctl(int fd, unsigned long request, void *arg);
long m4k_execve(const char *filename, char *const argv[], char *const envp[]);
