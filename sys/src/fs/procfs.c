#include <vfs.h>
#include <process.h>
#include <signal.h>
#include <kernel.h>
#include <console.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PROCFS_MAX_CTL      512
#define PROCFS_MAGIC_FD     ((mkrn_file_ent_t *)0x500F)

/* Encode/decode procfs info in yafs_inode field:
 *   bits 31-24: file type (1=status,2=cmdline,3=environ,4=cwd,5=ctl)
 *   bits 23-0:  PID
 */
#define PROCFS_INODE(pid, type)  (((uint64_t)(type) << 32) | (uint64_t)(pid))
#define PROCFS_PID(inode)        ((uint32_t)(inode))
#define PROCFS_TYPE(inode)       ((uint32_t)((inode) >> 32))

static mkrn_process_t *procfs_find_pid(uint32_t pid)
{
    return mkrn_process_find((pid_t)pid);
}

static int procfs_format(char *buf, uint32_t size, const char *fmt, ...)
{
    int n = 0;
    va_list args;
    va_start(args, fmt);
    while (*fmt && n < (int)size - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
            case 'u': {
                uint32_t v = va_arg(args, uint32_t);
                char tmp[16];
                int ti = 0;
                if (v == 0) tmp[ti++] = '0';
                while (v > 0) {
                    tmp[ti++] = '0' + (v % 10);
                    v /= 10;
                }
                while (ti > 0 && n < (int)size - 1)
                    buf[n++] = tmp[--ti];
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                while (*s && n < (int)size - 1)
                    buf[n++] = *s++;
                break;
            }
            case 'c': {
                int c = va_arg(args, int);
                if (n < (int)size - 1)
                    buf[n++] = (char)c;
                break;
            }
            }
        } else {
            buf[n++] = *fmt;
        }
        fmt++;
    }
    va_end(args);
    buf[n] = '\0';
    return n;
}

static int procfs_gen_status(uint32_t pid, char *buf, uint32_t size)
{
    mkrn_process_t *p = procfs_find_pid(pid);
    if (!p)
        return procfs_format(buf, size, "PID: %u\nERROR: No such process\n", pid);

    int n = procfs_format(buf, size,
        "PID: %u\nPPID: %u\nName: %s\nState: [",
        p->pid, p->ppid, p->name);

    uint64_t s = p->state_tags;

#define ADD_TAG(tag, name) do { \
    if (s & (tag)) { \
        if (n > 0 && buf[n-1] != '[') { \
            if (n < (int)size - 1) buf[n++] = ','; \
        } \
        const char *_p = name; \
        while (*_p && n < (int)size - 1) buf[n++] = *_p++; \
    } \
} while(0)

    ADD_TAG(M4K_SCHED_READY, "SCHED_READY");
    ADD_TAG(M4K_SCHED_RUNNING, "SCHED_RUNNING");
    ADD_TAG(M4K_SCHED_SLEEPING, "SCHED_SLEEPING");
    ADD_TAG(M4K_WAIT_FS, "WAIT_FS");
    ADD_TAG(M4K_WAIT_PIPE, "WAIT_PIPE");
    ADD_TAG(M4K_WAIT_TIMER, "WAIT_TIMER");
    ADD_TAG(M4K_WAIT_IPC, "WAIT_IPC");
    ADD_TAG(M4K_WAIT_CHILD, "WAIT_CHILD");
    ADD_TAG(M4K_TRACED, "TRACED");
    ADD_TAG(M4K_STOPPED, "STOPPED");
    ADD_TAG(M4K_ZOMBIE, "ZOMBIE");
    ADD_TAG(M4K_IDLE, "IDLE");

#undef ADD_TAG

    if (n > 0 && buf[n-1] == '[') {
        const char *none = "NONE";
        while (*none && n < (int)size - 1) buf[n++] = *none++;
    }
    if (n < (int)size - 1) buf[n++] = ']';
    n += procfs_format(buf + n, size - n,
        "\nPriority: %u\nCWD: %s\n",
        p->priority, p->cwd);
    return n;
}

static int procfs_gen_cmdline(uint32_t pid, char *buf, uint32_t size)
{
    mkrn_process_t *p = procfs_find_pid(pid);
    if (!p)
        return procfs_format(buf, size, "ERROR\n");
    if (p->argv) {
        int n = 0;
        for (int i = 0; p->argv[i] && n < (int)size - 1; i++) {
            int len = (int)strlen(p->argv[i]);
            int rem = (int)size - n - 1;
            if (len > rem) len = rem;
            memcpy(buf + n, p->argv[i], len);
            n += len;
            if (n < (int)size - 1) buf[n++] = '\0';
        }
        if (n == 0 && size > 0) buf[n++] = '\0';
        return n;
    }
    return procfs_format(buf, size, "%s%c", p->name, '\0');
}

static int procfs_gen_environ(uint32_t pid, char *buf, uint32_t size)
{
    mkrn_process_t *p = procfs_find_pid(pid);
    if (!p)
        return procfs_format(buf, size, "ERROR\n");
    if (p->envp) {
        int n = 0;
        for (int i = 0; p->envp[i] && n < (int)size - 1; i++) {
            int len = (int)strlen(p->envp[i]);
            int rem = (int)size - n - 1;
            if (len > rem) len = rem;
            memcpy(buf + n, p->envp[i], len);
            n += len;
            if (n < (int)size - 1) buf[n++] = '\0';
        }
        if (n == 0 && size > 0) buf[n++] = '\0';
        return n;
    }
    if (size > 0) { buf[0] = '\0'; return 1; }
    return 0;
}

int mkrn_procfs_open(const char *path, int flags, int *out_fd)
{
    if (strncmp(path, "/sys/proc", 9) != 0)
        return -1;

    const char *p = path + 9;
    if (*p == '/') p++;

    /* List all PIDs (/sys/proc/) */
    if (*p == '\0') {
        for (int i = 0; i < M4K_VFS_MAX_FDS; i++) {
            extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
            if (!fd_table[i].in_use) {
                fd_table[i].fd = i;
                fd_table[i].file = PROCFS_MAGIC_FD;
                fd_table[i].offset = 0;
                fd_table[i].flags = flags | M4K_O_DIRECTORY;
                fd_table[i].in_use = true;
                fd_table[i].yafs_inode = 0; /* root dir */
                *out_fd = i;
                return 0;
            }
        }
        return -1;
    }

    /* Parse PID */
    uint32_t pid = 0;
    while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (*p - '0');
        p++;
    }
    if (pid == 0) return -1;

    mkrn_process_t *proc = procfs_find_pid(pid);
    if (!proc) return -1;

    /* /sys/proc/<pid>/ directory */
    if (*p == '\0' || *p == '/') {
        if (*p == '/') p++;
        (void)p;
        for (int i = 0; i < M4K_VFS_MAX_FDS; i++) {
            extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
            if (!fd_table[i].in_use) {
                fd_table[i].fd = i;
                fd_table[i].file = PROCFS_MAGIC_FD;
                fd_table[i].offset = 0;
                fd_table[i].flags = flags | M4K_O_DIRECTORY;
                fd_table[i].in_use = true;
                fd_table[i].yafs_inode = PROCFS_INODE(pid, 0);
                *out_fd = i;
                return 0;
            }
        }
        return -1;
    }

    if (*p == '/') p++;

    /* Determine file type */
    uint32_t ftype;
    if (strcmp(p, "status") == 0) ftype = 1;
    else if (strcmp(p, "cmdline") == 0) ftype = 2;
    else if (strcmp(p, "environ") == 0) ftype = 3;
    else if (strcmp(p, "cwd") == 0) ftype = 4;
    else if (strcmp(p, "ctl") == 0) ftype = 5;
    else if (strcmp(p, "ns") == 0 || strncmp(p, "ns/", 3) == 0) {
        ftype = 6; /* ns directory */
    } else return -1;

    for (int i = 0; i < M4K_VFS_MAX_FDS; i++) {
        extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
        if (!fd_table[i].in_use) {
            fd_table[i].fd = i;
            fd_table[i].file = PROCFS_MAGIC_FD;
            fd_table[i].offset = 0;
            fd_table[i].flags = (ftype == 5) ? M4K_O_WRONLY : M4K_O_RDONLY;
            fd_table[i].in_use = true;
            fd_table[i].yafs_inode = PROCFS_INODE(pid, ftype);
            *out_fd = i;
            return 0;
        }
    }
    return -1;
}

int mkrn_procfs_close(int fd)
{
    (void)fd;
    return 0;
}

int mkrn_procfs_read(int fd, void *buf, uint32_t count)
{
    extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !fd_table[fd].in_use)
        return -1;
    if (fd_table[fd].file != PROCFS_MAGIC_FD)
        return -1;

    uint32_t pid = PROCFS_PID(fd_table[fd].yafs_inode);
    uint32_t ftype = PROCFS_TYPE(fd_table[fd].yafs_inode);
    char *gen_buf = (char *)buf;
    uint32_t gen_sz = count;
    uint32_t offset = fd_table[fd].offset;

    int n = 0;
    switch (ftype) {
    case 0:
        return -1;
    case 1: n = procfs_gen_status(pid, gen_buf, gen_sz); break;
    case 2: n = procfs_gen_cmdline(pid, gen_buf, gen_sz); break;
    case 3: n = procfs_gen_environ(pid, gen_buf, gen_sz); break;
    case 4: {
        mkrn_process_t *p = procfs_find_pid(pid);
        if (!p) return -1;
        n = procfs_format(gen_buf, gen_sz, "%s", p->cwd[0] ? p->cwd : "/");
        break;
    }
    default: return -1;
    }

    if ((int)offset >= n) return 0;
    uint32_t avail = (uint32_t)(n - (int)offset);
    if (count > avail) count = avail;
    memmove(buf, (char *)buf + offset, count);
    fd_table[fd].offset = offset + count;
    return (int)count;
}

int mkrn_procfs_write(int fd, const void *buf, uint32_t count)
{
    extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !fd_table[fd].in_use)
        return -1;
    if (fd_table[fd].file != PROCFS_MAGIC_FD)
        return -1;

    uint32_t pid = PROCFS_PID(fd_table[fd].yafs_inode);
    uint32_t ftype = PROCFS_TYPE(fd_table[fd].yafs_inode);
    if (ftype != 5) return -1;

    char cmd[PROCFS_MAX_CTL];
    uint32_t len = count;
    if (len > sizeof(cmd) - 1) len = sizeof(cmd) - 1;
    memcpy(cmd, buf, len);
    cmd[len] = '\0';
    if (len > 0 && cmd[len - 1] == '\n')
        cmd[len - 1] = '\0';

    /* Parse and execute ctl command */
    mkrn_process_t *p = procfs_find_pid(pid);
    if (!p) return -1;

    const char *c = cmd;
    while (*c == ' ' || *c == '\t') c++;

    if (strncmp(c, "kill ", 5) == 0) {
        int sig = 0;
        const char *s = c + 5;
        while (*s >= '0' && *s <= '9')
            sig = sig * 10 + (*s++ - '0');
        if (sig > 0 && sig < M4K_NSIG)
            return (mkrn_kill((pid_t)pid, sig) == 0) ? (int)count : -1;
        return -1;
    }

    if (strcmp(c, "suspend") == 0) {
        p->state_tags |= M4K_STOPPED;
        p->state_tags &= ~M4K_SCHED_READY;
        return (int)count;
    }

    if (strcmp(c, "resume") == 0) {
        p->state_tags &= ~M4K_STOPPED;
        if (!(p->state_tags & M4K_STATE_SCHED_MASK))
            p->state_tags |= M4K_SCHED_READY;
        return (int)count;
    }

    if (strncmp(c, "niceness ", 9) == 0) {
        int val = 0;
        const char *s = c + 9;
        while (*s >= '0' && *s <= '9')
            val = val * 10 + (*s++ - '0');
        p->priority = (uint32_t)val;
        return (int)count;
    }

    if (strncmp(c, "chdir ", 6) == 0) {
        strncpy(p->cwd, c + 6, sizeof(p->cwd) - 1);
        p->cwd[sizeof(p->cwd) - 1] = '\0';
        return (int)count;
    }

    if (strncmp(c, "state +", 7) == 0) {
        const char *t = c + 7;
        if (strcmp(t, "WAIT_FS") == 0) p->state_tags |= M4K_WAIT_FS;
        else if (strcmp(t, "WAIT_PIPE") == 0) p->state_tags |= M4K_WAIT_PIPE;
        else if (strcmp(t, "WAIT_TIMER") == 0) p->state_tags |= M4K_WAIT_TIMER;
        else if (strcmp(t, "WAIT_IPC") == 0) p->state_tags |= M4K_WAIT_IPC;
        else if (strcmp(t, "WAIT_CHILD") == 0) p->state_tags |= M4K_WAIT_CHILD;
        else if (strcmp(t, "TRACED") == 0) p->state_tags |= M4K_TRACED;
        else return -1;
        return (int)count;
    }

    if (strncmp(c, "state -", 7) == 0) {
        const char *t = c + 7;
        if (strcmp(t, "WAIT_FS") == 0) p->state_tags &= ~M4K_WAIT_FS;
        else if (strcmp(t, "WAIT_PIPE") == 0) p->state_tags &= ~M4K_WAIT_PIPE;
        else if (strcmp(t, "WAIT_TIMER") == 0) p->state_tags &= ~M4K_WAIT_TIMER;
        else if (strcmp(t, "WAIT_IPC") == 0) p->state_tags &= ~M4K_WAIT_IPC;
        else if (strcmp(t, "WAIT_CHILD") == 0) p->state_tags &= ~M4K_WAIT_CHILD;
        else if (strcmp(t, "TRACED") == 0) p->state_tags &= ~M4K_TRACED;
        else if (strcmp(t, "STOPPED") == 0) p->state_tags &= ~M4K_STOPPED;
        else return -1;
        return (int)count;
    }

    if (strncmp(c, "state =", 7) == 0) {
        const char *t = c + 7;
        if (strcmp(t, "SCHED_READY") == 0)
            p->state_tags = M4K_SCHED_READY;
        else if (strcmp(t, "SCHED_SLEEPING") == 0)
            p->state_tags = M4K_SCHED_SLEEPING;
        else
            return -1;
        return (int)count;
    }

    if (strncmp(c, "setenv ", 7) == 0) return (int)count;
    if (strncmp(c, "unsetenv ", 9) == 0) return (int)count;

    return -1;
}

int mkrn_procfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS || !fd_table[fd].in_use)
        return -1;
    if (fd_table[fd].file != PROCFS_MAGIC_FD)
        return -1;

    uint32_t pid = PROCFS_PID(fd_table[fd].yafs_inode);
    uint32_t ftype = PROCFS_TYPE(fd_table[fd].yafs_inode);

    /* /sys/proc/ root listing */
    if (pid == 0) {
        uint32_t written = 0;
        /* Scan all processes */
        mkrn_process_t *cur = mkrn_process_get_current();
        if (cur && written < max) {
            memset(&buf[written], 0, sizeof(struct mkrn_vfs_dirent));
            buf[written].inode = cur->pid;
            buf[written].type = 2;
            procfs_format(buf[written].name, sizeof(buf[written].name), "%u", cur->pid);
            written++;
        }
        for (uint32_t i = 1; i < 256 && written < max; i++) {
            if (i == cur->pid) continue;
            mkrn_process_t *p = mkrn_process_find((pid_t)i);
            if (p) {
                memset(&buf[written], 0, sizeof(struct mkrn_vfs_dirent));
                buf[written].inode = p->pid;
                buf[written].type = 2;
                procfs_format(buf[written].name, sizeof(buf[written].name), "%u", p->pid);
                written++;
            }
        }
        return (int)written;
    }

    /* /sys/proc/<pid>/ns/ */
    if (ftype == 6) {
        mkrn_process_t *p = procfs_find_pid(pid);
        if (!p) return -1;
        uint32_t written = 0;
        for (uint32_t i = 0; i < p->ns.entry_count && written < max; i++) {
            memset(&buf[written], 0, sizeof(struct mkrn_vfs_dirent));
            buf[written].inode = 1000 + i;
            buf[written].type = 1;
            strncpy(buf[written].name,
                p->ns.entries[i].mnt_path[0] ? p->ns.entries[i].mnt_path : "mnt",
                sizeof(buf[written].name) - 1);
            written++;
        }
        return (int)written;
    }

    /* /sys/proc/<pid>/ */
    if (max >= 6) {
        const char *names[] = {"status", "cmdline", "environ", "cwd", "ns", "ctl"};
        for (int i = 0; i < 6; i++) {
            memset(&buf[i], 0, sizeof(struct mkrn_vfs_dirent));
            buf[i].inode = 100 + i;
            buf[i].type = (names[i][0] == 'n' && names[i][1] == 's') ? 2 : 1;
            strncpy(buf[i].name, names[i], sizeof(buf[i].name) - 1);
        }
        return 6;
    }

    return 0;
}

int mkrn_procfs_is_procfs_fd(int fd)
{
    extern mkrn_file_desc_t fd_table[M4K_VFS_MAX_FDS];
    if (fd < 0 || fd >= M4K_VFS_MAX_FDS) return 0;
    return (fd_table[fd].file == PROCFS_MAGIC_FD) ? 1 : 0;
}

void mkrn_procfs_init(void)
{
    M4K_LOG_INFO("ProcFS initialized at /sys/proc");
}
