#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <vfs.h>
#include <process.h>
#include <kernel.h>
#include <m4k/state.h>
#include <m4k/types.h>

#define PROCFS_MAX_FILES 64
#define PROCFS_MAX_NAME  64

typedef struct {
    char name[PROCFS_MAX_NAME];
    uint32_t pid;
    bool is_ctl;
    bool in_use;
} procfs_file_t;

static procfs_file_t procfs_files[PROCFS_MAX_FILES];
static int procfs_next_fd = 1000;

static const char *format_state_tags(uint64_t tags)
{
    static char buf[128];
    int pos = 0;
    buf[0] = '[';

    struct { uint64_t bit; const char *name; } map[] = {
        {M4K_SCHED_READY, "SCHED_READY"},
        {M4K_SCHED_RUNNING, "SCHED_RUNNING"},
        {M4K_SCHED_SLEEPING, "SCHED_SLEEPING"},
        {M4K_WAIT_FS, "WAIT_FS"},
        {M4K_WAIT_PIPE, "WAIT_PIPE"},
        {M4K_WAIT_TIMER, "WAIT_TIMER"},
        {M4K_WAIT_IPC, "WAIT_IPC"},
        {M4K_WAIT_CHILD, "WAIT_CHILD"},
        {M4K_WAIT_LOCK, "WAIT_LOCK"},
        {M4K_WAIT_TERMINAL, "WAIT_TERMINAL"},
        {M4K_TRACED, "TRACED"},
        {M4K_STOPPED, "STOPPED"},
        {M4K_ZOMBIE, "ZOMBIE"},
        {M4K_IDLE, "IDLE"},
        {0, NULL}
    };

    int first = 1;
    for (int i = 0; map[i].name; i++) {
        if (tags & map[i].bit) {
            if (!first) {
                buf[pos++] = ',';
                if (pos >= 126) break;
                buf[pos++] = ' ';
            }
            for (const char *s = map[i].name; *s && pos < 126; s++)
                buf[pos++] = *s;
            first = 0;
        }
    }

    if (first) {
        buf[pos++] = 'N';
        buf[pos++] = 'O';
        buf[pos++] = 'N';
        buf[pos++] = 'E';
    }
    buf[pos++] = ']';
    buf[pos] = '\0';
    return buf;
}

void mkrn_procfs_init(void)
{
    mkrn_memset(procfs_files, 0, sizeof(procfs_files));
    M4K_LOG_INFO("ProcFS initialized");
}

static procfs_file_t *procfs_alloc_file(void)
{
    for (int i = 0; i < PROCFS_MAX_FILES; i++) {
        if (!procfs_files[i].in_use) {
            procfs_files[i].in_use = true;
            return &procfs_files[i];
        }
    }
    return NULL;
}

static void procfs_free_file(procfs_file_t *pf)
{
    pf->in_use = false;
}

static int procfs_getpid_from_path(const char *path)
{
    const char *p = path;
    while (*p == '/') p++;
    if (strncmp(p, "sys/proc/", 9) != 0)
        return -1;
    p += 9;

    int pid = 0;
    while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (*p - '0');
        p++;
    }
    return pid;
}

int mkrn_procfs_open(const char *path, int flags, int *out_fd)
{
    (void)flags;
    int pid = procfs_getpid_from_path(path);
    if (pid < 0) return -1;

    mkrn_process_t *proc = mkrn_process_find((pid_t)pid);
    if (!proc) return -1;

    const char *base = path;
    const char *slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/') slash = p;
    }
    const char *name = slash ? slash + 1 : path;

    procfs_file_t *pf = procfs_alloc_file();
    if (!pf) return -1;

    pf->pid = (uint32_t)pid;
    pf->is_ctl = (strcmp(name, "ctl") == 0);
    mkrn_strncpy(pf->name, path, PROCFS_MAX_NAME - 1);
    pf->name[PROCFS_MAX_NAME - 1] = '\0';

    *out_fd = procfs_next_fd++;
    return 0;
}

int mkrn_procfs_close(int fd)
{
    for (int i = 0; i < PROCFS_MAX_FILES; i++) {
        if (procfs_files[i].in_use) {
            procfs_free_file(&procfs_files[i]);
            return 0;
        }
    }
    return -1;
}

static procfs_file_t *procfs_find_by_fd(int fd)
{
    (void)fd;
    for (int i = 0; i < PROCFS_MAX_FILES; i++) {
        if (procfs_files[i].in_use)
            return &procfs_files[i];
    }
    return NULL;
}

int mkrn_procfs_read(int fd, void *buf, uint32_t count)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    if (!pf) return -1;

    mkrn_process_t *proc = mkrn_process_find((pid_t)pf->pid);
    if (!proc) return -1;

    char tmp[512];
    int len;

    if (pf->is_ctl) {
        return 0;
    }

    const char *state_str = format_state_tags(proc->state_tags);
    {
        int i;
        char nbuf[16];
        int ni;
        uint32_t n;
        char rev[16];
        int ri;

        /* manually format "PID: %u\n" etc */
#define ADD_LABEL(s) do { for (i = 0; (s)[i]; i++) { tmp[len++] = (s)[i]; } } while(0)
#define ADD_UINT(v) do { \
            n = (v); ni = 0; \
            if (n == 0) { nbuf[ni++] = '0'; } \
            else { ri = 0; while (n > 0) { rev[ri++] = '0' + (n % 10); n /= 10; } \
                  while (ri > 0) { nbuf[ni++] = rev[--ri]; } } \
            for (int _j = 0; _j < ni; _j++) { tmp[len++] = nbuf[_j]; } \
        } while(0)
#define ADD_STR(s) do { for (i = 0; (s)[i] && i < 60; i++) { tmp[len++] = (s)[i]; } } while(0)
#define ADD_NL do { tmp[len++] = '\n'; } while(0)

        ADD_LABEL("PID: "); ADD_UINT(proc->pid); ADD_NL;
        ADD_LABEL("PPID: "); ADD_UINT(proc->ppid); ADD_NL;
        ADD_LABEL("UID: "); ADD_UINT(proc->uid); ADD_NL;
        ADD_LABEL("GID: "); ADD_UINT(proc->gid); ADD_NL;
        ADD_LABEL("EUID: "); ADD_UINT(proc->euid); ADD_NL;
        ADD_LABEL("EGID: "); ADD_UINT(proc->egid); ADD_NL;
        ADD_LABEL("State: "); ADD_STR(state_str); ADD_NL;
        ADD_LABEL("CMD: "); ADD_STR(proc->name); ADD_NL;

#undef ADD_LABEL
#undef ADD_UINT
#undef ADD_STR
#undef ADD_NL
    }

    if (len > (int)count) len = (int)count;
    mkrn_memcpy(buf, tmp, (uint32_t)len);
    return len;
}

int mkrn_procfs_write(int fd, const void *buf, uint32_t count)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    if (!pf) return -1;

    if (!pf->is_ctl) return -1;

    mkrn_process_t *proc = mkrn_process_find((pid_t)pf->pid);
    if (!proc) return -1;

    char line[128];
    uint32_t len = count < 127 ? count : 127;
    mkrn_memcpy(line, buf, len);
    line[len] = '\0';

    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
        line[--len] = '\0';

    if (strncmp(line, "suspend", 7) == 0) {
        proc->state_tags |= M4K_STOPPED;
        proc->state_tags &= ~M4K_SCHED_READY;
        return (int)count;
    }

    if (strcmp(line, "resume") == 0) {
        proc->state_tags &= ~M4K_STOPPED;
        proc->state_tags |= M4K_SCHED_READY;
        return (int)count;
    }

    if (strncmp(line, "kill ", 5) == 0) {
        int sig = 0;
        const char *sp = line + 5;
        while (*sp >= '0' && *sp <= '9') {
            sig = sig * 10 + (*sp - '0');
            sp++;
        }
        if (sig > 0) {
            mkrn_kill((pid_t)pf->pid, sig);
        }
        return (int)count;
    }

    if (strncmp(line, "state +", 7) == 0) {
        const char *tag = line + 7;
        struct { const char *name; uint64_t bit; } tag_map[] = {
            {"WAIT_FS", M4K_WAIT_FS},
            {"WAIT_PIPE", M4K_WAIT_PIPE},
            {"WAIT_TIMER", M4K_WAIT_TIMER},
            {"WAIT_IPC", M4K_WAIT_IPC},
            {"WAIT_CHILD", M4K_WAIT_CHILD},
            {"WAIT_LOCK", M4K_WAIT_LOCK},
            {"WAIT_TERMINAL", M4K_WAIT_TERMINAL},
            {"TRACED", M4K_TRACED},
            {"STOPPED", M4K_STOPPED},
            {NULL, 0}
        };
        for (int i = 0; tag_map[i].name; i++) {
            if (strcmp(tag, tag_map[i].name) == 0) {
                proc->state_tags |= tag_map[i].bit;
                break;
            }
        }
        return (int)count;
    }

    return -1;
}

int mkrn_procfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    (void)fd;
    (void)buf;
    (void)max;
    return 0;
}

int mkrn_procfs_is_procfs_fd(int fd)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    return pf ? 1 : 0;
}
