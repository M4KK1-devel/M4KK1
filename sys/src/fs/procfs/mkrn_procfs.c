#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <kstrtox.h>
#include <console.h>
#include <vfs.h>
#include <process.h>
#include <kernel.h>
#include <m4k/state.h>
#include <m4k/types.h>
#include <namespace.h>

#define PROCFS_MAX_FILES 64
#define PROCFS_FD_BASE   1000
#define PROCFS_FD_LIMIT  2000   /* sessions range starts here */
#define PROCFS_MAX_NAME  64

#define FT_STATUS   0
#define FT_CMDLINE  1
#define FT_ENVIRON  2
#define FT_CWD      3
#define FT_NS       4
#define FT_CTL      5

typedef struct {
    int fd;
    char name[PROCFS_MAX_NAME];
    uint32_t pid;
    bool in_use;
    int file_type;
    int ns_index;
    int read_offset;
} procfs_file_t;

static procfs_file_t procfs_files[PROCFS_MAX_FILES];
static int procfs_next_fd = PROCFS_FD_BASE;

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
            /* Recycle fd numbers: a monotonically increasing counter
             * runs into the sessions fd range after 1000 open/close
             * cycles, corrupting VFS routing.  Wrap within our range,
             * skipping numbers still held by open files. */
            int fd = procfs_next_fd;
            for (int guard = 0; guard < PROCFS_FD_LIMIT - PROCFS_FD_BASE; guard++) {
                int taken = 0;
                for (int j = 0; j < PROCFS_MAX_FILES; j++) {
                    if (procfs_files[j].in_use && procfs_files[j].fd == fd) {
                        taken = 1;
                        break;
                    }
                }
                if (!taken)
                    break;
                if (++fd >= PROCFS_FD_LIMIT)
                    fd = PROCFS_FD_BASE;
            }
            procfs_files[i].fd = fd;
            if (++procfs_next_fd >= PROCFS_FD_LIMIT)
                procfs_next_fd = PROCFS_FD_BASE;
            procfs_files[i].in_use = true;
            procfs_files[i].read_offset = 0;
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

    unsigned int pid_val = 0;
    if (mkrn_kstrtouint(p, 10, &pid_val) < 0)
        pid_val = 0;
    int pid = (int)pid_val;
    return pid;
}

int mkrn_procfs_open(const char *path, int flags, int *out_fd)
{
    (void)flags;

    /* Handle directory listing of /sys/proc/ itself */
    const char *p = path;
    while (*p == '/') p++;
    if (strcmp(p, "sys/proc") == 0 || strcmp(p, "sys/proc/") == 0) {
        procfs_file_t *pf = procfs_alloc_file();
        if (!pf) return -1;
        pf->pid = 0;
        pf->file_type = -1;
        mkrn_strncpy(pf->name, path, PROCFS_MAX_NAME - 1);
        pf->name[PROCFS_MAX_NAME - 1] = '\0';
        *out_fd = pf->fd;
        return 0;
    }

    int pid = procfs_getpid_from_path(path);
    if (pid < 0) return -1;

    mkrn_process_t *proc = mkrn_process_find((pid_t)pid);
    if (!proc) return -1;

    /* Find the file component after the PID */
    const char *name = path;
    const char *last_slash = NULL;
    for (const char *cp = path; *cp; cp++) {
        if (*cp == '/') last_slash = cp;
    }
    /* Also check for second-to-last component for ns/ */
    const char *second_last = NULL;
    const char *prev = NULL;
    for (const char *cp = path; *cp; cp++) {
        if (*cp == '/') { second_last = prev; prev = cp; }
    }
    name = last_slash ? last_slash + 1 : path;

    procfs_file_t *pf = procfs_alloc_file();
    if (!pf) return -1;

    pf->pid = (uint32_t)pid;
    pf->ns_index = -1;

    /* Check for ns/ sub-files */
    if (second_last && strncmp(second_last + 1, "ns", 2) == 0 &&
        (second_last[3] == '/' || second_last[3] == '\0')) {
        pf->file_type = FT_NS;
        /* Parse ns index from filename */
        if (name && *name >= '0' && *name <= '9') {
            unsigned int idx_val = 0;
            if (mkrn_kstrtouint(name, 10, &idx_val) < 0)
                idx_val = 0;
            pf->ns_index = (int)idx_val;
        }
    } else if (strcmp(name, "ctl") == 0) {
        pf->file_type = FT_CTL;
    } else if (strcmp(name, "cmdline") == 0) {
        pf->file_type = FT_CMDLINE;
    } else if (strcmp(name, "environ") == 0) {
        pf->file_type = FT_ENVIRON;
    } else if (strcmp(name, "cwd") == 0) {
        pf->file_type = FT_CWD;
    } else if (strcmp(name, "status") == 0) {
        pf->file_type = FT_STATUS;
    } else {
        pf->file_type = FT_STATUS;
    }

    mkrn_strncpy(pf->name, path, PROCFS_MAX_NAME - 1);
    pf->name[PROCFS_MAX_NAME - 1] = '\0';
    *out_fd = pf->fd;
    return 0;
}

int mkrn_procfs_close(int fd)
{
    for (int i = 0; i < PROCFS_MAX_FILES; i++) {
        if (procfs_files[i].in_use && procfs_files[i].fd == fd) {
            procfs_free_file(&procfs_files[i]);
            return 0;
        }
    }
    return -1;
}

static procfs_file_t *procfs_find_by_fd(int fd)
{
    for (int i = 0; i < PROCFS_MAX_FILES; i++) {
        if (procfs_files[i].in_use && procfs_files[i].fd == fd)
            return &procfs_files[i];
    }
    return NULL;
}

int mkrn_procfs_read(int fd, void *buf, uint32_t count)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    if (!pf) return -1;

    if (pf->read_offset > 0)
        return 0;

    /* Directory listing of /sys/proc/ */
    if (pf->file_type == -1) {
        char tmp[512];
        int len = 0;
        /* One-pass pid snapshot — the old loop probed find() for every
         * candidate pid 0..1023 (O(1024*n) registry walks per read). */
        uint32_t pids[128];
        uint32_t np = mkrn_process_get_pids(pids, 128);
        for (uint32_t i = 1; i < np; i++) {   /* insertion sort: ascending */
            uint32_t key = pids[i];
            int j = (int)i - 1;
            while (j >= 0 && pids[j] > key) { pids[j + 1] = pids[j]; j--; }
            pids[j + 1] = key;
        }
        for (uint32_t i = 0; np > 0 && i < np && len < (int)sizeof(tmp) - 12
             && len < (int)count - 12; i++) {
            char nbuf[16];
            int ni = 0;
            uint32_t n = pids[i];
            if (n == 0) { nbuf[ni++] = '0'; }
            else { char rev[16]; int ri = 0; while (n > 0) { rev[ri++] = '0' + (n % 10); n /= 10; } while (ri > 0) nbuf[ni++] = rev[--ri]; }
            for (int j = 0; j < ni && len < (int)sizeof(tmp) - 1; j++) tmp[len++] = nbuf[j];
            tmp[len++] = '\n';
        }
        if (len == 0) { tmp[len++] = '\n'; }
        if (len > (int)count) len = (int)count;
        mkrn_memcpy(buf, tmp, (uint32_t)len);
        pf->read_offset = len;
        return len;
    }

    mkrn_process_t *proc = mkrn_process_find((pid_t)pf->pid);
    if (!proc) return -1;

    char tmp[512];
    int len = 0;

    if (pf->file_type == FT_CTL) {
        return 0;
    }

    if (pf->file_type == FT_CWD) {
        /* Symlink: return the absolute cwd path */
        int i;
        for (i = 0; proc->cwd[i] && i < 508; i++)
            tmp[i] = proc->cwd[i];
        tmp[i++] = '\n';
        len = i;
        if (len > (int)count) len = (int)count;
        mkrn_memcpy(buf, tmp, (uint32_t)len);
        pf->read_offset = len;
        return len;
    }

    if (pf->file_type == FT_NS) {
        if (pf->ns_index >= 0 && pf->ns_index < (int)proc->ns.entry_count) {
            m4k_ns_entry_t *e = &proc->ns.entries[pf->ns_index];
            /* Format: mnt_path  ->  target_path  (FLAGS) */
            int i;
            for (i = 0; e->mnt_path[i] && i < 100; i++)
                tmp[i] = e->mnt_path[i];
            tmp[i++] = ' '; tmp[i++] = '-'; tmp[i++] = '>'; tmp[i++] = ' ';
            int j;
            for (j = 0; e->target_path[j] && i < 500; j++, i++)
                tmp[i] = e->target_path[j];
            tmp[i++] = '\n';
            len = i;
        } else {
            /* List all ns entries */
            for (uint32_t ni = 0; ni < proc->ns.entry_count && len < (int)count - 50; ni++) {
                m4k_ns_entry_t *e = &proc->ns.entries[ni];
                int j;
                for (j = 0; e->mnt_path[j] && len < 500; j++)
                    tmp[len++] = e->mnt_path[j];
                tmp[len++] = ' '; tmp[len++] = '-'; tmp[len++] = '>'; tmp[len++] = ' ';
                for (j = 0; e->target_path[j] && len < 500; j++)
                    tmp[len++] = e->target_path[j];
                tmp[len++] = '\n';
            }
            if (len == 0) { tmp[len++] = '\n'; }
        }
        if (len > (int)count) len = (int)count;
        mkrn_memcpy(buf, tmp, (uint32_t)len);
        pf->read_offset = len;
        return len;
    }

    if (pf->file_type == FT_CMDLINE) {
        if (proc->argv) {
            for (int i = 0; proc->argv[i] && len < 508; i++) {
                int pos = 0;
                while (proc->argv[i][pos] && len < 508)
                    tmp[len++] = proc->argv[i][pos++];
                tmp[len++] = '\0';
            }
        }
        if (len > (int)count) len = (int)count;
        mkrn_memcpy(buf, tmp, (uint32_t)len);
        pf->read_offset = len;
        return len;
    }

    if (pf->file_type == FT_ENVIRON) {
        if (proc->envp) {
            for (int i = 0; proc->envp[i] && len < 508; i++) {
                int pos = 0;
                while (proc->envp[i][pos] && len < 508)
                    tmp[len++] = proc->envp[i][pos++];
                tmp[len++] = '\0';
            }
        }
        if (len > (int)count) len = (int)count;
        mkrn_memcpy(buf, tmp, (uint32_t)len);
        pf->read_offset = len;
        return len;
    }

    {
        const char *state_str = format_state_tags(proc->state_tags);
        int i;
        char nbuf[16];
        int ni;
        uint32_t n;
        char rev[16];
        int ri;

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
    pf->read_offset = len;
    return len;
}

int mkrn_procfs_write(int fd, const void *buf, uint32_t count)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    if (!pf) return -1;

    if (pf->file_type != FT_CTL) return -1;

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
        unsigned int sig_val = 0;
        if (mkrn_kstrtouint(line + 5, 10, &sig_val) < 0)
            sig_val = 0;
        int sig = (int)sig_val;
        if (sig > 0)
            mkrn_kill((pid_t)pf->pid, sig);
        return (int)count;
    }

    if (strncmp(line, "state +", 7) == 0) {
        const char *tag = line + 7;
        struct { const char *name; uint64_t bit; } tag_map[] = {
            {"SCHED_READY", M4K_SCHED_READY},
            {"SCHED_SLEEPING", M4K_SCHED_SLEEPING},
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

    if (strncmp(line, "state -", 7) == 0) {
        const char *tag = line + 7;
        struct { const char *name; uint64_t bit; } tag_map[] = {
            {"SCHED_READY", M4K_SCHED_READY},
            {"SCHED_SLEEPING", M4K_SCHED_SLEEPING},
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
                proc->state_tags &= ~tag_map[i].bit;
                break;
            }
        }
        return (int)count;
    }

    if (strncmp(line, "state =", 7) == 0) {
        const char *tag = line + 7;
        proc->state_tags = 0;
        struct { const char *name; uint64_t bit; } tag_map[] = {
            {"SCHED_READY", M4K_SCHED_READY},
            {"SCHED_SLEEPING", M4K_SCHED_SLEEPING},
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
                proc->state_tags = tag_map[i].bit;
                break;
            }
        }
        return (int)count;
    }

    if (strncmp(line, "niceness ", 9) == 0) {
        int nice = 0;
        int sign = 1;
        const char *np = line + 9;
        if (*np == '-') { sign = -1; np++; }
        unsigned int nice_val = 0;
        if (mkrn_kstrtouint(np, 10, &nice_val) < 0)
            nice_val = 0;
        nice = sign * (int)nice_val;
        /* FIXME: implement actual niceness scheduling */
        (void)sign;
        (void)proc;
        return (int)count;
    }

    if (strncmp(line, "chdir ", 6) == 0) {
        const char *dir = line + 6;
        mkrn_strncpy(proc->cwd, dir, 255);
        proc->cwd[255] = '\0';
        return (int)count;
    }

    if (strncmp(line, "chroot ", 7) == 0) {
        /* FIXME: implement per-process root */
        M4K_LOG_INFO("chroot not yet implemented\n");
        return (int)count;
    }

    if (strncmp(line, "setenv ", 7) == 0) {
        const char *ev = line + 7;
        /* Find or create env entry */
        const char *eq = ev;
        while (*eq && *eq != '=') eq++;
        if (*eq == '=' && proc->envp) {
            int klen = (int)(eq - ev);
            int found = -1, empty = -1;
            for (int i = 0; proc->envp[i] && i < 63; i++) {
                if (proc->envp[i][0] == '\0' && empty < 0) empty = i;
                if (strncmp(proc->envp[i], ev, klen) == 0 && proc->envp[i][klen] == '=') {
                    found = i;
                    break;
                }
            }
            int idx = found >= 0 ? found : (empty >= 0 ? empty : -1);
            if (idx >= 0) {
                int elen = 0;
                while (ev[elen] && elen < 127) elen++;
                char *env_copy = (char *)mkrn_alloc((size_t)(elen + 1));
                if (env_copy) {
                    mkrn_memcpy(env_copy, ev, elen);
                    env_copy[elen] = '\0';
                    proc->envp[idx] = env_copy;
                }
            }
        }
        return (int)count;
    }

    if (strncmp(line, "unsetenv ", 9) == 0) {
        const char *key = line + 9;
        int klen = 0;
        while (key[klen]) klen++;
        if (proc->envp) {
            for (int i = 0; proc->envp[i] && i < 63; i++) {
                if (strncmp(proc->envp[i], key, klen) == 0 && proc->envp[i][klen] == '=') {
                    proc->envp[i][0] = '\0';
                    break;
                }
            }
        }
        return (int)count;
    }

    return -1;
}

int mkrn_procfs_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    if (!pf || !buf || max == 0) return 0;

    uint32_t written = 0;

    if (pf->file_type == -1) {
        /* Listing /sys/proc/ — return PID directories.
         * One-pass pid snapshot (old loop probed find() per candidate
         * pid 0..1023 = O(1024*n) registry walks per getdents). */
        uint32_t pids[128];
        uint32_t np = mkrn_process_get_pids(pids, 128);
        for (uint32_t i = 1; i < np; i++) {   /* insertion sort: ascending */
            uint32_t key = pids[i];
            int j = (int)i - 1;
            while (j >= 0 && pids[j] > key) { pids[j + 1] = pids[j]; j--; }
            pids[j + 1] = key;
        }
        for (uint32_t i = 0; i < np && written < max; i++) {
            char nbuf[16];
            int ni = 0;
            uint32_t n = pids[i];
            if (n == 0) { nbuf[ni++] = '0'; }
            else { char rev[16]; int ri = 0; while (n > 0) { rev[ri++] = '0' + (n % 10); n /= 10; } while (ri > 0) nbuf[ni++] = rev[--ri]; }
            nbuf[ni] = '\0';
            mkrn_memcpy(buf[written].name, nbuf, (uint32_t)(ni + 1));
            buf[written].type = 1;
            written++;
        }
        return (int)written;
    }

    mkrn_process_t *proc = mkrn_process_find((pid_t)pf->pid);
    if (!proc) return 0;

    /* Listing a PID directory — return status, cmdline, etc. */
    struct { const char *n; int t; } entries[] = {
        {"status", 0},
        {"cmdline", 0},
        {"environ", 0},
        {"cwd", 0},
        {"ctl", 0},
        {"ns", 1},
        {NULL, 0}
    };

    for (int i = 0; entries[i].n && written < max; i++) {
        mkrn_memcpy(buf[written].name, entries[i].n, (uint32_t)(mkrn_strlen(entries[i].n) + 1));
        buf[written].type = entries[i].t;
        written++;
    }

    return (int)written;
}

int mkrn_procfs_is_procfs_fd(int fd)
{
    procfs_file_t *pf = procfs_find_by_fd(fd);
    return pf ? 1 : 0;
}
