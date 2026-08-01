/*
 * M4KK1 4P1 - execve.c
 * Description: ELF binary loading and process execution.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "elf.h"
#include "process.h"
#include "memory.h"
#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include <string.h>

extern unsigned char init_init_elf[];
extern unsigned int init_init_elf_len;

static void idle_loop(void)
{
    /* Never expected to run (kmain execve's init onto the idle pcb
     * before the first switch), but this is a ring-3 entry: `hlt` is
     * privileged and would #GP, so busy-spin instead. */
    for (;;)
        __asm__ volatile("pause" : : : "memory");
}

/* Entry trampoline for a brand-new process: the scheduler pops
 * ebx/esi/edi/ebp and ret's here, then we iret the synthetic ring-3
 * interrupt frame (built by execve) into user mode. */
__attribute__((naked)) void mkrn_process_entry_iret(void)
{
    __asm__ volatile("iret" : : : "memory");
    __builtin_unreachable();
}

int mkrn_execve(u8 *elf_data, u32 size, const char *proc_name)
{
    mkrn_process_t *init = mkrn_process_get_current();
    if (!init) {
        M4K_LOG_ERROR("execve: no current process");
        return -1;
    }

    mkrn_strcpy(init->name, proc_name ? proc_name : "unknown");
    mkrn_strcpy(init->cwd, "/");

    mkrn_elf32_ehdr_t *ehdr = (mkrn_elf32_ehdr_t *)elf_data;
    mkrn_elf32_phdr_t *phdr;
    u32 i;

    if (size < sizeof(mkrn_elf32_ehdr_t)) {
        M4K_LOG_ERROR("execve: ELF too small");
        return -1;
    }

    if (ehdr->e_ident[0] != M4K_ELFMAG0
        || ehdr->e_ident[1] != M4K_ELFMAG1
        || ehdr->e_ident[2] != M4K_ELFMAG2
        || ehdr->e_ident[3] != M4K_ELFMAG3) {
        M4K_LOG_ERROR("execve: bad ELF magic");
        return -1;
    }

    if (ehdr->e_ident[M4K_EI_CLASS] != M4K_ELFCLASS32) {
        M4K_LOG_ERROR("execve: not 32-bit ELF");
        return -1;
    }

    if (ehdr->e_type != M4K_ET_EXEC) {
        M4K_LOG_ERROR("execve: not executable");
        return -1;
    }

    if (ehdr->e_machine != M4K_EM_386) {
        M4K_LOG_ERROR("execve: not i386");
        return -1;
    }

    if (ehdr->e_phoff == 0
        || ehdr->e_phentsize == 0
        || ehdr->e_phnum == 0) {
        M4K_LOG_ERROR("execve: no program headers");
        return -1;
    }

    M4K_LOG_INFO("execve: loading ELF");

    for (i = 0; i < ehdr->e_phnum; i++) {
        phdr = (mkrn_elf32_phdr_t *)(
            elf_data
            + ehdr->e_phoff
            + i * ehdr->e_phentsize);

        if (phdr->p_type != M4K_PT_LOAD)
            continue;

        M4K_LOG_INFO("execve: LOAD segment");
        mkrn_console_write("  vaddr=0x");
        mkrn_console_write_hex(phdr->p_vaddr);
        mkrn_console_write(" filesz=0x");
        mkrn_console_write_hex(phdr->p_filesz);
        mkrn_console_write(" memsz=0x");
        mkrn_console_write_hex(phdr->p_memsz);
        mkrn_console_write("\n");

        mkrn_memcpy(
            (void *)phdr->p_vaddr,
            elf_data + phdr->p_offset,
            phdr->p_filesz);

        if (phdr->p_memsz > phdr->p_filesz) {
            mkrn_memset(
                (void *)(phdr->p_vaddr
                         + phdr->p_filesz),
                0,
                phdr->p_memsz - phdr->p_filesz);
        }
    }

    mkrn_console_write("[INFO] execve: ELF loaded, entry=0x");
    mkrn_console_write_hex(ehdr->e_entry);
    mkrn_console_write("\n");

    u32 *stack = (u32 *)mkrn_alloc(M4K_STACK_SIZE);
    if (!stack) {
        M4K_LOG_ERROR("execve: failed to allocate stack");
        return -1;
    }

    /* Ring-3 entry frame (low -> high):
     *   [ebx][esi][edi][ebp][mkrn_process_entry_iret]
     *   [eip][cs=0x23][eflags][esp][ss=0x2B]
     * The scheduler pops the 4 registers and ret's to the trampoline,
     * which iret's into user mode (CPL3).  From then on every `int`
     * switches to this process's private kernel stack via TSS esp0.
     * Selectors carry RPL=3 (0x23/0x2B): iret to an outer ring requires
     * SS RPL == new CS DPL, otherwise #GP. */
    u32 *sp = (u32 *)((u32)stack + M4K_STACK_SIZE);

    *--sp = M4K_GDT_USER_DATA | 0x3;        /* ss (ring 3, RPL=3) */
    *--sp = (u32)stack + M4K_STACK_SIZE;    /* user esp: fresh stack top */
    *--sp = 0x3202;                          /* eflags: IF + IOPL=3 (user port I/O) */
    *--sp = M4K_GDT_USER_CODE | 0x3;        /* cs (ring 3, RPL=3) */
    *--sp = ehdr->e_entry;                  /* eip */
    *--sp = (u32)mkrn_process_entry_iret;   /* scheduler ret target */
    *--sp = 0;                              /* sched ebp */
    *--sp = 0;                              /* sched edi */
    *--sp = 0;                              /* sched esi */
    *--sp = 0;                              /* sched ebx */

    init->thread_esp = (u32)sp;
    init->state_tags = M4K_SCHED_READY;

    /* Release the replaced image's user stack (idle's pre-init frame
     * or a fork child's private copy); the new frame above is the only
     * live reference from now on.  Done only here, after every
     * validation and the new stack allocation succeeded, so a failed
     * execve leaves the old stack intact. */
    if (init->user_stack_base) {
        mkrn_free((void *)init->user_stack_base);
        init->user_stack_base = 0;
    }
    init->user_stack_base = (u32)stack;

    mkrn_console_write("[INFO] execve: init process ready, stack at 0x");
    mkrn_console_write_hex((u32)stack);
    mkrn_console_write(" thread_esp=0x");
    mkrn_console_write_hex(init->thread_esp);
    mkrn_console_write("\n");

    return 0;
}

static void setup_idle_stack(mkrn_process_t *proc)
{
    u32 *stack = (u32 *)mkrn_alloc(M4K_STACK_SIZE);
    u32 *sp = (u32 *)((u32)stack + M4K_STACK_SIZE);

    /* Same ring-3 entry frame as execve (replaced before switch_first). */
    *--sp = M4K_GDT_USER_DATA | 0x3;
    *--sp = (u32)stack + M4K_STACK_SIZE;
    *--sp = 0x3202;                          /* eflags: IF + IOPL=3 (user code does direct port I/O) */
    *--sp = M4K_GDT_USER_CODE | 0x3;
    *--sp = (u32)idle_loop;
    *--sp = (u32)mkrn_process_entry_iret;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    proc->thread_esp = (u32)sp;
    proc->state_tags = M4K_SCHED_READY;
    proc->user_stack_base = (u32)stack;

    /* Private kernel stack: TSS esp0 is per-process (set on each
     * context switch), so a process's pending ISR frame can never be
     * clobbered by another process entering the kernel. */
    u32 *kstack = (u32 *)mkrn_alloc(M4K_STACK_SIZE);
    proc->kernel_stack = (u32)kstack + M4K_STACK_SIZE;
}

mkrn_process_t *mkrn_execve_create_idle(void)
{
    mkrn_process_t *proc =
        (mkrn_process_t *)mkrn_alloc(
            sizeof(mkrn_process_t));
    if (!proc)
        return NULL;

    mkrn_memset(proc, 0, sizeof(mkrn_process_t));
    proc->pid = 1;
    proc->ppid = 0;
    proc->priority = M4K_PRIO_LOW;
    mkrn_strcpy(proc->name, "idle");
    mkrn_strcpy(proc->cwd, "/");
    proc->rlimits[M4K_RLIMIT_NPROC].rlim_cur = 256;
    proc->rlimits[M4K_RLIMIT_NPROC].rlim_max = 256;
    proc->rlimits[M4K_RLIMIT_NOFILE].rlim_cur = 128;
    proc->rlimits[M4K_RLIMIT_NOFILE].rlim_max = 128;

    setup_idle_stack(proc);
    return proc;
}
