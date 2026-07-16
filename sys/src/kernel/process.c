#include "process.h"
#include "timer.h"
#include "memory.h"
#include "kernel.h"
#include "console.h"
#include "syscall.h"
#include "signal.h"
#include <string.h>
#include <stdint.h>

#ifndef asm
#define asm __asm__
#endif

static mkrn_process_ctrl_t process_ctrl;
static mkrn_process_t *current = NULL;
static uint32_t scheduler_enabled = 0;

#define READY_QUEUE_SIZE 64
static mkrn_process_t *ready_queue[READY_QUEUE_SIZE];
static uint32_t ready_queue_count = 0;

static void ready_enqueue(mkrn_process_t *p)
{
    if (ready_queue_count >= READY_QUEUE_SIZE)
        return;
    ready_queue[ready_queue_count++] = p;
}

static mkrn_process_t *ready_dequeue(void)
{
    if (ready_queue_count == 0)
        return NULL;
    mkrn_process_t *p = ready_queue[0];
    ready_queue_count--;
    for (uint32_t i = 0; i < ready_queue_count; i++)
        ready_queue[i] = ready_queue[i + 1];
    return p;
}

static void ready_remove(mkrn_process_t *p)
{
    for (uint32_t i = 0; i < ready_queue_count; i++) {
        if (ready_queue[i] == p) {
            ready_queue_count--;
            for (uint32_t j = i; j < ready_queue_count; j++)
                ready_queue[j] = ready_queue[j + 1];
            return;
        }
    }
}

static uint32_t next_pid(void)
{
    return process_ctrl.next_pid++;
}

/* ── Process lookup ── */

mkrn_process_t *mkrn_process_find(pid_t pid)
{
    /* Search ready queue */
    for (uint32_t i = 0; i < ready_queue_count; i++) {
        if (ready_queue[i]->pid == (uint32_t)pid)
            return ready_queue[i];
    }

    /* Also check current */
    if (current && current->pid == (uint32_t)pid)
        return current;

    return NULL;
}

/* ── Public API ── */

mkrn_process_t *mkrn_process_get_current(void)
{
    return current;
}

uint32_t mkrn_process_get_pid(void)
{
    return current ? current->pid : 0;
}

uint32_t mkrn_process_get_ppid(void)
{
    return current ? current->ppid : 0;
}

uint32_t mkrn_process_get_count(void)
{
    return process_ctrl.process_count;
}

int mkrn_process_enqueue_ready(mkrn_process_t *p)
{
    if (!p)
        return -1;
    ready_enqueue(p);
    return 0;
}

void mkrn_sched_start(void)
{
    scheduler_enabled = 1;
    M4K_LOG_INFO("Scheduler started");
}

void mkrn_process_yield(void)
{
    if (!scheduler_enabled || !current)
        return;

    uint32_t saved_esp;

    asm volatile(
        "pushl %%ebp\n"
        "pushl %%edi\n"
        "pushl %%esi\n"
        "pushl %%ebx\n"
        "movl %%esp, %0\n"
        : "=r" (saved_esp)
        :
        : "memory"
    );

    current->thread_esp = saved_esp;

    /* Re-enqueue if still running (yield) or sleeping (yield after block) */
    if (current->state_tags & M4K_SCHED_RUNNING) {
        current->state_tags &= ~M4K_SCHED_RUNNING;
        current->state_tags |= M4K_SCHED_READY;
        ready_enqueue(current);
    }

    /* Dequeue next ready process (skip stopped) */
    mkrn_process_t *next = NULL;
    while (1) {
        next = ready_dequeue();
        if (!next)
            break;
        if (!(next->state_tags & M4K_STOPPED) &&
            (next->state_tags & M4K_SCHED_READY)) {
            break;
        }
        /* Put stopped/non-ready back at end */
        ready_enqueue(next);
    }

    if (!next) {
        asm volatile(
            "movl %0, %%esp\n"
            "popl %%ebx\n"
            "popl %%esi\n"
            "popl %%edi\n"
            "popl %%ebp\n"
            "ret\n"
            :
            : "r" (saved_esp)
            : "memory"
        );
        return;
    }

    current = next;
    next->state_tags &= ~(M4K_SCHED_READY | M4K_SCHED_SLEEPING);
    next->state_tags |= M4K_SCHED_RUNNING;

    asm volatile(
        "movl %0, %%esp\n"
        "popl %%ebx\n"
        "popl %%esi\n"
        "popl %%edi\n"
        "popl %%ebp\n"
        "ret\n"
        :
        : "r" (next->thread_esp)
        : "memory"
    );
}

void mkrn_process_block(void)
{
    if (!current)
        return;
    current->state_tags &= ~M4K_SCHED_RUNNING;
    current->state_tags |= M4K_SCHED_SLEEPING;
    mkrn_process_yield();
}

void mkrn_process_wakeup(mkrn_process_t *p)
{
    if (!p)
        return;
    if (p->state_tags & M4K_SCHED_SLEEPING) {
        p->state_tags &= ~M4K_SCHED_SLEEPING;
        p->state_tags |= M4K_SCHED_READY;
        ready_enqueue(p);
    }
}

void mkrn_process_switch_first(void)
{
    if (!current) {
        mkrn_panic("No process to switch to");
    }

    mkrn_process_t *p = current;
    p->state_tags = M4K_SCHED_RUNNING;
    ready_enqueue(p);
    scheduler_enabled = 1;

    asm volatile(
        "movl %0, %%esp\n"
        "popl %%ebx\n"
        "popl %%esi\n"
        "popl %%edi\n"
        "popl %%ebp\n"
        "ret\n"
        :
        : "r" (p->thread_esp)
        : "memory"
    );

    __builtin_unreachable();
}

void mkrn_process_exit(int status)
{
    if (!current)
        return;

    M4K_LOG_INFO("Process exiting: PID=");
    mkrn_console_write_dec(current->pid);
    mkrn_console_write("\n");

    current->exit_status = status;
    current->state_tags &= ~(M4K_SCHED_RUNNING | M4K_SCHED_READY | M4K_SCHED_SLEEPING);
    current->state_tags &= ~M4K_STOPPED;

    /* Send SIGCHLD to parent */
    mkrn_process_t *parent = mkrn_process_find((pid_t)current->ppid);
    if (parent) {
        mkrn_kill((pid_t)current->ppid, M4K_SIGCHLD);
    }

    process_ctrl.process_count--;

    /* Reparent children to PID 1 */
    mkrn_process_reparent_children(current);

    /* Reap if NOWAIT */
    mkrn_process_t *next = ready_dequeue();
    if (next) {
        current = next;
        current->state_tags &= ~(M4K_SCHED_READY | M4K_SCHED_SLEEPING);
        current->state_tags |= M4K_SCHED_RUNNING;

        asm volatile(
            "movl %0, %%esp\n"
            "popl %%ebx\n"
            "popl %%esi\n"
            "popl %%edi\n"
            "popl %%ebp\n"
            "ret\n"
            :
            : "r" (next->thread_esp)
            : "memory"
        );
    }

    while (1) {
        asm volatile("hlt");
    }
}

void mkrn_process_reparent_children(mkrn_process_t *parent)
{
    if (!parent) return;

    for (uint32_t i = 0; i < ready_queue_count; i++) {
        if (ready_queue[i]->ppid == parent->pid) {
            ready_queue[i]->ppid = 1;
        }
    }
}

/* ── Process initialization ── */

mkrn_process_t *mkrn_execve_create_idle(void);

void mkrn_process_init(void)
{
    M4K_LOG_INFO("Initializing process management...");

    mkrn_memset(&process_ctrl, 0, sizeof(process_ctrl));
    process_ctrl.next_pid = 1;
    ready_queue_count = 0;

    mkrn_process_t *idle = mkrn_execve_create_idle();
    if (!idle) {
        mkrn_panic("Failed to create idle process");
    }

    current = idle;
    process_ctrl.current = idle;
    process_ctrl.process_count = 1;
    process_ctrl.next_pid = 2;

    M4K_LOG_INFO("Idle process created: PID=1");
}

/* ── fill_info for getprocs ── */

int mkrn_process_fill_info(struct mkrn_procinfo *buf, uint32_t max)
{
    if (!buf || max == 0)
        return 0;

    uint32_t written = 0;

    /* Convert state_tags to legacy state values for userspace PS */
#define TAG_TO_STATE(tags) ( \
    ((tags) & M4K_ZOMBIE)     ? 3 : \
    ((tags) & M4K_SCHED_READY)  ? 1 : \
    ((tags) & M4K_SCHED_RUNNING) ? 0 : \
    ((tags) & M4K_SCHED_SLEEPING) ? 2 : 2)

    /* Write current first */
    if (current && written < max) {
        buf[written].pid = current->pid;
        buf[written].ppid = current->ppid;
        buf[written].state = TAG_TO_STATE(current->state_tags);
        mkrn_strncpy(buf[written].name, current->name, 31);
        buf[written].name[31] = '\0';
        written++;
    }

    /* Then ready queue */
    for (uint32_t i = 0; i < ready_queue_count && written < max; i++) {
        mkrn_process_t *p = ready_queue[i];
        if (p && p != current) {
            buf[written].pid = p->pid;
            buf[written].ppid = p->ppid;
            buf[written].state = TAG_TO_STATE(p->state_tags);
            mkrn_strncpy(buf[written].name, p->name, 31);
            buf[written].name[31] = '\0';
            written++;
        }
    }
#undef TAG_TO_STATE

    return (int)written;
}

/* ── 4P1 fork_status: clone with selective inheritance ── */

pid_t mkrn_fork_status(uint64_t inherit_mask, uint32_t flags)
{
    if (!(flags & RFPROC))
        return -1;

    if (inherit_mask & (M4K_ZOMBIE | M4K_IDLE | M4K_STATE_RESERVED))
        return -1;

    mkrn_process_t *parent = current;
    if (!parent)
        return -1;

    mkrn_process_t *child = (mkrn_process_t *)mkrn_alloc(sizeof(mkrn_process_t));
    if (!child)
        return -1;

    mkrn_memset(child, 0, sizeof(mkrn_process_t));

    /* Basic copy from parent */
    child->pid = next_pid();
    child->ppid = parent->pid;
    child->priority = parent->priority;
    child->uid = parent->uid;
    mkrn_strcpy(child->name, parent->name);
    mkrn_strcpy(child->cwd, parent->cwd);

    /* State inheritance: only inherit requested bits */
    child->state_tags = parent->state_tags & inherit_mask;

    /* If no scheduling class inherited, default to SCHED_READY */
    if (!(child->state_tags & M4K_STATE_SCHED_MASK))
        child->state_tags |= M4K_SCHED_READY;

    /* File descriptors */
    if (flags & RFFDG) {
        /* Copy FD table - FIXME: this would need VFS integration */
    }

    /* Namespace */
    if (flags & RFNAMEG) {
        mkrn_ns_copy(&child->ns, &parent->ns);
    } else {
        mkrn_ns_init(&child->ns);
    }

    /* Environment */
    if (flags & RFENVG) {
        child->envp = parent->envp; /* shared */
    } else {
        child->envp = NULL;
    }

    /* Memory address space */
    if (!(flags & RFMEM)) {
        /* FIXME: proper address space copy would need paging support */
    }

    /* Stack: allocate new stack for child */
    uint32_t *stack = (uint32_t *)mkrn_alloc(M4K_STACK_SIZE);
    if (!stack) {
        mkrn_free(child);
        return -1;
    }
    uint32_t *sp = (uint32_t *)((uint32_t)stack + M4K_STACK_SIZE);

    /* Push initial return address and zeros */
    *--sp = 0xDEAD0000; /* marker */
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    child->thread_esp = (uint32_t)sp;
    child->next = NULL;

    process_ctrl.process_count++;
    ready_enqueue(child);

    return (pid_t)child->pid;
}

/* ── 4P1 waitpid ── */

pid_t mkrn_waitpid(pid_t pid, int *status, int options)
{
    (void)options; /* blocking wait only for now */

    while (1) {
        mkrn_process_t *child = NULL;

        /* Find a matching child */
        for (uint32_t i = 0; i < ready_queue_count; i++) {
            if (ready_queue[i]->ppid == mkrn_process_get_pid()) {
                if (pid == -1 || (uint32_t)pid == ready_queue[i]->pid) {
                    child = ready_queue[i];
                    break;
                }
            }
        }

        if (!child) {
            if (options & M4K_WNOHANG)
                return 0;
            /* FIXME: block on WAIT_CHILD */
            mkrn_process_yield();
            continue;
        }

        /* Check if child has exited */
        if (!(child->state_tags & M4K_ZOMBIE)) {
            if (options & M4K_WNOHANG)
                return 0;
            mkrn_process_yield();
            continue;
        }

        /* Reap the zombie */
        uint32_t exit_val = child->exit_status;
        pid_t child_pid = (pid_t)child->pid;

        /* Remove from ready queue */
        ready_remove(child);
        mkrn_free(child);

        if (status) {
            if (exit_val < M4K_NSIG) {
                /* Killed by signal */
                *status = exit_val; /* low 7 bits = signal number */
            } else {
                /* Normal exit */
                *status = (exit_val & 0xFF) << 8;
            }
        }

        return child_pid;
    }
}

/* ── 4P1 kill ── */

int mkrn_kill(pid_t pid, int sig)
{
    if (sig <= 0 || sig >= M4K_NSIG)
        return -1;

    mkrn_process_t *target = mkrn_process_find(pid);
    if (!target)
        return -1;

    /* PID 1 is immune to SIGKILL */
    if (pid == 1 && sig == M4K_SIGKILL)
        return 0;

    mkrn_signal_deliver(target, sig);
    return 0;
}

/* ── 4P1 setns (namespace control) ── */

int mkrn_setns(const char *path, const char *target, uint32_t flags)
{
    if (!current || !path || !target)
        return -1;

    return mkrn_ns_set(&current->ns, path, target, flags);
}

/* ── 4P1 spawn (minimal stub) ── */

pid_t mkrn_spawn(const char *path, char *const argv[],
                 char *const envp[], uint32_t flags)
{
    /* FIXME: implement full ELF loading for new process */
    (void)path;
    (void)argv;
    (void)envp;
    (void)flags;
    return -1;
}

/* ── Signal pending check ── */

int mkrn_process_has_pending_signal(mkrn_process_t *p)
{
    if (!p) return 0;
    return (p->pending_signals != 0);
}


