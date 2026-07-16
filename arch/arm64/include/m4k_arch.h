/**
 * M4KK1 ARM64 Architecture Definitions
 * ARM64架构特定定义和接口
 *
 * 文件: m4k_arch.h
 * 作者: M4KK1 Development Team
 * 版本: v0.2.0-multarch
 * 日期: 2025-10-16
 *
 * 描述:
 *   定义ARM64架构特定的常量、宏和接口函数
 *   提供与硬件抽象层的接口
 *
 * 架构特性:
 *   - 64位ARMv8-A架构
 *   - AArch64指令集
 *   - 4级页表结构
 *   - 多处理器支持
 *
 * 内存布局:
 *   - 内核基地址: 0xC0000000
 *   - 用户空间: 0x00000000 - 0xBFFFFFFF
 *   - 内核空间: 0xC0000000 - 0xFFFFFFFF
 *
 * 中断处理:
 *   - 使用ARM GIC中断控制器
 *   - 0x4D: M4KK1独特系统调用
 *   - 异常级别EL1-EL3
 *
 * 修改历史:
 *   v0.2.0: 实现ARM64架构支持
 *   v0.1.0: 初始架构定义
 */

#ifndef __M4K_ARM64_ARCH_H__
#define __M4K_ARM64_ARCH_H__

#include <stdint.h>

/* 架构标识 */
#define M4K_ARCH_ARM64          1
#define M4K_ARCH_NAME           "arm64"
#define M4K_ARCH_BITS           64

/* 内存布局 */
#define M4K_KERNEL_BASE         0xC0000000ULL
#define M4K_KERNEL_HEAP         0xC0400000ULL
#define M4K_KERNEL_STACK        0xC07FE000ULL
#define M4K_USER_BASE           0x00000000ULL
#define M4K_USER_LIMIT          0xBFFFFFFFULL

/* 页表结构 */
#define M4K_PAGE_SIZE           4096
#define M4K_PAGE_MASK           (~(M4K_PAGE_SIZE - 1))
#define M4K_PAGE_SHIFT          12

/* 中断向量 */
#define M4K_INT_SYSCALL         0x4D    /* M4KK1独特系统调用 */
#define M4K_INT_TIMER           0x1B    /* ARM通用定时器 */
#define M4K_INT_KEYBOARD        0x3F    /* 键盘中断 */
#define M4K_INT_MOUSE           0x40    /* 鼠标中断 */

/* 异常级别 */
#define M4K_EL3                 3       /* 安全监控模式 */
#define M4K_EL2                 2       /* 虚拟化主机 */
#define M4K_EL1                 1       /* 内核模式 */
#define M4K_EL0                 0       /* 用户模式 */

/* 系统寄存器 */
#define M4K_SCTLR_EL1           0xC0000000
#define M4K_TTBR0_EL1           0xC0000001
#define M4K_TTBR1_EL1           0xC0000002
#define M4K_TCR_EL1             0xC0000003
#define M4K_MAIR_EL1            0xC0000004
#define M4K_TPIDR_EL1           0xC0000005

/* 架构特定函数声明 */
void m4k_arch_init(void);
void m4k_arch_detect_features(void);
uint32_t m4k_arch_get_cpu_count(void);
void m4k_arch_send_ipi(uint32_t cpu, uint32_t vector);

/* 内存管理 */
void m4k_arch_paging_init(void);
void *m4k_arch_map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
void m4k_arch_unmap_page(uint64_t virtual_addr);

/* 中断处理 */
void m4k_arch_interrupt_init(void);
void m4k_arch_register_handler(uint32_t vector, void *handler);
void m4k_arch_enable_interrupts(void);
void m4k_arch_disable_interrupts(void);

/* I/O 操作 */
uint8_t m4k_arch_inb(uint16_t port);
void m4k_arch_outb(uint16_t port, uint8_t value);
uint16_t m4k_arch_inw(uint16_t port);
void m4k_arch_outw(uint16_t port, uint16_t value);
uint32_t m4k_arch_ind(uint16_t port);
void m4k_arch_outd(uint16_t port, uint32_t value);

/* CPU信息 */
typedef struct {
    uint64_t midr_el1;      /* 主ID寄存器 */
    uint64_t mpidr_el1;     /* 多处理器亲和寄存器 */
    uint64_t id_aa64mmfr0_el1; /* 内存模型特性寄存器 */
    uint32_t cpu_count;
    uint32_t core_count;
    uint32_t thread_count;
    char cpu_name[32];
} m4k_cpu_info_t;

extern m4k_cpu_info_t m4k_cpu_info;

/* 内存信息 */
typedef struct {
    uint64_t total_memory;
    uint64_t free_memory;
    uint64_t used_memory;
    uint64_t kernel_memory;
    uint64_t user_memory;
} m4k_memory_info_t;

extern m4k_memory_info_t m4k_memory_info;

/* 架构特定内联函数 */
static inline uint64_t m4k_read_sctlr_el1(void) {
    uint64_t value;
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(value));
    return value;
}

static inline void m4k_write_sctlr_el1(uint64_t value) {
    __asm__ volatile ("msr sctlr_el1, %0" : : "r"(value));
}

static inline uint64_t m4k_read_ttbr0_el1(void) {
    uint64_t value;
    __asm__ volatile ("mrs %0, ttbr0_el1" : "=r"(value));
    return value;
}

static inline void m4k_write_ttbr0_el1(uint64_t value) {
    __asm__ volatile ("msr ttbr0_el1, %0" : : "r"(value));
}

static inline uint64_t m4k_read_ttbr1_el1(void) {
    uint64_t value;
    __asm__ volatile ("mrs %0, ttbr1_el1" : "=r"(value));
    return value;
}

static inline void m4k_write_ttbr1_el1(uint64_t value) {
    __asm__ volatile ("msr ttbr1_el1, %0" : : "r"(value));
}

static inline void m4k_enable_interrupts(void) {
    __asm__ volatile ("msr daifclr, #0x3" : : : "memory");
}

static inline void m4k_disable_interrupts(void) {
    __asm__ volatile ("msr daifset, #0x3" : : : "memory");
}

static inline uint64_t m4k_read_daif(void) {
    uint64_t value;
    __asm__ volatile ("mrs %0, daif" : "=r"(value));
    return value;
}

static inline void m4k_write_daif(uint64_t value) {
    __asm__ volatile ("msr daif, %0" : : "r"(value));
}

static inline void m4k_invalidate_tlb(void) {
    __asm__ volatile ("tlbi vmalle1" : : : "memory");
    __asm__ volatile ("dsb ish" : : : "memory");
    __asm__ volatile ("isb" : : : "memory");
}

static inline void m4k_halt(void) {
    __asm__ volatile ("wfi" : : : "memory");
}

static inline void m4k_pause(void) {
    __asm__ volatile ("yield" : : : "memory");
}

#endif /* __M4K_ARM64_ARCH_H__ */