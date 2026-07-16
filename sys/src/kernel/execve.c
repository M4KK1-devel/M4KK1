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
#include <string.h>

extern unsigned char init_init_elf[];
extern unsigned int init_init_elf_len;

static mkrn_process_t *idle_process = NULL;

static void idle_loop(void)
{
    while (1) {
        __asm__ volatile("hlt");
    }
}

int mkrn_execve(u8 *elf_data, u32 size)
{
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

    M4K_LOG_INFO("execve: ELF loaded, entry=0x");
    mkrn_console_write_hex(ehdr->e_entry);
    mkrn_console_write("\n");

    mkrn_process_t *init = mkrn_process_get_current();
    if (!init) {
        M4K_LOG_ERROR("execve: no current process");
        return -1;
    }

    mkrn_strcpy(init->name, "init");
    mkrn_strcpy(init->cwd, "/");

    u32 *stack = (u32 *)mkrn_alloc(M4K_STACK_SIZE);
    if (!stack) {
        M4K_LOG_ERROR("execve: failed to allocate stack");
        return -1;
    }

    u32 *sp = (u32 *)(
        (u32)stack + M4K_STACK_SIZE);

    *--sp = ehdr->e_entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    init->thread_esp = (u32)sp;
    init->state_tags = M4K_SCHED_READY;

    M4K_LOG_INFO("execve: init process ready, stack at 0x");
    mkrn_console_write_hex((u32)stack);
    mkrn_console_write(" thread_esp=0x");
    mkrn_console_write_hex(init->thread_esp);
    mkrn_console_write("\n");

    return 0;
}

static void setup_idle_stack(mkrn_process_t *proc)
{
    u32 *stack = (u32 *)mkrn_alloc(M4K_STACK_SIZE);
    u32 *sp = (u32 *)(
        (u32)stack + M4K_STACK_SIZE);

    *--sp = (u32)idle_loop;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    proc->thread_esp = (u32)sp;
    proc->state_tags = M4K_SCHED_READY;
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

    setup_idle_stack(proc);
    return proc;
}
