/*
 * M4KK1 4P1 - process.h
 * Description: Process management and scheduler declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_PROC_RUNNING     0
#define M4K_PROC_READY       1
#define M4K_PROC_BLOCKED     2
#define M4K_PROC_TERMINATED  3

#define M4K_PRIO_HIGH   0
#define M4K_PRIO_NORMAL 1
#define M4K_PRIO_LOW    2

typedef struct mkrn_process {
    u32 pid;
    u32 ppid;
    u32 state;
    u32 priority;
    u32 thread_esp;
    u32 sleep_ticks;
    char name[32];
    char cwd[256];
    struct mkrn_process *next;
} mkrn_process_t;

typedef struct {
    mkrn_process_t *current;
    mkrn_process_t *ready_queue;
    u32 process_count;
    u32 next_pid;
} mkrn_process_ctrl_t;

struct procinfo;

/**
 * mkrn_process_init - Initialize process subsystem
 *
 * Return: void
 */
void mkrn_process_init(void);

/**
 * mkrn_execve - Execute a new process from ELF data
 * @elf_data: Pointer to ELF binary data
 * @size: Size of ELF data
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_execve(u8 *elf_data, u32 size);

/**
 * mkrn_sched_start - Start the scheduler
 *
 * Return: void
 */
void mkrn_sched_start(void);

/**
 * mkrn_process_yield - Yield the current process
 *
 * Return: void
 */
void mkrn_process_yield(void);

/**
 * mkrn_process_switch_first - Switch to first process
 *
 * Return: void
 */
void mkrn_process_switch_first(void);

/**
 * mkrn_process_exit - Terminate current process
 *
 * Return: void
 */
void mkrn_process_exit(void);

/**
 * mkrn_process_block - Block current process
 *
 * Return: void
 */
void mkrn_process_block(void);

/**
 * mkrn_process_wakeup - Wake up a blocked process
 * @process: Process to wake up
 *
 * Return: void
 */
void mkrn_process_wakeup(mkrn_process_t *process);

/**
 * mkrn_process_get_current - Get current process
 *
 * Return: Pointer to current process
 */
mkrn_process_t *mkrn_process_get_current(void);

/**
 * mkrn_process_get_pid - Get current process PID
 *
 * Return: Current PID
 */
u32 mkrn_process_get_pid(void);

/**
 * mkrn_process_get_count - Get number of processes
 *
 * Return: Process count
 */
u32 mkrn_process_get_count(void);

/**
 * mkrn_process_fill_info - Fill process info buffer
 * @buf: Buffer to fill
 * @max: Maximum number of entries
 *
 * Return: Number of entries filled
 */
int mkrn_process_fill_info(struct procinfo *buf, u32 max);
