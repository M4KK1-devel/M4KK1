/*
 * M4KK1 4P1 - process.c
 * Description: Process management and scheduler implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "process.h"
#include "timer.h"
#include "memory.h"
#include "kernel.h"
#include "console.h"
#include "syscall.h"
#include <string.h>
#include <stdint.h>

struct procinfo {
    u32 pid;
    u32 ppid;
    u32 state;
    char name[32];
};

#ifndef asm
#define asm __asm__
#endif

static mkrn_process_ctrl_t process_control;
static mkrn_process_t *current_process = NULL;
static u32 scheduler_enabled = 0;

#define READY_QUEUE_SIZE 64
static mkrn_process_t *ready_queue[READY_QUEUE_SIZE];
static u32 ready_queue_count = 0;

static u32 next_pid(void)
{
    return process_control.next_pid++;
}

static int ready_enqueue(mkrn_process_t *p)
{
    if (ready_queue_count >= READY_QUEUE_SIZE)
        return -1;
    ready_queue[ready_queue_count++] = p;
    return 0;
}

static mkrn_process_t *ready_dequeue(void)
{
    if (ready_queue_count == 0)
        return NULL;
    mkrn_process_t *p = ready_queue[0];
    ready_queue_count--;
    for (u32 i = 0; i < ready_queue_count; i++)
        ready_queue[i] = ready_queue[i + 1];
    return p;
}

static void ready_remove(mkrn_process_t *p)
{
    for (u32 i = 0; i < ready_queue_count; i++) {
        if (ready_queue[i] == p) {
            ready_queue_count--;
            for (u32 j = i; j < ready_queue_count; j++)
                ready_queue[j] = ready_queue[j + 1];
            return;
        }
    }
}

mkrn_process_t *mkrn_execve_create_idle(void);

void mkrn_process_init(void)
{
    M4K_LOG_INFO("Initializing process management...");

    mkrn_memset(&process_control, 0,
                sizeof(mkrn_process_ctrl_t));
    process_control.next_pid = 1;
    ready_queue_count = 0;

    mkrn_process_t *idle = mkrn_execve_create_idle();
    if (!idle) {
        mkrn_panic("Failed to create idle process");
    }
    mkrn_console_write("  idle_proc=0x");
    mkrn_console_write_hex((u32)idle);
    mkrn_console_write("\n");

    current_process = idle;
    process_control.current = idle;
    process_control.process_count = 1;
    process_control.next_pid = 2;

    M4K_LOG_INFO("Idle process created: PID=1");
}

mkrn_process_t *mkrn_process_get_current(void)
{
    return current_process;
}

static void process_set_current(mkrn_process_t *p)
{
    current_process = p;
    process_control.current = p;
}

u32 mkrn_process_get_pid(void)
{
    return current_process ? current_process->pid : 0;
}

u32 mkrn_process_get_count(void)
{
    return process_control.process_count;
}

void mkrn_sched_start(void)
{
    scheduler_enabled = 1;
    M4K_LOG_INFO("Scheduler started");
}

void mkrn_process_yield(void)
{
    if (!scheduler_enabled || !current_process)
        return;

    u32 saved_esp;

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

    current_process->thread_esp = saved_esp;

    if (current_process->state == M4K_PROC_RUNNING) {
        current_process->state = M4K_PROC_READY;
        ready_enqueue(current_process);
    }

    mkrn_process_t *next = ready_dequeue();
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

    process_set_current(next);
    next->state = M4K_PROC_RUNNING;

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
    if (!current_process)
        return;
    current_process->state = M4K_PROC_BLOCKED;
    mkrn_process_yield();
}

void mkrn_process_wakeup(mkrn_process_t *process)
{
    if (!process || process->state != M4K_PROC_BLOCKED)
        return;
    process->state = M4K_PROC_READY;
    ready_enqueue(process);
}

void mkrn_process_switch_first(void)
{
    if (!current_process) {
        mkrn_panic("No process to switch to");
    }

    mkrn_process_t *p = current_process;
    p->state = M4K_PROC_RUNNING;
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

int mkrn_process_fill_info(struct procinfo *buf, u32 max)
{
    if (!buf || max == 0)
        return 0;
    u32 written = 0;

    if (current_process && written < max) {
        buf[written].pid = current_process->pid;
        buf[written].ppid = current_process->ppid;
        buf[written].state = current_process->state;
        int i;
        for (i = 0;
             i < 31 && current_process->name[i];
             i++)
            buf[written].name[i] =
                current_process->name[i];
        buf[written].name[i] = '\0';
        written++;
    }

    for (u32 rq = 0;
         rq < ready_queue_count && written < max;
         rq++) {
        mkrn_process_t *p = ready_queue[rq];
        if (p && p != current_process) {
            buf[written].pid = p->pid;
            buf[written].ppid = p->ppid;
            buf[written].state = p->state;
            int i;
            for (i = 0; i < 31 && p->name[i]; i++)
                buf[written].name[i] = p->name[i];
            buf[written].name[i] = '\0';
            written++;
        }
    }
    return written;
}

void mkrn_process_exit(void)
{
    if (!current_process)
        return;

    M4K_LOG_INFO("Process exiting: PID=");
    mkrn_console_write_dec(current_process->pid);
    mkrn_console_write("\n");

    current_process->state = M4K_PROC_TERMINATED;
    process_control.process_count--;

    mkrn_process_t *next = ready_dequeue();
    if (next) {
        process_set_current(next);
        next->state = M4K_PROC_RUNNING;

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
