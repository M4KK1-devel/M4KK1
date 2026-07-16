/**
 * M4KK1 x86 Architecture Definitions
 * x86架构特定定义 (32位)
 */

#ifndef _M4K_ARCH_X86_H
#define _M4K_ARCH_X86_H

#include <stdint.h>

/* 架构标识 */
#define M4K_ARCH_X86            1
#define M4K_ARCH_NAME           "x86"
#define M4K_ARCH_BITS           32

/* 内存布局 */
#define KERNEL_BASE             0xC0000000UL   /* 3GB */
#define KERNEL_HEAP             0xC0400000UL   /* 3GB + 4MB */
#define USER_BASE               0x00000000UL   /* 0 */
#define USER_STACK_TOP          0xBFFFFFFFUL   /* 3GB - 1 */

/* 栈大小 */
#define KERNEL_STACK_SIZE       0x1000     /* 4KB */
#define USER_STACK_SIZE         0x10000    /* 64KB */

/* 页面大小 */
#define PAGE_SIZE               0x1000     /* 4KB */
#define PAGE_SHIFT              12
#define PAGE_MASK               (~(PAGE_SIZE - 1))

/* 段选择子 */
#define KERNEL_CODE_SEGMENT     0x08
#define KERNEL_DATA_SEGMENT     0x10
#define USER_CODE_SEGMENT       0x18
#define USER_DATA_SEGMENT       0x20

/* 中断相关 */
#define IDT_ENTRIES             256
#define IDT_BASE                0x00000000UL
#define IDT_LIMIT               (IDT_ENTRIES * 8 - 1)

/* GDT相关 */
#define GDT_ENTRIES             5
#define GDT_BASE                0x00001000UL
#define GDT_LIMIT               (GDT_ENTRIES * 8 - 1)

/* 系统调用 */
#define SYSCALL_INTERRUPT       0x80
#define M4K_SYSCALL_INTERRUPT   0x4D

/* 寄存器结构 */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags, cr3;
    uint32_t cs, ss, ds, es, fs, gs;
} m4k_registers_t;

/* 中断栈帧 */
typedef struct {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
} m4k_interrupt_frame_t;

/* 页表项 */
typedef uint32_t pte_t;
typedef uint32_t pde_t;

/* 页表标志 */
#define PTE_PRESENT             (1 << 0)
#define PTE_WRITABLE            (1 << 1)
#define PTE_USER                (1 << 2)
#define PTE_WRITE_THROUGH       (1 << 3)
#define PTE_CACHE_DISABLE       (1 << 4)
#define PTE_ACCESSED            (1 << 5)
#define PTE_DIRTY               (1 << 6)
#define PTE_LARGE_PAGE          (1 << 7)

/* 架构特定函数声明 */
void m4k_arch_init(void);
void m4k_arch_detect_features(void);
void m4k_arch_setup_paging(void);

/* 内联汇编辅助函数 */
static inline void m4k_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

static inline void m4k_enable_interrupts(void) {
    __asm__ volatile ("sti");
}

static inline void m4k_disable_interrupts(void) {
    __asm__ volatile ("cli");
}

static inline void m4k_halt(void) {
    __asm__ volatile ("hlt");
}

static inline void m4k_pause(void) {
    __asm__ volatile ("pause");
}

static inline uint32_t m4k_read_cr0(void) {
    uint32_t value;
    __asm__ volatile ("movl %%cr0, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr0(uint32_t value) {
    __asm__ volatile ("movl %0, %%cr0" : : "r"(value));
}

static inline uint32_t m4k_read_cr3(void) {
    uint32_t value;
    __asm__ volatile ("movl %%cr3, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr3(uint32_t value) {
    __asm__ volatile ("movl %0, %%cr3" : : "r"(value));
}

static inline uint32_t m4k_read_cr4(void) {
    uint32_t value;
    __asm__ volatile ("movl %%cr4, %0" : "=r"(value));
    return value;
}

static inline void m4k_write_cr4(uint32_t value) {
    __asm__ volatile ("movl %0, %%cr4" : : "r"(value));
}

/* 原子操作 */
static inline uint32_t m4k_atomic_exchange(uint32_t *ptr, uint32_t value) {
    __asm__ volatile (
        "xchgl %0, %1"
        : "+r"(value), "+m"(*ptr)
        : : "memory"
    );
    return value;
}

static inline uint32_t m4k_atomic_compare_exchange(uint32_t *ptr, uint32_t old_val, uint32_t new_val) {
    uint32_t result;
    __asm__ volatile (
        "lock cmpxchgl %2, %1"
        : "=a"(result), "+m"(*ptr)
        : "r"(new_val), "0"(old_val)
        : "memory"
    );
    return result;
}

static inline uint32_t m4k_atomic_add(uint32_t *ptr, uint32_t value) {
    __asm__ volatile (
        "lock xaddl %0, %1"
        : "+r"(value), "+m"(*ptr)
        : : "memory"
    );
    return value;
}

static inline uint32_t m4k_atomic_increment(uint32_t *ptr) {
    return m4k_atomic_add(ptr, 1);
}

static inline uint32_t m4k_atomic_decrement(uint32_t *ptr) {
    return m4k_atomic_add(ptr, -1);
}

/* 内存屏障 */
static inline void m4k_memory_barrier(void) {
    __asm__ volatile ("" : : : "memory");
}

static inline void m4k_read_barrier(void) {
    __asm__ volatile ("" : : : "memory");
}

static inline void m4k_write_barrier(void) {
    __asm__ volatile ("" : : : "memory");
}

#endif /* _M4K_ARCH_X86_H */