/*
 * M4KK1 4P1 - process.h
 * Description: Process management types and API declarations
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <namespace.h>
#include <signal.h>

typedef int32_t pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;

#define M4K_UID_ROOT    0
#define M4K_UID_NOBODY  65534
#define M4K_GID_ROOT    0
#define M4K_GID_PRIME   1001

/* ── State Tag Bitmask (replaces old M4K_PROC_RUNNING/READY/BLOCKED/TERMINATED) ── */

/* Scheduling class (exactly one SHALL be set for a live process) */
#define M4K_SCHED_READY     0x00000001ULL
#define M4K_SCHED_RUNNING   0x00000002ULL
#define M4K_SCHED_SLEEPING  0x00000004ULL

/* Waiting reasons (zero or more, informational) */
#define M4K_WAIT_FS         0x00000100ULL
#define M4K_WAIT_PIPE       0x00000200ULL
#define M4K_WAIT_TIMER      0x00000400ULL
#define M4K_WAIT_IPC        0x00000800ULL
#define M4K_WAIT_CHILD      0x00001000ULL
#define M4K_WAIT_LOCK       0x00002000ULL
#define M4K_WAIT_TERMINAL   0x00004000ULL

/* Debug / control */
#define M4K_TRACED          0x00010000ULL
#define M4K_STOPPED         0x00020000ULL
#define M4K_TERMINATE       0x00040000ULL

/* Lifecycle */
#define M4K_ZOMBIE          0x01000000ULL
#define M4K_IDLE            0x02000000ULL

/* Convenience masks */
#define M4K_STATE_SCHED_MASK   0x00000007ULL
#define M4K_STATE_WAIT_MASK    0x00007F00ULL
#define M4K_STATE_CTRL_MASK    0x00070000ULL
#define M4K_STATE_LIFE_MASK    0x03000000ULL
#define M4K_STATE_RESERVED     (~0x0307FF07ULL)

/* ── Fork/Clone Flags ── */

#define RFPROC      0x0001
#define RFFDG       0x0002
#define RFNAMEG     0x0004
#define RFENVG      0x0008
#define RFMEM       0x0010
#define RFNOWAIT    0x0020

/* Spawn flags */
#define M4K_SPAWN_NEW_NS      0x0001
#define M4K_SPAWN_NEW_FD      0x0002
#define M4K_SPAWN_NEW_ENV     0x0004
#define M4K_SPAWN_NOWAIT      0x0008
#define M4K_SPAWN_TRACE       0x0010

/* ── Resource Limits (§6.10, §6.12.3) ── */

#define M4K_RLIMIT_CPU        0
#define M4K_RLIMIT_DATA       1
#define M4K_RLIMIT_STACK      2
#define M4K_RLIMIT_NPROC      3
#define M4K_RLIMIT_NOFILE     4
#define M4K_RLIMIT_MEMLOCK    5
#define M4K_RLIMIT_NLIMITS    6

#define M4K_RLIM_INFINITY    (~0ULL)

struct m4k_rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

/* ── Priorities ── */

#define M4K_PRIO_HIGH   0
#define M4K_PRIO_NORMAL 1
#define M4K_PRIO_LOW    2

/* ── Process structure (4P1 Process Model) ── */

typedef struct mkrn_process {
    uint32_t pid;
    uint32_t ppid;
    uint32_t uid;
    uint32_t euid;
    uint32_t gid;
    uint32_t egid;
    uint32_t group_count;
    uint32_t group_list[16];
    struct m4k_rlimit rlimits[M4K_RLIMIT_NLIMITS];
    uint32_t priority;
    uint32_t thread_esp;
    uint32_t kernel_stack;  /* top of this process's private kernel stack (TSS esp0) */
    uint32_t user_stack_base; /* bottom of this process's user stack allocation */
    /* Private copy of the int-0x80/int-0x4D user frame
     * {eip, cs, eflags, esp, ss, ebp}, written by the syscall ISRs on
     * every entry and read by fork to lay out the child's resume
     * frame.  Per-PCB (not a single global): fork copies 128KB of
     * stack while holding a pointer into the old global, and any
     * OTHER task entering a syscall via timer preemption overwrote
     * it mid-copy (the MDM waitpid storm) — fork then built a
     * poisoned iret frame (uesp=0xffffffff, eip=0xc8) and the child
     * #GP/#DF'd on resume. */
    uint32_t user_frame[6];
    uint32_t sleep_ticks;
    uint32_t exit_status;
    uint32_t pending_signals;
    uint32_t zombie_children;

    uint64_t state_tags;

    char name[32];
    char cwd[256];
    char **argv;
    char **envp;

    m4k_namespace_t ns;

    struct mkrn_process *next;
    struct mkrn_process *children;
    struct mkrn_process *sibling;
    struct mkrn_process *wait_next;
} mkrn_process_t;

typedef struct {
    mkrn_process_t *current;
    uint32_t process_count;
    uint32_t next_pid;
} mkrn_process_ctrl_t;

/* ── procinfo for getprocs syscall ── */

struct mkrn_procinfo {
    uint32_t pid;
    uint32_t ppid;
    uint32_t state;
    char name[32];
};

/* Backward compat alias for syscall.c */
#define procinfo mkrn_procinfo

/* ── Public API ── */

void mkrn_process_init(void);
int mkrn_execve(uint8_t *elf_data, uint32_t size, const char *proc_name);
void mkrn_sched_start(void);
void mkrn_process_yield(void);
void mkrn_process_switch_first(void);

void mkrn_process_exit(int status);
void mkrn_process_block(void);
void mkrn_process_wakeup(mkrn_process_t *process);

mkrn_process_t *mkrn_process_get_current(void);
void mkrn_process_dump_sched(void);
int mkrn_process_is_ready(pid_t pid);
uint32_t mkrn_process_get_tags(pid_t pid);
uint32_t mkrn_process_get_thread_esp(pid_t pid);
uint32_t mkrn_process_get_pid(void);
uint32_t mkrn_process_get_ppid(void);
uint32_t mkrn_process_get_uid(void);
uint32_t mkrn_process_get_euid(void);
uint32_t mkrn_process_get_gid(void);
uint32_t mkrn_process_get_egid(void);
int mkrn_process_set_uid(uint32_t uid);
int mkrn_process_set_gid(uint32_t gid);
int mkrn_process_get_groups(uint32_t *list, int size);
int mkrn_process_set_groups(int size, const uint32_t *list);
uint32_t mkrn_process_get_count(void);
int mkrn_process_set_rlimit(int resource, const struct m4k_rlimit *rlp);
int mkrn_process_get_rlimit(int resource, struct m4k_rlimit *rlp);

int mkrn_process_fill_info(struct mkrn_procinfo *buf, uint32_t max);

/* 4P1 new APIs */

pid_t mkrn_fork_status(uint64_t inherit_mask, uint32_t flags);
void mkrn_fork_child_restore(void);
extern uint32_t g_syscall_user_frame[6];
pid_t mkrn_spawn(const char *path, char *const argv[], char *const envp[], uint32_t flags);
pid_t mkrn_waitpid(pid_t pid, int *status, int options);
int mkrn_kill(pid_t pid, int sig);
void mkrn_process_terminate(pid_t pid);
int mkrn_setns(const char *path, const char *target, uint32_t flags);
/* Internal helpers */
mkrn_process_t *mkrn_process_find(pid_t pid);

/* One-pass pid snapshot of all live processes (see process.c).
 * Returns count copied; pids come out in registry (unsorted) order. */
uint32_t mkrn_process_get_pids(uint32_t *pids, uint32_t max);
int mkrn_process_enqueue_ready(mkrn_process_t *p);
void mkrn_process_reparent_children(mkrn_process_t *parent);
int mkrn_process_has_pending_signal(mkrn_process_t *p);
