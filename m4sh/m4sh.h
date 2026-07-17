/*
 * M4KK1 4P1 - m4sh.h
 * Description: Main header for M4KK1 shell
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Memory helpers (nostdlib - no libc available) */
static void *memcpy(void *d, const void *s, int n)
{
    char *cd = (char *)d;
    const char *cs = (const char *)s;
    for (int i = 0; i < n; i++)
        cd[i] = cs[i];
    return d;
}
static void *memset(void *d, int c, int n)
{
    char *cd = (char *)d;
    for (int i = 0; i < n; i++)
        cd[i] = c;
    return d;
}

/* String helpers */
static int musr_strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}
static void musr_strcpy(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}
static int musr_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
static int musr_strpref(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return 0;
    return 1;
}
static int musr_strchr(const char *s, int c)
{
    while (*s) {
        if (*s == c)
            return 1;
        s++;
    }
    return 0;
}

/* Syscall numbers */
#define S_OPEN      0x05
#define S_CLOSE     0x06
#define S_READ      0x03
#define S_WRITE     0x04
#define S_GETDENTS  0x47
#define S_GETCWD    0x10
#define S_CHDIR     0x11
#define S_MKDIR     0x12
#define S_RMDIR     0x13
#define S_UNLINK    0x15
#define S_RENAME    0x16
#define S_PIPE      0x28
#define S_DUP2      0x2A
#define S_GETPID    0x09
#define S_TIME      0x1E
#define S_BRK       0x0B
#define S_UNAME     0x73
#define S_SYSINFO   0x84
#define S_GETPROCS  0x85
#define S_STATFS    0x86
#define S_MOUNT     0x87
#define S_UMOUNT    0x88
#define S_MOUNTINFO 0x89
#define S_EXIT      0x01
#define S_FORK      0x02
#define S_WAITPID   0x07
#define S_GETPPID   0x0A
#define S_KILL      0x64
#define S_GETUID    0x20
#define S_GETEUID   0x24
#define S_SETUID    0x22
#define S_CHMOD     0x1B
#define S_CHOWN     0x1C
#define O_RDONLY    0x00000001
#define O_CREAT     0x00000100
#define O_WRONLY    0x00000002
#define O_TRUNC     0x00001000
#define DIRENT_NAME_MAX 256

struct dirent {
    uint64_t inode;
    uint32_t type;
    char name[DIRENT_NAME_MAX];
    uint64_t size;
};
struct sysinfo {
    uint32_t total_ram, free_ram, used_ram, process_count;
};
#define PROCBUF_MAX 64
struct procinfo {
    uint32_t pid, ppid, state;
    char name[32];
};
struct statfs {
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t used_blocks;
};

#define MOUNT_MAX 16
struct mount_entry {
    char source[128];
    char target[128];
    char fstype[32];
    int mounted;
};

#define UTSNAME_LEN 65
struct utsname {
    char sysname[UTSNAME_LEN];
    char nodename[UTSNAME_LEN];
    char release[UTSNAME_LEN];
    char version[UTSNAME_LEN];
    char machine[UTSNAME_LEN];
};

/* Syscall wrappers */
static inline uint32_t musr_sc0(uint32_t n)
{
    uint32_t r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n) : "memory");
    return r;
}
static inline uint32_t musr_sc1(uint32_t n, uint32_t a)
{
    uint32_t r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}
static inline uint32_t musr_sc2(uint32_t n, uint32_t a, uint32_t b)
{
    uint32_t r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b)
                     : "memory");
    return r;
}
static inline uint32_t musr_sc3(uint32_t n, uint32_t a, uint32_t b,
                                uint32_t c)
{
    uint32_t r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}

static int musr_sc_open(const char *p, int f)
{
    return (int)musr_sc2(S_OPEN, (uint32_t)p, (uint32_t)f);
}
static int musr_sc_close(int f)
{
    return (int)musr_sc1(S_CLOSE, (uint32_t)f);
}
static int musr_sc_read(int f, void *b, int c)
{
    return (int)musr_sc3(S_READ, (uint32_t)f, (uint32_t)b, (uint32_t)c);
}
static int musr_sc_write(int f, const void *b, int c)
{
    return (int)musr_sc3(S_WRITE, (uint32_t)f, (uint32_t)b, (uint32_t)c);
}
static int musr_sc_getdents(int f, struct dirent *b, int c)
{
    return (int)musr_sc3(S_GETDENTS, (uint32_t)f, (uint32_t)b, (uint32_t)c);
}
static int musr_sc_getcwd(char *b, int s)
{
    return (int)musr_sc2(S_GETCWD, (uint32_t)b, (uint32_t)s);
}
static int musr_sc_chdir(const char *p)
{
    return (int)musr_sc1(S_CHDIR, (uint32_t)p);
}
static int musr_sc_mkdir(const char *p)
{
    return (int)musr_sc1(S_MKDIR, (uint32_t)p);
}
static int musr_sc_unlink(const char *p)
{
    return (int)musr_sc1(S_UNLINK, (uint32_t)p);
}
static int musr_sc_rmdir(const char *p)
{
    return (int)musr_sc1(S_RMDIR, (uint32_t)p);
}
static int musr_sc_rename(const char *o, const char *n)
{
    return (int)musr_sc2(S_RENAME, (uint32_t)o, (uint32_t)n);
}
static int musr_sc_pipe(int *f)
{
    return (int)musr_sc1(S_PIPE, (uint32_t)f);
}
static int musr_sc_dup2(int o, int n)
{
    return (int)musr_sc2(S_DUP2, (uint32_t)o, (uint32_t)n);
}
static int musr_sc_uname(struct utsname *u)
{
    return (int)musr_sc1(S_UNAME, (uint32_t)u);
}
static int musr_sc_getpid(void)
{
    return (int)musr_sc0(S_GETPID);
}
static int musr_sc_getppid(void)
{
    return (int)musr_sc0(S_GETPPID);
}
static int musr_sc_fork(void)
{
    return (int)musr_sc0(S_FORK);
}
static int musr_sc_waitpid(int pid, int *status, int opts)
{
    return (int)musr_sc3(S_WAITPID, (uint32_t)pid,
                         (uint32_t)status, (uint32_t)opts);
}
static int musr_sc_kill(int pid, int sig)
{
    return (int)musr_sc2(S_KILL, (uint32_t)pid, (uint32_t)sig);
}
static uint32_t musr_sc_getuid(void)
{
    return musr_sc0(S_GETUID);
}
static uint32_t musr_sc_geteuid(void)
{
    return musr_sc0(S_GETEUID);
}
static int musr_sc_setuid(uint32_t uid)
{
    return (int)musr_sc1(S_SETUID, uid);
}
static int musr_sc_time(void)
{
    return (int)musr_sc0(S_TIME);
}
static int musr_sc_sysinfo(struct sysinfo *i)
{
    return (int)musr_sc1(S_SYSINFO, (uint32_t)i);
}
static int musr_sc_getprocs(struct procinfo *b, int m)
{
    return (int)musr_sc2(S_GETPROCS, (uint32_t)b, (uint32_t)m);
}
static int musr_sc_statfs(struct statfs *s)
{
    return (int)musr_sc1(S_STATFS, (uint32_t)s);
}
static int musr_sc_mount(const char *src, const char *tgt, const char *fst)
{
    return (int)musr_sc3(S_MOUNT, (uint32_t)src, (uint32_t)tgt,
                         (uint32_t)fst);
}
static int musr_sc_umount(const char *tgt)
{
    return (int)musr_sc1(S_UMOUNT, (uint32_t)tgt);
}
static int musr_sc_mountinfo(void *buf, int max)
{
    return (int)musr_sc2(S_MOUNTINFO, (uint32_t)buf, (uint32_t)max);
}

/* Serial I/O */
#define COM1_DATA 0x3F8
#define COM1_LSR 0x3FD
#define LSR_THR_EMPTY 0x20
#define LSR_DR 0x01

static inline void outb(uint16_t p, uint8_t v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline uint8_t inb(uint16_t p)
{
    uint8_t r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(p));
    return r;
}
static void ser_putc(char c)
{
    while (!(inb(COM1_DATA + 5) & LSR_THR_EMPTY))
        ;
    outb(COM1_DATA, c);
}
static void ser_puts(const char *s)
{
    while (*s)
        ser_putc(*s++);
}
static int ser_getc(void)
{
    if (inb(COM1_LSR) & LSR_DR)
        return inb(COM1_DATA);
    return -1;
}

/* Colours */
#define COL_RST "\x1B[0m"
#define COL_RED "\x1B[31m"
#define COL_GRN "\x1B[32m"
#define COL_YLW "\x1B[33m"
#define COL_BLU "\x1B[34m"
#define COL_MAG "\x1B[35m"
#define COL_CYN "\x1B[36m"
#define COL_WHT "\x1B[37m"

static void c_grn(void)
{
    ser_puts("\x1B[32m");
}
static void c_red(void)
{
    ser_puts("\x1B[31m");
}
static void c_ylw(void)
{
    ser_puts("\x1B[33m");
}
static void c_cyn(void)
{
    ser_puts("\x1B[36m");
}
static void c_mag(void)
{
    ser_puts("\x1B[35m");
}
static void c_wht(void)
{
    ser_puts("\x1B[37m");
}
static void c_rst(void)
{
    ser_puts("\x1B[0m");
}

/* Output abstraction */
extern int out_fd;

static void out_putc(char c)
{
    if (out_fd == 1)
        ser_putc(c);
    else
        musr_sc_write(out_fd, &c, 1);
}
static void out_puts(const char *s)
{
    if (out_fd == 1)
        ser_puts(s);
    else {
        while (*s) {
            musr_sc_write(out_fd, s, 1);
            s++;
        }
    }
}
static void out_printf(const char *s)
{
    out_puts(s);
}

static void print_u32(uint32_t v)
{
    char d[16];
    int p = 0;
    do {
        d[p++] = '0' + (char)(v % 10);
        v /= 10;
    } while (v > 0);
    while (p > 0)
        out_putc(d[--p]);
}
static void print_u64(uint64_t v)
{
    if (v > 0xFFFFFFFFUL) {
        out_puts("~~~~");
        return;
    }
    print_u32((uint32_t)v);
}
static void print_s32(int32_t v)
{
    if (v < 0) {
        out_putc('-');
        v = -v;
    }
    print_u32((uint32_t)v);
}

/* CWD management */
extern char cwd[256];

static void cwd_init(void)
{
    int n = musr_sc_getcwd(cwd, 256);
    if (n <= 0)
        musr_strcpy(cwd, "/");
}
static void cwd_to_abs(const char *in, char *out, int osize)
{
    if (!in || !*in) {
        musr_strcpy(out, cwd);
        return;
    }
    if (in[0] == '/') {
        musr_strcpy(out, in);
        return;
    }
    int i, j;
    for (i = 0; cwd[i] && i < osize - 1; i++)
        out[i] = cwd[i];
    int clen = i;
    if (clen > 0 && cwd[clen - 1] != '/')
        out[clen++] = '/';
    for (j = 0; in[j] && clen < osize - 1; j++, clen++)
        out[clen] = in[j];
    out[clen] = '\0';
}

/* Command table type */
typedef void (*musr_cmd_f)(int, char **);
typedef struct {
    const char *name;
    musr_cmd_f func;
    const char *desc;
} musr_cmd_t;

extern musr_cmd_t musr_cmd_table[];

/* Environment variables */
extern char envars[16][128];
extern int envar_cnt;

/* Helpers for command files */
static void print_pad_u32(uint32_t v, int w)
{
    int pad = w;
    uint32_t t = v;
    do {
        if (pad > 0)
            pad--;
        t /= 10;
    } while (t > 0);
    while (pad > 0) {
        out_putc(' ');
        pad--;
    }
    print_u32(v);
}

/* Forward declarations for all command functions */
void musr_cmd_help(int, char **);
void musr_cmd_cd(int, char **);
void musr_cmd_ls(int, char **);
void musr_cmd_cat(int, char **);
void musr_cmd_echo(int, char **);
void musr_cmd_exit(int, char **);
void musr_cmd_pwd(int, char **);
void musr_cmd_touch(int, char **);
void musr_cmd_mkdir(int, char **);
void musr_cmd_mv(int, char **);
void musr_cmd_rm(int, char **);
void musr_cmd_rmdir(int, char **);
void musr_cmd_cp(int, char **);
void musr_cmd_ps(int, char **);
void musr_cmd_kill(int, char **);
void musr_cmd_nice(int, char **);
void musr_cmd_time(int, char **);
void musr_cmd_grep(int, char **);
void musr_cmd_wc(int, char **);
void musr_cmd_export(int, char **);
void musr_cmd_last(int, char **);
void musr_cmd_date(int, char **);
void musr_cmd_clear(int, char **);
void musr_cmd_uname(int, char **);
void musr_cmd_free(int, char **);
void musr_cmd_df(int, char **);
void musr_cmd_mount(int, char **);
void musr_cmd_umount(int, char **);
void musr_cmd_at(int, char **);
void musr_cmd_batch(int, char **);
void musr_cmd_calc(int, char **);
void musr_cmd_blkid(int, char **);
void musr_cmd_cal(int, char **);
void musr_cmd_diff(int, char **);
void musr_cmd_sead(int, char **);
void musr_cmd_id(int, char **);
void musr_cmd_login(int, char **);
void musr_at_check_jobs(void);
