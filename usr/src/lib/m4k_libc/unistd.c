/*
 * M4KK1 4P1 - unistd.c
 * Description: Minimal unistd implementation for M4KK1
 * Maps POSIX functions to M4KK1 system calls
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "include/unistd.h"
#include "include/errno.h"

/* M4KK1 syscall interface */
static inline long m4k_syscall1(long num, long a1)
{
    long ret;
    __asm__ volatile("int $0x4D" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

static inline long m4k_syscall2(long num, long a1, long a2)
{
    long ret;
    __asm__ volatile("int $0x4D" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline long m4k_syscall3(long num, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x4D" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

/* Syscall numbers */
#define SYS_READ        0x4D000009
#define SYS_WRITE       0x4D00000A
#define SYS_OPEN        0x4D00000B
#define SYS_CLOSE       0x4D00000C
#define SYS_EXIT        0x4D000001
#define SYS_GETPID      0x4D000004
#define SYS_GETPPID     0x4D000006
#define SYS_GETUID      0x4D000020
#define SYS_GETGID      0x4D000022
#define SYS_GETEUID     0x4D000021
#define SYS_GETEGID     0x4D000023
#define SYS_SETUID      0x4D000024
#define SYS_SETGID      0x4D000025
#define SYS_UNLINK      0x4D000015
#define SYS_RMDIR       0x4D000013
#define SYS_CHDIR       0x4D000011
#define SYS_GETCWD      0x4D000010
#define SYS_PIPE        0x4D000028
#define SYS_DUP2        0x4D00002A
#define SYS_SLEEP       0x4D00005B

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)m4k_syscall3(SYS_READ, fd, (long)buf, count);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)m4k_syscall3(SYS_WRITE, fd, (long)buf, count);
}

int close(int fd)
{
    return (int)m4k_syscall1(SYS_CLOSE, fd);
}

int open(const char *pathname, int flags, ...)
{
    return (int)m4k_syscall2(SYS_OPEN, (long)pathname, flags);
}

off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ENOSYS;
    return -1;
}

pid_t fork(void)
{
    errno = ENOSYS;
    return -1;
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
    (void)filename;
    (void)argv;
    (void)envp;
    errno = ENOSYS;
    return -1;
}

int execv(const char *filename, char *const argv[])
{
    return execve(filename, argv, NULL);
}

int execvp(const char *file, char *const argv[])
{
    return execv(file, argv);
}

void _exit(int status)
{
    m4k_syscall1(SYS_EXIT, status);
    while (1);
}

pid_t getpid(void)
{
    return (pid_t)m4k_syscall1(SYS_GETPID, 0);
}

pid_t getppid(void)
{
    return (pid_t)m4k_syscall1(SYS_GETPPID, 0);
}

uid_t getuid(void)
{
    return (uid_t)m4k_syscall1(SYS_GETUID, 0);
}

uid_t geteuid(void)
{
    return (uid_t)m4k_syscall1(SYS_GETEUID, 0);
}

gid_t getgid(void)
{
    return (gid_t)m4k_syscall1(SYS_GETGID, 0);
}

gid_t getegid(void)
{
    return (gid_t)m4k_syscall1(SYS_GETEGID, 0);
}

int setuid(uid_t uid)
{
    return (int)m4k_syscall1(SYS_SETUID, uid);
}

int setgid(gid_t gid)
{
    return (int)m4k_syscall1(SYS_SETGID, gid);
}

char *getcwd(char *buf, size_t size)
{
    return (char *)m4k_syscall2(SYS_GETCWD, (long)buf, size);
}

int chdir(const char *path)
{
    return (int)m4k_syscall1(SYS_CHDIR, (long)path);
}

int unlink(const char *pathname)
{
    return (int)m4k_syscall1(SYS_UNLINK, (long)pathname);
}

int rmdir(const char *pathname)
{
    return (int)m4k_syscall1(SYS_RMDIR, (long)pathname);
}

int link(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    errno = ENOSYS;
    return -1;
}

int symlink(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    errno = ENOSYS;
    return -1;
}

int readlink(const char *path, char *buf, size_t bufsiz)
{
    (void)path;
    (void)buf;
    (void)bufsiz;
    errno = ENOSYS;
    return -1;
}

int pipe(int pipefd[2])
{
    return (int)m4k_syscall1(SYS_PIPE, (long)pipefd);
}

int dup(int oldfd)
{
    (void)oldfd;
    errno = ENOSYS;
    return -1;
}

int dup2(int oldfd, int newfd)
{
    return (int)m4k_syscall2(SYS_DUP2, oldfd, newfd);
}

unsigned int sleep(unsigned int seconds)
{
    /* Blocking SYS_SLEEP parks the caller off the ready queue until
     * the PIT tick scan wakes it (mkrn_process_sleep).  The kernel
     * clamps one sleep to 10 s, so chunk longer waits.  The old
     * nop-busy-wait burned 100% CPU for the whole interval. */
    while (seconds > 10) {
        m4k_syscall1(SYS_SLEEP, 10000);
        seconds -= 10;
    }
    if (seconds)
        m4k_syscall1(SYS_SLEEP, seconds * 1000);
    return 0;
}

int usleep(useconds_t usec)
{
    /* Same blocking syscall, millisecond resolution: round up so a
     * sub-tick request still yields one full tick to other tasks
     * instead of returning early via the busy path. */
    unsigned int ms = (unsigned int)(usec / 1000);
    if (usec % 1000)
        ms++;
    if (ms == 0)
        ms = 1;
    while (ms > 10000) {
        m4k_syscall1(SYS_SLEEP, 10000);
        ms -= 10000;
    }
    if (ms)
        m4k_syscall1(SYS_SLEEP, ms);
    return 0;
}
