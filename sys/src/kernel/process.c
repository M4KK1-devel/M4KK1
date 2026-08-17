/*
 * M4KK1 4P1 - process.c
 * Description: Process management core implementation
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <process.h>
#include <timer.h>
#include <memory.h>
#include <gdt.h>
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

/* Forward: reaping (defined below the scheduler) */
static void mkrn_process_reap(mkrn_process_t *p);

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

/* All-process registry: every PCB is linked here from creation until
 * its parent reaps it via waitpid.  The ready queue only holds tasks
 * waiting for the CPU; sleeping and zombie processes must stay
 * findable for kill()/waitpid()/procfs, which walk this list. */
static mkrn_process_t *all_procs = NULL;
static uint32_t all_procs_count = 0;

static void proc_registry_add(mkrn_process_t *p)
{
    if (!p)
        return;
    p->next = all_procs;
    all_procs = p;
    all_procs_count++;
}

static void proc_registry_remove(mkrn_process_t *p)
{
    mkrn_process_t **pp = &all_procs;
    while (*pp) {
        if (*pp == p) {
            *pp = p->next;
            p->next = NULL;
            all_procs_count--;
            return;
        }
        pp = &(*pp)->next;
    }
}

/* Release every allocation owned by a reaped zombie PCB.  Called from
 * waitpid after the parent harvested the exit status, at which point
 * no scheduler state can reference the process anymore. */
static void proc_free_zombie(mkrn_process_t *p)
{
    if (p->user_stack_base)
        mkrn_free((void *)p->user_stack_base);
    if (p->kernel_stack)
        mkrn_free((void *)(p->kernel_stack - M4K_STACK_SIZE));
    proc_registry_remove(p);
    mkrn_free(p);
}

/* Stall probe: dump current process + ready-queue contents */
void mkrn_process_dump_sched(void)
{
    mkrn_console_write("Q cur=");
    if (current) {
        mkrn_console_write_dec(current->pid);
        mkrn_console_write(":");
        mkrn_console_write(current->name);
        mkrn_console_write(" esp=");
        mkrn_console_write_hex(current->thread_esp);
        mkrn_console_write(" tags=");
        mkrn_console_write_hex((uint32_t)current->state_tags);
    } else {
        mkrn_console_write("none");
    }
    mkrn_console_write(" q=[");
    for (uint32_t i = 0; i < ready_queue_count; i++) {
        if (i > 0)
            mkrn_console_write(",");
        mkrn_console_write_dec(ready_queue[i]->pid);
        mkrn_console_write(":");
        mkrn_console_write_hex((uint32_t)ready_queue[i]->state_tags);
    }
    mkrn_console_write("]\n");
}

int mkrn_process_is_ready(pid_t pid)
{
    for (uint32_t i = 0; i < ready_queue_count; i++)
        if (ready_queue[i]->pid == (uint32_t)pid)
            return 1;
    return 0;
}

uint32_t mkrn_process_get_tags(pid_t pid)
{
    mkrn_process_t *p = mkrn_process_find(pid);
    return p ? (uint32_t)p->state_tags : 0;
}

uint32_t mkrn_process_get_thread_esp(pid_t pid)
{
    mkrn_process_t *p = mkrn_process_find(pid);
    return p ? p->thread_esp : 0;
}

static uint32_t next_pid(void)
{
    return process_ctrl.next_pid++;
}

/* ── Process lookup ── */

mkrn_process_t *mkrn_process_find(pid_t pid)
{
    /* Walk the all-process registry: the ready queue alone misses
     * sleeping and zombie processes, breaking kill/procfs/reparent. */
    for (mkrn_process_t *p = all_procs; p; p = p->next) {
        if (p->pid == (uint32_t)pid)
            return p;
    }

    /* Also check current (running right now) */
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

uint32_t mkrn_process_get_uid(void)
{
    return current ? current->uid : 0;
}
uint32_t mkrn_process_get_euid(void)
{
    return current ? current->euid : 0;
}
uint32_t mkrn_process_get_gid(void)
{
    return current ? current->gid : 0;
}
uint32_t mkrn_process_get_egid(void)
{
    return current ? current->egid : 0;
}
/* Check if current process is in the prime group (GID 1001) */
static int is_prime_member(void)
{
    if (!current) return 0;
    if (current->egid == M4K_GID_PRIME) return 1;
    for (uint32_t i = 0; i < current->group_count; i++) {
        if (current->group_list[i] == M4K_GID_PRIME)
            return 1;
    }
    return 0;
}

int mkrn_process_set_uid(uint32_t uid)
{
    if (!current) return -M4K_EPERM;
    if (current->euid == M4K_UID_ROOT) {
        /* Root can set both RUID and EUID (§6.12.4) */
        current->uid = uid;
        current->euid = uid;
    } else if (uid == 0 && is_prime_member()) {
        /* Prime group members can escalate to root (§6.9) */
        current->euid = uid;
    } else if (uid == current->uid || uid == current->euid) {
        /* Non-root can only set EUID (§6.12.4) */
        current->euid = uid;
    } else {
        return -M4K_EPERM;
    }
    return 0;
}
int mkrn_process_set_gid(uint32_t gid)
{
    if (!current) return -M4K_EPERM;
    if (current->egid == M4K_GID_ROOT) {
        /* Root can set both RGID and EGID (§6.12.4) */
        current->gid = gid;
        current->egid = gid;
    } else if (gid == current->gid || gid == current->egid) {
        /* Non-root can only set EGID (§6.12.4) */
        current->egid = gid;
    } else {
        return -M4K_EPERM;
    }
    return 0;
}

int mkrn_process_get_groups(uint32_t *list, int size)
{
    if (!current) return -M4K_EPERM;
    if (!list || size <= 0) return (int)current->group_count;
    int count = (int)current->group_count;
    if (size < count) count = size;
    for (int i = 0; i < count; i++)
        list[i] = current->group_list[i];
    return count;
}

int mkrn_process_set_groups(int size, const uint32_t *list)
{
    if (!current) return -M4K_EPERM;
    if (current->euid != M4K_UID_ROOT)
        return -M4K_EPERM;
    if (size < 0) size = 0;
    if (size > 16) size = 16;
    current->group_count = (uint32_t)size;
    for (int i = 0; i < size; i++)
        current->group_list[i] = list[i];
    return 0;
}

int mkrn_process_set_rlimit(int resource, const struct m4k_rlimit *rlp)
{
    if (!current || !rlp) return -M4K_EINVAL;
    if (resource < 0 || resource >= M4K_RLIMIT_NLIMITS)
        return -M4K_EINVAL;
    if (current->euid != M4K_UID_ROOT) {
        if (rlp->rlim_max > current->rlimits[resource].rlim_max)
            return -M4K_EPERM;
    }
    current->rlimits[resource].rlim_cur = rlp->rlim_cur;
    current->rlimits[resource].rlim_max = rlp->rlim_max;
    return 0;
}

int mkrn_process_get_rlimit(int resource, struct m4k_rlimit *rlp)
{
    if (!current || !rlp) return -M4K_EINVAL;
    if (resource < 0 || resource >= M4K_RLIMIT_NLIMITS)
        return -M4K_EINVAL;
    rlp->rlim_cur = current->rlimits[resource].rlim_cur;
    rlp->rlim_max = current->rlimits[resource].rlim_max;
    return 0;
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

void mkrn_process_yield(void);

/* Scratch slot used by the naked context-switch trampoline below.
 * Exported (not static): the naked asm references it by absolute
 * symbol name, which linkers only resolve for global symbols. */
uint32_t g_switch_esp;

/* Pick the next process to run.  Called with g_switch_esp holding this
 * process's kernel stack pointer (after the 4-register push block of
 * the trampoline).  On a switch, g_switch_esp is overwritten with the
 * next process's thread_esp and that process is returned (non-NULL).
 * Returns NULL when the caller should simply continue. */
mkrn_process_t *mkrn_process_switch_pick(void)
{
    if (!scheduler_enabled || !current)
        return NULL;

    current->thread_esp = g_switch_esp;

    /* Re-enqueue if still running (yield) or sleeping (yield after block) */
    if (current->state_tags & M4K_SCHED_RUNNING) {
        current->state_tags &= ~M4K_SCHED_RUNNING;
        current->state_tags |= M4K_SCHED_READY;
        ready_enqueue(current);
    }

    /* Dequeue next ready process (skip stopped, reap terminated) */
    mkrn_process_t *next = NULL;
    uint32_t scanned = 0;
    while (scanned < ready_queue_count + 1) {
        next = ready_dequeue();
        if (!next)
            break;
        scanned++;
        if (next->state_tags & M4K_TERMINATE) {
            mkrn_process_reap(next);
            scanned--;          /* reaped task left the queue */
            continue;
        }
        if (!(next->state_tags & M4K_STOPPED) &&
            (next->state_tags & M4K_SCHED_READY)) {
            break;
        }
        /* Put stopped/non-ready back at end */
        ready_enqueue(next);
    }
    if (scanned >= ready_queue_count + 1)
        next = NULL;            /* every queued task is stopped: spin idly */

    if (next) {
        current = next;
        next->state_tags &= ~(M4K_SCHED_READY | M4K_SCHED_SLEEPING);
        next->state_tags |= M4K_SCHED_RUNNING;
        g_switch_esp = next->thread_esp;
        mkrn_set_kernel_stack(next->kernel_stack);
    }
    return next;
}

/* Cooperative context switch.  Naked: the whole save/resume lives in
 * one inline-asm block, so no compiler-generated function frame sits
 * between the saved stack pointer and the return address it must pop.
 *
 * Frame protocol (identical for every resume path, including the
 * scheduler and mkrn_process_exit):
 *   [ebp][edi][esi][ebx] then ret-address into the caller; the forked
 *   child instead has [trampoline] and an iret frame below.
 */
__attribute__((naked)) void mkrn_process_yield(void)
{
    __asm__ volatile(
        "pushl %%ebp\n"
        "pushl %%edi\n"
        "pushl %%esi\n"
        "pushl %%ebx\n"
        "movl %%esp, g_switch_esp\n"
        "call mkrn_process_switch_pick\n"
        "movl g_switch_esp, %%esp\n"
        "popl %%ebx\n"
        "popl %%esi\n"
        "popl %%edi\n"
        "popl %%ebp\n"
        "ret\n"
        : : : "memory");
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
    /* NOTE: do NOT ready_enqueue(p) here.  The ready queue holds
     * processes that are waiting to be resumed by a future yield;
     * this process starts running directly and its thread_esp frame
     * would otherwise be consumed now while a stale queue entry still
     * points at it, so the first mkrn_process_yield() would resume a
     * process with an already-consumed frame (garbage EIP -> #UD). */
    scheduler_enabled = 1;

    mkrn_set_kernel_stack(p->kernel_stack);

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

/* Reap a process: mark its state, notify parent, detach children.
 * Used both by mkrn_process_exit() (current process) and by the
 * scheduler when picking up a M4K_TERMINATE-marked task. */
static void mkrn_process_reap(mkrn_process_t *p)
{
    M4K_LOG_INFO("Process exiting: PID=");
    mkrn_console_write_dec(p->pid);
    mkrn_console_write("\n");

    /* Transition to zombie: keep the PCB (holding the exit status)
     * until the parent harvests it in waitpid, but stop scheduling. */
    p->state_tags &= ~(M4K_SCHED_RUNNING | M4K_SCHED_READY |
                       M4K_SCHED_SLEEPING | M4K_STOPPED | M4K_TERMINATE);
    p->state_tags |= M4K_ZOMBIE;
    ready_remove(p);

    /* Send SIGCHLD to parent */
    mkrn_process_t *parent = mkrn_process_find((pid_t)p->ppid);
    if (parent) {
        mkrn_kill((pid_t)p->ppid, M4K_SIGCHLD);
    }

    process_ctrl.process_count--;

    /* Reparent children to PID 1 */
    mkrn_process_reparent_children(p);
}

/* Dequeue the next runnable task, reaping any that were marked for
 * termination (M4K_TERMINATE).  Returns NULL when the queue is empty. */
static mkrn_process_t *mkrn_process_dequeue_live(void)
{
    for (;;) {
        mkrn_process_t *p = ready_dequeue();
        if (!p)
            return NULL;
        if (p->state_tags & M4K_TERMINATE) {
            mkrn_process_reap(p);
            continue;
        }
        return p;
    }
}

void mkrn_process_exit(int status)
{
    if (!current)
        return;

    current->exit_status = status;
    mkrn_process_reap(current);
    mkrn_process_t *dying = current;

    /* Switch to the next task */
    mkrn_process_t *next = mkrn_process_dequeue_live();
    if (next) {
        /* Publish the handoff BEFORE any logging: mkrn_console_write
         * paths can deliver interrupts whose handlers dereference
         * `current`, which would still point at the reaped zombie
         * PCB below.  The dying PCB stays valid until waitpid reaps
         * it, so reading its fields after the switch is safe. */
        current = next;
        next->state_tags &= ~(M4K_SCHED_READY | M4K_SCHED_SLEEPING);
        next->state_tags |= M4K_SCHED_RUNNING;

        mkrn_set_kernel_stack(next->kernel_stack);

        mkrn_console_write("[SW]save=0x");
        mkrn_console_write_hex(dying->thread_esp);
        mkrn_console_write(" kstk=0x");
        mkrn_console_write_hex(next->kernel_stack);
        mkrn_console_write(" next=");
        mkrn_console_write_dec(next->pid);
        mkrn_console_write(" esp=0x");
        mkrn_console_write_hex(next->thread_esp);
        mkrn_console_write(" state=0x");
        mkrn_console_write_hex((uint32_t)next->state_tags);
        mkrn_console_write("\n");

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

    for (mkrn_process_t *p = all_procs; p; p = p->next) {
        if (p->ppid == parent->pid && p != parent) {
            p->ppid = 1;
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
    all_procs = NULL;
    all_procs_count = 0;

    mkrn_process_t *idle = mkrn_execve_create_idle();
    if (!idle) {
        mkrn_panic("Failed to create idle process");
    }

    current = idle;
    process_ctrl.current = idle;
    process_ctrl.process_count = 1;
    process_ctrl.next_pid = 2;
    proc_registry_add(idle);

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

    /* Then every other process in the registry (includes sleeping
     * and zombie processes the ready queue never shows). */
    for (mkrn_process_t *p = all_procs; p && written < max; p = p->next) {
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

/* Captured by isr_syscall / isr_m4k_syscall at every syscall entry:
 * {user_eip, user_cs, user_eflags, user_esp, user_ss, user_ebp} */
uint32_t g_syscall_user_frame[6];

/* Resume trampoline for a freshly forked child: the scheduler pops
 * ebx/esi/edi/ebp and ret's here, then we pop the synthetic
 * interrupt frame and iret back into user mode right after the
 * fork syscall, with eax = 0 (child side). */
void mkrn_fork_child_restore(void)
{
    __asm__ volatile(
        "popl %%eax\n"
        "popl %%ebx\n"
        "popl %%ecx\n"
        "popl %%edx\n"
        "popl %%esi\n"
        "popl %%edi\n"
        "popl %%ebp\n"
        "iret\n"
        : : "r"(0) : "memory");
    __builtin_unreachable();
}

pid_t mkrn_fork_status(uint64_t inherit_mask, uint32_t flags)
{
    if (!(flags & RFPROC)) {
        M4K_LOG_ERROR("fork: no RFPROC");
        return -1;
    }

    if (inherit_mask & (M4K_ZOMBIE | M4K_IDLE | M4K_STATE_RESERVED)) {
        M4K_LOG_ERROR("fork: bad inherit_mask");
        return -1;
    }

    mkrn_process_t *parent = current;
    if (!parent) {
        M4K_LOG_ERROR("fork: no current");
        return -1;
    }

    mkrn_process_t *child = (mkrn_process_t *)mkrn_alloc(sizeof(mkrn_process_t));
    if (!child) {
        M4K_LOG_ERROR("fork: pcb alloc failed");
        return -1;
    }

    mkrn_memset(child, 0, sizeof(mkrn_process_t));

    /* Basic copy from parent */
    child->pid = next_pid();
    child->ppid = parent->pid;
    child->priority = parent->priority;
    child->uid = parent->uid;
    child->euid = parent->euid;
    child->gid = parent->gid;
    child->egid = parent->egid;
    child->group_count = parent->group_count;
    for (uint32_t gi = 0; gi < child->group_count; gi++)
        child->group_list[gi] = parent->group_list[gi];
    for (int ri = 0; ri < M4K_RLIMIT_NLIMITS; ri++) {
        child->rlimits[ri].rlim_cur = parent->rlimits[ri].rlim_cur;
        child->rlimits[ri].rlim_max = parent->rlimits[ri].rlim_max;
    }
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

    /* Private kernel stack for the child.  TSS esp0 is per-process
     * (set on each context switch), so the child's future syscalls
     * push onto its own stack instead of clobbering the parent's
     * still-pending ISR frame.  The child's resume block lives on
     * the top of this stack and is consumed once at first resume. */
    uint32_t *kstack = (uint32_t *)mkrn_alloc(M4K_STACK_SIZE);
    if (!kstack) {
        M4K_LOG_ERROR("fork: kstack alloc failed");
        mkrn_free(child);
        return -1;
    }
    child->kernel_stack = (uint32_t)kstack + M4K_STACK_SIZE;

    /* The child resumes in user mode right after the fork syscall,
     * with eax = 0 (child side).  It runs the parent's saved frame
     * chain (the legacy wrappers execute
     * `mov %eax,-0x4(%ebp); ...; leave; ret` right after `int $0x80`),
     * so it inherits the parent's user esp/ebp — relocated onto a
     * private byte-for-byte copy of the parent's stack (see below) so
     * the two processes never clobber each other's live frames.
     *
     * Scheduler resume protocol (mkrn_process_yield): esp is restored
     * to thread_esp, then pop ebx / pop esi / pop edi / pop ebp / ret.
     * So thread_esp must point at a block laid out (low -> high):
     *   [ebx][esi][edi][ebp][trampoline-ret]      <- yield pops + ret
     *   [eax][ebx][ecx][edx][esi][edi][ebp]       <- mkrn_fork_child_restore pops
     *   [eip][cs][eflags][esp][ss]                <- iret frame
     */
    uint32_t ueip    = g_syscall_user_frame[0];
    uint32_t ucs     = g_syscall_user_frame[1];
    uint32_t ueflags = g_syscall_user_frame[2];
    uint32_t uesp    = g_syscall_user_frame[3];
    uint32_t uss     = g_syscall_user_frame[4];
    uint32_t uebp    = g_syscall_user_frame[5];
    uint32_t child_top = child->kernel_stack;

    /* Private user stack for the child: a byte-for-byte copy of the
     * parent's stack.  Without this, the child resumes on the parent's
     * live frames and its continuation (m4k_spawn + wrapper calls)
     * pushes INTO the parent's saved-ebp/return-address slots, so the
     * parent later unwinds garbage when it resumes.  The resume esp/ebp
     * are remapped into the copy at the same relative offset. */
    if (parent->user_stack_base) {
        uint32_t *ustack = (uint32_t *)mkrn_alloc(M4K_STACK_SIZE);
        if (ustack) {
            mkrn_memcpy(ustack, (void *)parent->user_stack_base,
                        M4K_STACK_SIZE);
            uint32_t orig_base = parent->user_stack_base;
            uint32_t new_base = (uint32_t)ustack;
            child->user_stack_base = new_base;
            uesp = new_base + (uesp - orig_base);
            uebp = new_base + (uebp - orig_base);
            /* The byte copy still contains the parent's absolute frame
             * pointers; rebase every saved ebp in the chain the child
             * will unwind (each [ebp] slot up to the outermost frame). */
            uint32_t p = uebp;
            while (p >= new_base &&
                   p < new_base + M4K_STACK_SIZE) {
                uint32_t v = *(uint32_t *)p;
                if (v < orig_base ||
                    v >= orig_base + M4K_STACK_SIZE)
                    break;
                v = new_base + (v - orig_base);
                *(uint32_t *)p = v;
                p = v;
            }
        }
    }

    uint32_t *f = (uint32_t *)child_top;
    *--f = uss;
    *--f = uesp;                  /* user esp: resume on the child's stack copy */
    *--f = ueflags;
    *--f = ucs;
    *--f = ueip;
    *--f = uebp;                  /* user ebp: rebased into the stack copy */
    *--f = 0;                     /* edi */
    *--f = 0;                     /* esi */
    *--f = 0;                     /* edx */
    *--f = 0;                     /* ecx */
    *--f = 0;                     /* ebx */
    *--f = 0;                     /* eax: fork returns 0 in child */
    *--f = (uint32_t)mkrn_fork_child_restore;  /* trampoline ret target */
    *--f = 0;                     /* sched ebp */
    *--f = 0;                     /* sched edi */
    *--f = 0;                     /* sched esi */
    *--f = 0;                     /* sched ebx */
    child->thread_esp = (uint32_t)f;
    child->next = NULL;

    process_ctrl.process_count++;
    proc_registry_add(child);
    if (ready_queue_count >= READY_QUEUE_SIZE) {
        /* Queue full: undo the fork instead of returning a ghost pid
         * that the scheduler will silently drop. */
        M4K_LOG_ERROR("fork: ready queue full");
        proc_registry_remove(child);
        process_ctrl.process_count--;
        mkrn_free((void *)(child->kernel_stack - M4K_STACK_SIZE));
        if (child->user_stack_base)
            mkrn_free((void *)child->user_stack_base);
        mkrn_free(child);
        return -1;
    }
    ready_enqueue(child);

    return (pid_t)child->pid;
}

/* ── 4P1 waitpid ── */

pid_t mkrn_waitpid(pid_t pid, int *status, int options)
{
    (void)options; /* blocking wait only for now */

    while (1) {
        mkrn_process_t *child = NULL;

        /* Find a matching child in the all-process registry (the
         * ready queue misses zombies — they were removed on exit). */
        for (mkrn_process_t *p = all_procs; p; p = p->next) {
            if (p->ppid == mkrn_process_get_pid()
                && p != current) {
                if (pid == -1 || (uint32_t)pid == p->pid) {
                    child = p;
                    if (child->state_tags & M4K_ZOMBIE)
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

        /* Free the PCB and its stacks */
        proc_free_zombie(child);

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

/* Mark a process for termination; the scheduler reaps it the next
 * time it would run.  Safe to call from another process's syscall. */
void mkrn_process_terminate(pid_t pid)
{
    mkrn_process_t *target = mkrn_process_find(pid);
    if (!target)
        return;

    target->state_tags &= ~M4K_SCHED_SLEEPING;
    target->state_tags |= M4K_TERMINATE;
    if (!(target->state_tags & M4K_STATE_SCHED_MASK))
        target->state_tags |= M4K_SCHED_READY;
    /* A SLEEPING or STOPPED target is not in the ready queue — merely
     * setting M4K_SCHED_READY would never get it dispatched, so the
     * M4K_TERMINATE mark would sit forever and waitpid would hang.
     * Re-enqueue so the scheduler's reap path sees it (queue-full is
     * tolerable: a queued target gets reaped on its next dequeue). */
    if (!mkrn_process_is_ready(target->pid))
        ready_enqueue(target);
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


