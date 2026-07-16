/*
 * M4KK1 4P1 - m4k_syscall.h
 * Description: M4KK1 system call ABI interface definitions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

/* System call numbers use 0x4D0xxxx range (4D = M4) */
#define M4K_SYS_EXIT        0x4D000001
#define M4K_SYS_FORK        0x4D000002
#define M4K_SYS_READ        0x4D000003
#define M4K_SYS_WRITE       0x4D000004
#define M4K_SYS_OPEN        0x4D000005
#define M4K_SYS_CLOSE       0x4D000006
#define M4K_SYS_EXEC        0x4D000007
#define M4K_SYS_MMAP        0x4D000008
#define M4K_SYS_MUNMAP      0x4D000009
#define M4K_SYS_IOCTL       0x4D00000A
#define M4K_SYS_FCNTL       0x4D00000B
#define M4K_SYS_SELECT      0x4D00000C
#define M4K_SYS_POLL        0x4D00000D
#define M4K_SYS_EPOLL       0x4D00000E

/* Process cloning flags */
#define M4K_CLONE_VM        0x00000100  /* Share virtual memory */
#define M4K_CLONE_FS        0x00000200  /* Share filesystem info */
#define M4K_CLONE_FILES     0x00000400  /* Share file descriptor table */
#define M4K_CLONE_SIGHAND   0x00000800  /* Share signal handlers */
#define M4K_CLONE_THREAD    0x00010000  /* Create thread */

/* File flags */
#define M4K_O_RDONLY        0x00000001
#define M4K_O_WRONLY        0x00000002
#define M4K_O_RDWR          0x00000004
#define M4K_O_CREAT         0x00000100
#define M4K_O_EXCL          0x00000200
#define M4K_O_TRUNC         0x00001000
#define M4K_O_APPEND        0x00002000
#define M4K_O_NONBLOCK      0x00004000

/* Memory protection flags */
#define M4K_PROT_NONE       0x00
#define M4K_PROT_READ       0x01
#define M4K_PROT_WRITE      0x02
#define M4K_PROT_EXEC       0x04
#define M4K_PROT_GROWSDOWN  0x01000000
#define M4K_PROT_GROWSUP    0x02000000

/* Memory mapping flags */
#define M4K_MAP_SHARED      0x01
#define M4K_MAP_PRIVATE     0x02
#define M4K_MAP_FIXED       0x10
#define M4K_MAP_ANONYMOUS   0x20
#define M4K_MAP_GROWSDOWN   0x0100
#define M4K_MAP_GROWSUP     0x0200
#define M4K_MAP_LOCKED      0x2000

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
