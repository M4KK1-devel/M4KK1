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
static void musr_strncpy(char *d, const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n && s[i]; i++)
        d[i] = s[i];
    if (i < n)
        d[i] = '\0';
    else if (n > 0)
        d[n - 1] = '\0';
}
static int musr_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
static void musr_strcat(char *d, const char *s)
{
    d += musr_strlen(d);
    while ((*d++ = *s++))
        ;
}
static void musr_strncat(char *d, const char *s, size_t n)
{
    size_t dlen = musr_strlen(d);
    size_t i;
    for (i = 0; i < n && s[i]; i++)
        d[dlen + i] = s[i];
    d[dlen + i] = '\0';
}
static int musr_strncmp(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
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
#define S_SETTIME   0x5D
#define S_UPTIME    0x8C
#define S_RTCREAD   0x8D
#define S_RTCWRITE  0x8E
#define S_TIMERLIST 0x8F
#define S_NETINFO   0x90
#define S_PING      0x91
#define S_TCPCONN   0x92
#define S_TCPSEND   0x93
#define S_TCPRECV   0x94
#define S_TCPCLOSE  0x95
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
#define S_GETGID    0x21
#define S_SETUID    0x22
#define S_SETGID    0x23
#define S_GETEUID   0x24
#define S_GETEGID   0x25
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
static uint32_t musr_sc_getgid(void)
{
    return musr_sc0(S_GETGID);
}
static uint32_t musr_sc_getegid(void)
{
    return musr_sc0(S_GETEGID);
}
static int musr_sc_setgid(uint32_t gid)
{
    return (int)musr_sc1(S_SETGID, gid);
}
#define S_GETGROUPS 0x8A
static int musr_sc_getgroups(int size, uint32_t *list)
{
    return (int)musr_sc2(S_GETGROUPS, (uint32_t)size, (uint32_t)list);
}
static int musr_sc_time(void)
{
    return (int)musr_sc0(S_TIME);
}
static int musr_sc_settime(uint32_t epoch)
{
    return (int)musr_sc1(S_SETTIME, epoch);
}
static uint32_t musr_sc_uptime(void)
{
    return musr_sc0(S_UPTIME);
}
static int musr_sc_rtcread(uint32_t *buf)
{
    return (int)musr_sc1(S_RTCREAD, (uint32_t)buf);
}
static int musr_sc_rtcwrite(uint32_t *buf)
{
    return (int)musr_sc1(S_RTCWRITE, (uint32_t)buf);
}
static uint32_t musr_sc_timerlist(void)
{
    return musr_sc0(S_TIMERLIST);
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

/* ── musr_* library aliases (§5.1) ── */

static int musr_atoi(const char *s)
{
    int v = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v * sign;
}
#define musr_printf out_puts
#define musr_getchar ser_getc
#define musr_putchar ser_putc
#define musr_memcpy memcpy

/* ── m4k_* ABI wrappers (§4.7, §6.12) — native int 0x4D ABI ── */

static inline uint32_t m4k_sc0(uint32_t n)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n) : "memory");
    return r;
}
static inline uint32_t m4k_sc1(uint32_t n, uint32_t a)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n), "b"(a) : "memory");
    return r;
}
static inline uint32_t m4k_sc2(uint32_t n, uint32_t a, uint32_t b)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n), "b"(a), "c"(b) : "memory");
    return r;
}
static inline uint32_t m4k_sc3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
static inline uint32_t m4k_sc4(uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return r;
}

static inline uint32_t m4k_sc5(uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t r;
    __asm__ volatile("int $0x4D" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return r;
}

/* ── struct m4k_rlimit (needed by m4k_setrlimit/m4k_getrlimit) ── */

struct m4k_rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

#define M4K_RLIMIT_CPU        0
#define M4K_RLIMIT_DATA       1
#define M4K_RLIMIT_STACK      2
#define M4K_RLIMIT_NPROC      3
#define M4K_RLIMIT_NOFILE     4
#define M4K_RLIMIT_MEMLOCK    5
#define M4K_RLIMIT_NLIMITS    6

#define M4K_RLIM_INFINITY    (~0ULL)

/* ── M4KK1 native syscall numbers (§4.7.2, §6.12) ── */

#define M4K_SYS_EXIT        0x4D000001
#define M4K_SYS_SPAWN       0x4D000002
#define M4K_SYS_WAIT        0x4D000003
#define M4K_SYS_GETPID      0x4D000004
#define M4K_SYS_KILL        0x4D000005
#define M4K_SYS_GETPPID     0x4D000006
#define M4K_SYS_FORK_ST     0x4D000007
#define M4K_SYS_SETNS       0x4D000008
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
#define M4K_SYS_BRK         0x4D000040
#define M4K_SYS_REGISTER_SESSION   0x4D000041
#define M4K_SYS_GET_SESSION_LIST   0x4D000042
#define M4K_SYS_YIELD      0x4D000043
#define M4K_SYS_MMAP        0x4D00000E
#define M4K_SYS_MUNMAP      0x4D00000F
#define M4K_SYS_MEMINFO     0x4D000010

static inline int m4k_getpid(void)    { return (int)m4k_sc0(M4K_SYS_GETPID); }
static inline int m4k_yield(void)     { return (int)m4k_sc0(M4K_SYS_YIELD); }
static inline int m4k_getppid(void)   { return (int)m4k_sc0(M4K_SYS_GETPPID); }
static inline int m4k_exit(uint32_t status) { return (int)m4k_sc1(M4K_SYS_EXIT, status); }
static inline int m4k_getuid(void)    { return (int)m4k_sc0(M4K_SYS_GETUID); }
static inline int m4k_geteuid(void)   { return (int)m4k_sc0(M4K_SYS_GETEUID); }
static inline int m4k_getgid(void)    { return (int)m4k_sc0(M4K_SYS_GETGID); }
static inline int m4k_getegid(void)   { return (int)m4k_sc0(M4K_SYS_GETEGID); }
static inline int m4k_setuid(uint32_t u) { return (int)m4k_sc1(M4K_SYS_SETUID, u); }
static inline int m4k_setgid(uint32_t g) { return (int)m4k_sc1(M4K_SYS_SETGID, g); }
static inline int m4k_getgroups(int s, uint32_t *l) { return (int)m4k_sc2(M4K_SYS_GETGROUPS, (uint32_t)s, (uint32_t)l); }
static inline int m4k_setgroups(int s, uint32_t *l) { return (int)m4k_sc2(M4K_SYS_SETGROUPS, (uint32_t)s, (uint32_t)l); }
static inline int m4k_chmod(const char *p, int m) { return (int)m4k_sc2(M4K_SYS_CHMOD, (uint32_t)p, (uint32_t)m); }
static inline int m4k_chown(const char *p, uint32_t u, uint32_t g) { return (int)m4k_sc3(M4K_SYS_CHOWN, (uint32_t)p, u, g); }
static inline int m4k_access(const char *p, int m) { return (int)m4k_sc2(M4K_SYS_ACCESS, (uint32_t)p, (uint32_t)m); }
static inline int m4k_kill(int pid, int s) { return (int)m4k_sc2(M4K_SYS_KILL, (uint32_t)pid, (uint32_t)s); }
static inline int m4k_waitpid(int p, int *s, int o) { return (int)m4k_sc3(M4K_SYS_WAIT, (uint32_t)p, (uint32_t)s, (uint32_t)o); }
static inline int m4k_spawn(const char *path, uint32_t flags) { return (int)m4k_sc1(M4K_SYS_SPAWN, (uint32_t)path); }
static inline int m4k_setns(const char *p, const char *t, uint32_t f) { return (int)m4k_sc3(M4K_SYS_SETNS, (uint32_t)p, (uint32_t)t, f); }
static inline int m4k_get_memory_usage(uint32_t *t, uint32_t *u, uint32_t *f) { return (int)m4k_sc3(M4K_SYS_MEMINFO, (uint32_t)t, (uint32_t)u, (uint32_t)f); }
static inline int m4k_setrlimit(int r, const struct m4k_rlimit *l) { return (int)m4k_sc2(M4K_SYS_SETRLIMIT, (uint32_t)r, (uint32_t)l); }
static inline int m4k_getrlimit(int r, struct m4k_rlimit *l) { return (int)m4k_sc2(M4K_SYS_GETRLIMIT, (uint32_t)r, (uint32_t)l); }

/* ── Framebuffer / video ── */
#define M4K_SYS_GET_FRAMEBUFFER_INFO 0x4D000050
#define M4K_SYS_DRAW_TEST_PATTERN    0x4D000051
#define M4K_SYS_GET_MOUSE_EVENT      0x4D000052
#define M4K_SYS_FLIP                 0x4D000053
#define M4K_SYS_DRAW_RECT            0x4D000054
#define M4K_SYS_DRAW_TEXT            0x4D000055
#define M4K_SYS_GET_KEYBOARD_EVENT   0x4D000056
#define M4K_SYS_GFX_BLIT             0x4D000057
#define M4K_SYS_FLIP_RECT            0x4D000058
#define M4K_SYS_UPDATE_CURSOR        0x4D000059
#define M4K_SYS_BEEP                  0x4D00005A
#define M4K_SYS_SLEEP                 0x4D00005B
#define M4K_SYS_GET_MOUSE_POS         0x4D00005C
#define M4K_SYS_FILL_GRADIENT         0x4D00005D
struct m4k_framebuffer_info {
    uint32_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
};
struct m4k_mouse_event {
    int16_t dx;
    int16_t dy;
    uint8_t buttons;
    uint8_t reserved;
};

struct m4k_keyboard_event {
    uint8_t ascii_char;
    uint8_t keycode;
    uint8_t modifiers;
    uint8_t reserved;
};

/* Keyboard modifier bits returned in m4k_keyboard_event.modifiers
 * (mirrors the kernel's KEYBOARD_MOD_* low byte). */
#define M4K_MOD_SHIFT  0x01
#define M4K_MOD_CTRL   0x02
#define M4K_MOD_ALT    0x04

static inline int m4k_get_framebuffer_info(struct m4k_framebuffer_info *fb) {
    return (int)m4k_sc1(M4K_SYS_GET_FRAMEBUFFER_INFO, (uint32_t)fb);
}
static inline int m4k_draw_test_pattern(void) {
    return (int)m4k_sc0(M4K_SYS_DRAW_TEST_PATTERN);
}
static inline int m4k_get_mouse_event(struct m4k_mouse_event *ev) {
    return (int)m4k_sc1(M4K_SYS_GET_MOUSE_EVENT, (uint32_t)ev);
}
static inline int m4k_flip(void) {
    return (int)m4k_sc0(M4K_SYS_FLIP);
}
static inline int m4k_flip_rect(int x, int y, int w, int h) {
    return (int)m4k_sc4(M4K_SYS_FLIP_RECT, (uint32_t)x, (uint32_t)y,
                        (uint32_t)w, (uint32_t)h);
}
static inline int m4k_update_cursor(void) {
    return (int)m4k_sc0(M4K_SYS_UPDATE_CURSOR);
}
static inline int m4k_beep(uint32_t hz, uint32_t ms) {
    return (int)m4k_sc2(M4K_SYS_BEEP, hz, ms);
}
static inline int m4k_sleep(uint32_t ms) {
    return (int)m4k_sc1(M4K_SYS_SLEEP, ms);
}
static inline int m4k_draw_rect(int x, int y, int w, int h, uint32_t color) {
    return (int)m4k_sc5(M4K_SYS_DRAW_RECT, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, color);
}
static inline int m4k_draw_text(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    return (int)m4k_sc5(M4K_SYS_DRAW_TEXT, (uint32_t)x, (uint32_t)y, (uint32_t)str, fg, bg);
}
static inline int m4k_get_keyboard_event(struct m4k_keyboard_event *ev) {
    return (int)m4k_sc1(M4K_SYS_GET_KEYBOARD_EVENT, (uint32_t)ev);
}
/* Absolute cursor position as the kernel mouse driver sees it
 * (2x ballistics + screen clamp) — identical to the visible cursor. */
static inline int m4k_get_mouse_pos(int32_t *x, int32_t *y) {
    int32_t pos[2];
    int r = (int)m4k_sc1(M4K_SYS_GET_MOUSE_POS, (uint32_t)pos);
    if (x) *x = pos[0];
    if (y) *y = pos[1];
    return r;
}
static inline int m4k_gfx_blit(int x, int y, int w, int h, const void *src) {
    return (int)m4k_sc5(M4K_SYS_GFX_BLIT, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, (uint32_t)src);
}
/* Kernel-side vertical gradient fill: one syscall replaces the
 * per-scanline m4k_draw_rect loop (which paid a syscall + scheduler
 * yield per row).  Colors span the full screen height (top at y=0,
 * bottom at height-1) regardless of the clip rect, matching the old
 * gui_draw_gradient / copland_wallpaper_rect math. */
static inline int m4k_fill_gradient(int x, int y, int w, int h,
                                    uint32_t top, uint32_t bottom) {
    uint32_t params[2];
    params[0] = top;
    params[1] = bottom;
    return (int)m4k_sc5(M4K_SYS_FILL_GRADIENT, (uint32_t)x, (uint32_t)y,
                        (uint32_t)w, (uint32_t)h, (uint32_t)params);
}

#define M4K_SESSION_MAX 16
struct m4k_session_info {
    char tty[32];
    uint32_t uid;
    uint32_t pid;
    uint32_t login_time;
    char username[64];
    int active;
};

static inline int m4k_register_session(const char *tty, uint32_t pid, const char *username) {
    return (int)m4k_sc3(M4K_SYS_REGISTER_SESSION, (uint32_t)tty, pid, (uint32_t)username);
}
static inline int m4k_get_session_list(struct m4k_session_info *buf, int max) {
    return (int)m4k_sc2(M4K_SYS_GET_SESSION_LIST, (uint32_t)buf, (uint32_t)max);
}

static inline int m4k_chdir(const char *p) {
    return musr_sc_chdir(p);
}

/* Serial I/O */
#define COM1_DATA 0x3F8
#define COM1_LSR 0x3FD
#define LSR_THR_EMPTY 0x20
#define LSR_DR 0x01

static inline void outb(uint16_t p, uint8_t v)
{
#ifdef __PCC__
    __asm__ volatile("outb %b0, %w1" : : "a"(v), "d"(p));
#else
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
#endif
}
static inline uint8_t inb(uint16_t p)
{
    uint8_t r;
#ifdef __PCC__
    __asm__ volatile("inb %w1, %b0" : "=a"(r) : "d"(p));
#else
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(p));
#endif
    return r;
}
static void ser_putc(char c)
{
#ifdef M4SH_GRAPHICAL
    /* Graphical terminal (m4shg): stdout is a pipe owned by the
     * terminal emulator process — never touch the COM1 hardware. */
    musr_sc_write(1, &c, 1);
#else
    while (!(inb(COM1_DATA + 5) & LSR_THR_EMPTY))
        ;
    outb(COM1_DATA, c);
#endif
}
static void ser_puts(const char *s)
{
    while (*s)
        ser_putc(*s++);
}
static int ser_getc(void)
{
#ifdef M4SH_GRAPHICAL
    /* Graphical terminal (m4shg): stdin is a pipe fed by the terminal
     * emulator forwarding keystrokes.  Pipe reads are non-blocking
     * (return 0 when empty) — same contract as the serial variant,
     * yield so the WM keeps compositing while we poll. */
    char c;
    int n = musr_sc_read(0, &c, 1);
    if (n == 1)
        return (unsigned char)c;
    m4k_yield();
    return -1;
#else
    if (inb(COM1_LSR) & LSR_DR)
        return inb(COM1_DATA);
    m4k_yield();
    return -1;
#endif
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
        musr_strncpy(cwd, "/", sizeof(cwd)-1);
}
static void cwd_to_abs(const char *in, char *out, int osize)
{
    if (!in || !*in) {
        musr_strncpy(out, cwd, osize-1);
        return;
    }
    if (in[0] == '/') {
        musr_strncpy(out, in, osize-1);
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

/* ── Password & identity library (pwd.c) ── */

typedef struct {
    char username[64];
    uint32_t uid;
    uint32_t gid;
    char home[128];
    char shell[64];
    char gecos[128];
    char password_hash[256];
} passwd_entry_t;

typedef struct {
    char class_name[32];
    uint64_t cputime;
    uint64_t datasize;
    uint64_t stacksize;
    uint32_t maxproc;
    uint32_t openfiles;
} login_class_t;

void musr_hash_password(const char *password, const uint8_t *salt, char *hash_out);
int musr_verify_password(const char *password, const char *stored_hash);
void musr_make_password_hash(const char *password, char *hash_out);
int musr_read_passwd_db(passwd_entry_t *entries, int max);
int musr_getpwnam(const char *name, passwd_entry_t *out);
int musr_getpwuid(uint32_t uid, passwd_entry_t *out);
int musr_update_passwd_db(const passwd_entry_t *entries, int count);
int musr_parse_login_conf(const char *username, login_class_t *out);

/* ── Group database (grp.c, §6.6) ── */

typedef struct {
    char groupname[64];
    uint32_t gid;
    char members[256];
} group_entry_t;

int musr_read_groups_db(group_entry_t *entries, int max);
int musr_getgrnam(const char *name, group_entry_t *out);
int musr_getgrgid(uint32_t gid, group_entry_t *out);
int musr_in_group(const char *username, const char *groupname);
int musr_update_groups_db(const group_entry_t *entries, int count);

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
void musr_cmd_dd(int, char **);
void musr_cmd_beep(int, char **);
void musr_cmd_cal(int, char **);
void musr_cmd_diff(int, char **);
void musr_cmd_sead(int, char **);
void musr_cmd_id(int, char **);
void musr_cmd_login(int, char **);
void musr_cmd_quell(int, char **);
void musr_cmd_passwd(int, char **);
void musr_cmd_who(int, char **);
void musr_cmd_usermod(int, char **);
void musr_cmd_groupmod(int, char **);
void musr_cmd_cu(int, char **);
void musr_cmd_userlog(int, char **);
void musr_cmd_gfx_test(int, char **);
void musr_cmd_pcc(int, char **);
void musr_cmd_cc(int, char **);
void musr_cmd_man(int, char **);
void musr_cmd_ping(int, char **);
void musr_cmd_wget(int, char **);
void musr_cmd_ifconfig(int, char **);
void musr_boot_setup(void);
void musr_setup_env(void);
extern int musr_login_ok;
void musr_at_check_jobs(void);

/* ── flow.c: eval / shift / trap ── */
extern int last_exit_code;
void musr_exec_line(char *line);
extern char musr_pos_args[9][128];
void musr_set_args(int ac, char **av);
int  musr_pos_param(int n, char *out, int osize);
void musr_cmd_eval(int ac, char **av);
void musr_cmd_shift(int ac, char **av);
void musr_cmd_trap(int ac, char **av);
void musr_trap_fire(const char *name);
void musr_trap_on_exit(void);
