/*
 * M4KK1 4P1 - syscall.h
 * Description: Kernel syscall handler definitions and syscall numbers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_SC_EXIT        0x01
#define M4K_SC_FORK        0x02
#define M4K_SC_READ        0x03
#define M4K_SC_WRITE       0x04
#define M4K_SC_OPEN        0x05
#define M4K_SC_CLOSE       0x06
#define M4K_SC_WAITPID     0x07
#define M4K_SC_EXECVE      0x08
#define M4K_SC_GETPID      0x09
#define M4K_SC_GETPPID     0x0A
#define M4K_SC_BRK         0x0B
#define M4K_SC_MMAP        0x0C
#define M4K_SC_MUNMAP      0x0D
#define M4K_SC_MPROTECT    0x0E
#define M4K_SC_MSYNC       0x0F
#define M4K_SC_GETCWD      0x10
#define M4K_SC_CHDIR       0x11
#define M4K_SC_MKDIR       0x12
#define M4K_SC_RMDIR       0x13
#define M4K_SC_LINK        0x14
#define M4K_SC_UNLINK      0x15
#define M4K_SC_RENAME      0x16
#define M4K_SC_STAT        0x17
#define M4K_SC_FSTAT       0x18
#define M4K_SC_LSTAT       0x19
#define M4K_SC_ACCESS      0x1A
#define M4K_SC_CHMOD       0x1B
#define M4K_SC_CHOWN       0x1C
#define M4K_SC_UTIME       0x1D
#define M4K_SC_TIME        0x1E
#define M4K_SC_TIMES       0x1F
#define M4K_SC_GETUID      0x20
#define M4K_SC_GETGID      0x21
#define M4K_SC_SETUID      0x22
#define M4K_SC_SETGID      0x23
#define M4K_SC_GETEUID     0x24
#define M4K_SC_GETEGID     0x25
#define M4K_SC_SETEUID     0x26
#define M4K_SC_SETEGID     0x27
#define M4K_SC_PIPE        0x28
#define M4K_SC_DUP         0x29
#define M4K_SC_DUP2        0x2A
#define M4K_SC_SELECT      0x2B
#define M4K_SC_POLL        0x2C
#define M4K_SC_EPOLL_CREATE 0x2D
#define M4K_SC_EPOLL_CTL   0x2E
#define M4K_SC_EPOLL_WAIT  0x2F
#define M4K_SC_SOCKET      0x30
#define M4K_SC_BIND        0x31
#define M4K_SC_LISTEN      0x32
#define M4K_SC_ACCEPT      0x33
#define M4K_SC_CONNECT     0x34
#define M4K_SC_SEND        0x35
#define M4K_SC_RECV        0x36
#define M4K_SC_SENDTO      0x37
#define M4K_SC_RECVFROM    0x38
#define M4K_SC_SHUTDOWN    0x39
#define M4K_SC_SETSOCKOPT  0x3A
#define M4K_SC_GETSOCKOPT  0x3B
#define M4K_SC_IOCTL       0x3C
#define M4K_SC_FCNTL       0x3D
#define M4K_SC_READDIR     0x3E
#define M4K_SC_TELLDIR     0x3F
#define M4K_SC_SEEKDIR     0x40
#define M4K_SC_CLOSEDIR    0x41
#define M4K_SC_OPENDIR     0x42
#define M4K_SC_MKNOD       0x43
#define M4K_SC_MKFIFO      0x44
#define M4K_SC_TRUNCATE    0x45
#define M4K_SC_FTRUNCATE   0x46
#define M4K_SC_GETDENTS    0x47
#define M4K_SC_SYNC        0x48
#define M4K_SC_FSYNC       0x49
#define M4K_SC_FDATASYNC   0x4A
#define M4K_SC_MLOCK       0x4B
#define M4K_SC_MUNLOCK     0x4C
#define M4K_SC_MLOCKALL    0x4D
#define M4K_SC_MUNLOCKALL  0x4E
#define M4K_SC_NANOSLEEP   0x4F
#define M4K_SC_CLOCK_GETTIME 0x50
#define M4K_SC_CLOCK_SETTIME 0x51
#define M4K_SC_CLOCK_GETRES  0x52
#define M4K_SC_SCHED_YIELD   0x53
#define M4K_SC_SCHED_SETSCHEDULER 0x54
#define M4K_SC_SCHED_GETSCHEDULER 0x55
#define M4K_SC_SCHED_SETPARAM    0x56
#define M4K_SC_SCHED_GETPARAM    0x57
#define M4K_SC_SCHED_SETAFFINITY 0x58
#define M4K_SC_SCHED_GETAFFINITY 0x59
#define M4K_SC_PRLIMIT64         0x5A
#define M4K_SC_GETRUSAGE         0x5B
#define M4K_SC_GETTIMEOFDAY      0x5C
#define M4K_SC_SETTIMEOFDAY      0x5D
#define M4K_SC_ADJTIMEX          0x5E
#define M4K_SC_TIMER_CREATE      0x5F
#define M4K_SC_TIMER_DELETE      0x60
#define M4K_SC_TIMER_SETTIME     0x61
#define M4K_SC_TIMER_GETTIME     0x62
#define M4K_SC_TIMER_GETOVERRUN  0x63
#define M4K_SC_KILL              0x64
#define M4K_SC_TKILL             0x65
#define M4K_SC_TGKILL            0x66
#define M4K_SC_SIGACTION         0x67
#define M4K_SC_SIGPROCMASK       0x68
#define M4K_SC_SIGPENDING        0x69
#define M4K_SC_SIGSUSPEND        0x6A
#define M4K_SC_SIGTIMEDWAIT      0x6B
#define M4K_SC_SIGRETURN         0x6C
#define M4K_SC_REBOOT            0x6D
#define M4K_SC_KEXEC_LOAD        0x6E
#define M4K_SC_EXIT_GROUP        0x6F
#define M4K_SC_WAIT4             0x70
#define M4K_SC_CLONE             0x71
#define M4K_SC_VFORK             0x72
#define M4K_SC_UNAME             0x73
#define M4K_SC_SEMGET            0x74
#define M4K_SC_SEMOP             0x75
#define M4K_SC_SEMCTL            0x76
#define M4K_SC_SEMTIMEDOP        0x77
#define M4K_SC_MSGGET            0x78
#define M4K_SC_MSGSND            0x79
#define M4K_SC_MSGRCV            0x7A
#define M4K_SC_MSGCTL            0x7B
#define M4K_SC_SHMGET            0x7C
#define M4K_SC_SHMAT             0x7D
#define M4K_SC_SHMDT             0x7E
#define M4K_SC_SHMCTL            0x7F

/* Dynamic linker syscalls */
#define M4K_SC_DL_LOAD_LIBRARY   0x80
#define M4K_SC_DL_UNLOAD_LIBRARY 0x81
#define M4K_SC_DL_FIND_SYMBOL    0x82
#define M4K_SC_DL_GET_ERROR      0x83

/* M4KK1 extended syscalls */
#define M4K_SC_SYSINFO          0x84
#define M4K_SC_GETPROCS         0x85
#define M4K_SC_STATFS           0x86
#define M4K_SC_MOUNT            0x87
#define M4K_SC_UMOUNT           0x88
#define M4K_SC_MOUNTINFO        0x89

#define M4K_SC_SUCCESS 0
#define M4K_SC_ERROR   (-1)

struct mkrn_sysinfo {
    u32 total_ram;
    u32 free_ram;
    u32 used_ram;
    u32 process_count;
};

struct mkrn_statfs {
    u32 block_size;
    u32 total_blocks;
    u32 free_blocks;
    u32 used_blocks;
};

#define M4K_PROCBUF_MAX 64

/* struct mkrn_procinfo is now defined in process.h */
#include <process.h>

/**
 * mkrn_syscall_init - Initialize the syscall subsystem
 *
 * Return: void
 */
void mkrn_syscall_init(void);

/**
 * mkrn_syscall_handler - Handle a system call
 *
 * Return: void
 */
void mkrn_syscall_handler(void);

/**
 * mkrn_syscall_register - Register a syscall handler
 * @num: Syscall number
 * @handler: Handler function pointer
 *
 * Return: void
 */
void mkrn_syscall_register(u32 num, void *handler);

/**
 * mkrn_syscall_execute - Execute a system call
 * @num: Syscall number
 * @arg1: First argument
 * @arg2: Second argument
 * @arg3: Third argument
 * @arg4: Fourth argument
 * @arg5: Fifth argument
 *
 * Return: Result of the syscall
 */
u32 mkrn_syscall_execute(u32 num, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5);

#define M4K_SCALL0(num) mkrn_syscall_execute(num, 0, 0, 0, 0, 0)
#define M4K_SCALL1(num, arg1) mkrn_syscall_execute(num, (u32)arg1, 0, 0, 0, 0)
#define M4K_SCALL2(num, arg1, arg2) mkrn_syscall_execute(num, (u32)arg1, (u32)arg2, 0, 0, 0)
#define M4K_SCALL3(num, arg1, arg2, arg3) mkrn_syscall_execute(num, (u32)arg1, (u32)arg2, (u32)arg3, 0, 0)
#define M4K_SCALL4(num, arg1, arg2, arg3, arg4) mkrn_syscall_execute(num, (u32)arg1, (u32)arg2, (u32)arg3, (u32)arg4, 0)
#define M4K_SCALL5(num, arg1, arg2, arg3, arg4, arg5) mkrn_syscall_execute(num, (u32)arg1, (u32)arg2, (u32)arg3, (u32)arg4, (u32)arg5)
