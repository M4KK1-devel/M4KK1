/*
 * M4KK1 4P1 - m4k_arch.h
 * Description: x86_64 architecture-specific definitions and inline helpers.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

/* Architecture identification */
#define M4K_ARCH_X86_64         1
#define M4K_ARCH_NAME           "x86_64"
#define M4K_ARCH_BITS           64

/* Memory layout */
#define KERNEL_BASE             0xFFFFFFFF80000000ULL
#define KERNEL_HEAP             0xFFFFFFFF90000000ULL
#define USER_BASE               0x0000000000000000ULL
#define USER_STACK_TOP          0x00007FFFFFFFFFFFULL

/* Stack sizes */
#define KERNEL_STACK_SIZE       0x1000
#define USER_STACK_SIZE         0x10000

/* Page size */
#define PAGE_SIZE               0x1000
#define PAGE_SHIFT              12
#define PAGE_MASK               (~(PAGE_SIZE - 1))

/* Segment selectors */
#define KERNEL_CODE_SEGMENT     0x08
#define KERNEL_DATA_SEGMENT     0x10
#define USER_CODE_SEGMENT       0x18
#define USER_DATA_SEGMENT       0x20

/* Interrupt-related */
#define IDT_ENTRIES             256
#define IDT_BASE                0x0000000000000000ULL
#define IDT_LIMIT               (IDT_ENTRIES * 16 - 1)

/* GDT-related */
#define GDT_ENTRIES             5
#define GDT_BASE                0x0000000000001000ULL
#define GDT_LIMIT               (GDT_ENTRIES * 8 - 1)

/* TSS-related */
#define TSS_BASE                0x0000000000002000ULL
#define TSS_LIMIT               0x67
#define TSS_SEGMENT             0x28

/* System calls */
#define SYSCALL_INTERRUPT       0x80
#define M4K_SYSCALL_INTERRUPT   0x4D

/* Register structure */
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags, cr3;
    uint64_t cs, ss, ds, es, fs, gs;
} mkrn_registers_t;

/* Interrupt stack frame */
typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} mkrn_interrupt_frame_t;

/* Page table entry types */
typedef uint64_t pte_t;
typedef uint64_t pde_t;
typedef uint64_t pdpte_t;
typedef uint64_t pml4e_t;

/* Page table flags */
#define PTE_PRESENT             (1ULL << 0)
#define PTE_WRITABLE            (1ULL << 1)
#define PTE_USER                (1ULL << 2)
#define PTE_WRITE_THROUGH       (1ULL << 3)
#define PTE_CACHE_DISABLE       (1ULL << 4)
#define PTE_ACCESSED            (1ULL << 5)
#define PTE_DIRTY               (1ULL << 6)
#define PTE_LARGE_PAGE          (1ULL << 7)
#define PTE_GLOBAL              (1ULL << 8)
#define PTE_NO_EXECUTE          (1ULL << 63)

/* MSR - Memory Type Range Registers */
#define MSR_MTRR_BASE           0x200
#define MSR_MTRR_MASK           0x201
#define MSR_MTRR_DEF_TYPE       0x2FF

/* Memory types */
#define MTRR_TYPE_UC            0x00
#define MTRR_TYPE_WC            0x01
#define MTRR_TYPE_WT            0x04
#define MTRR_TYPE_WP            0x05
#define MTRR_TYPE_WB            0x06

/* CPU feature flags */
#define CPUID_FEAT_ECX_SSE3     (1 << 0)
#define CPUID_FEAT_ECX_PCLMUL   (1 << 1)
#define CPUID_FEAT_ECX_DTES64   (1 << 2)
#define CPUID_FEAT_ECX_MONITOR  (1 << 3)
#define CPUID_FEAT_ECX_DS_CPL   (1 << 4)
#define CPUID_FEAT_ECX_VMX      (1 << 5)
#define CPUID_FEAT_ECX_SMX      (1 << 6)
#define CPUID_FEAT_ECX_EST      (1 << 7)
#define CPUID_FEAT_ECX_TM2      (1 << 8)
#define CPUID_FEAT_ECX_SSSE3    (1 << 9)
#define CPUID_FEAT_ECX_CID      (1 << 10)
#define CPUID_FEAT_ECX_FMA      (1 << 12)
#define CPUID_FEAT_ECX_CX16     (1 << 13)
#define CPUID_FEAT_ECX_ETPRD    (1 << 14)
#define CPUID_FEAT_ECX_PDCM     (1 << 15)
#define CPUID_FEAT_ECX_DCA      (1 << 18)
#define CPUID_FEAT_ECX_SSE4_1   (1 << 19)
#define CPUID_FEAT_ECX_SSE4_2   (1 << 20)
#define CPUID_FEAT_ECX_x2APIC   (1 << 21)
#define CPUID_FEAT_ECX_MOVBE    (1 << 22)
#define CPUID_FEAT_ECX_POPCNT   (1 << 23)
#define CPUID_FEAT_ECX_AES      (1 << 25)
#define CPUID_FEAT_ECX_XSAVE    (1 << 26)
#define CPUID_FEAT_ECX_OSXSAVE  (1 << 27)
#define CPUID_FEAT_ECX_AVX      (1 << 28)
#define CPUID_FEAT_ECX_F16C     (1 << 29)
#define CPUID_FEAT_ECX_RDRAND   (1 << 30)

/**
 * mkrn_arch_init - Initialize x86_64 architecture
 *
 * Perform x86_64-specific architecture initialization including
 * SSE/AVX enablement and feature detection.
 */
void mkrn_arch_init(void);

/**
 * mkrn_arch_detect_features - Detect CPU features via CPUID
 *
 * Probe available CPU features and store results for later use.
 */
void mkrn_arch_detect_features(void);

/**
 * mkrn_arch_setup_paging - Set up 4-level page tables
 *
 * Initialize the kernel page table hierarchy.
 */
void mkrn_arch_setup_paging(void);

/**
 * mkrn_arch_enable_sse - Enable SSE instructions
 *
 * Set the OSFXSR and OSXMMEXCPT bits in CR4.
 */
void mkrn_arch_enable_sse(void);

/**
 * mkrn_arch_enable_avx - Enable AVX instructions
 *
 * Enable AVX and related extended features via XCR0.
 */
void mkrn_arch_enable_avx(void);

static inline void m4k_cpuid(uint32_t leaf, uint32_t *eax,
                             uint32_t *ebx, uint32_t *ecx,
                             uint32_t *edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

static inline uint64_t m4k_read_msr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

static inline void m4k_write_msr(uint32_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile (
        "wrmsr"
        : : "a"(low), "d"(high), "c"(msr)
    );
}

static inline void m4k_enable_interrupts(void)
{
    __asm__ volatile ("sti");
}

static inline void m4k_disable_interrupts(void)
{
    __asm__ volatile ("cli");
}

static inline void m4k_halt(void)
{
    __asm__ volatile ("hlt");
}

static inline void m4k_pause(void)
{
    __asm__ volatile ("pause");
}

static inline uint64_t m4k_read_cr0(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr0, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr0(uint64_t value)
{
    __asm__ volatile ("movq %0, %%cr0" : : "r"(value));
}

static inline uint64_t m4k_read_cr3(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr3(uint64_t value)
{
    __asm__ volatile ("movq %0, %%cr3" : : "r"(value));
}

static inline uint64_t m4k_read_cr4(void)
{
    uint64_t value;
    __asm__ volatile ("movq %%cr4, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr4(uint64_t value)
{
    __asm__ volatile ("movq %0, %%cr4" : : "r"(value));
}

static inline uint32_t m4k_atomic_exchange(uint32_t *ptr,
                                           uint32_t value)
{
    __asm__ volatile (
        "xchgl %0, %1"
        : "+r"(value), "+m"(*ptr)
        : : "memory"
    );
    return value;
}

static inline uint32_t m4k_atomic_compare_exchange(
    uint32_t *ptr, uint32_t old_val, uint32_t new_val)
{
    uint32_t result;
    __asm__ volatile (
        "lock cmpxchgl %2, %1"
        : "=a"(result), "+m"(*ptr)
        : "r"(new_val), "0"(old_val)
        : "memory"
    );
    return result;
}

static inline uint32_t m4k_atomic_add(uint32_t *ptr, uint32_t value)
{
    __asm__ volatile (
        "lock xaddl %0, %1"
        : "+r"(value), "+m"(*ptr)
        : : "memory"
    );
    return value;
}

static inline uint32_t m4k_atomic_increment(uint32_t *ptr)
{
    return m4k_atomic_add(ptr, 1);
}

static inline uint32_t m4k_atomic_decrement(uint32_t *ptr)
{
    return m4k_atomic_add(ptr, -1);
}

static inline void m4k_memory_barrier(void)
{
    __asm__ volatile ("mfence" : : : "memory");
}

static inline void m4k_read_barrier(void)
{
    __asm__ volatile ("lfence" : : : "memory");
}

static inline void m4k_write_barrier(void)
{
    __asm__ volatile ("sfence" : : : "memory");
}
