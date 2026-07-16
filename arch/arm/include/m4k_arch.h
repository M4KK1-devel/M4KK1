/**
 * M4KK1 ARM Architecture Definitions
 * ARM架构特定定义和接口 (32位)
 *
 * 文件: m4k_arch.h
 * 作者: M4KK1 Development Team
 * 版本: v0.2.0-multarch
 * 日期: 2025-10-16
 *
 * 描述:
 *   定义ARM架构特定的常量、宏和接口函数
 *   提供与硬件抽象层的接口
 *
 * 架构特性:
 *   - 32位ARM架构
 *   - ARMv7-A指令集
 *   - 2级页表结构
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
 *   - 异常模式处理
 *
 * 修改历史:
 *   v0.2.0: 实现ARM架构支持
 *   v0.1.0: 初始架构定义
 */

#ifndef __M4K_ARM_ARCH_H__
#define __M4K_ARM_ARCH_H__

#include <stdint.h>

/* 架构标识 */
#define M4K_ARCH_ARM            1
#define M4K_ARCH_NAME           "arm"
#define M4K_ARCH_BITS           32

/* 内存布局 */
#define M4K_KERNEL_BASE         0xC0000000UL
#define M4K_KERNEL_HEAP         0xC0400000UL
#define M4K_KERNEL_STACK        0xC07FE000UL
#define M4K_USER_BASE           0x00000000UL
#define M4K_USER_LIMIT          0xBFFFFFFFUL

/* 页表结构 */
#define M4K_PAGE_SIZE           4096
#define M4K_PAGE_MASK           (~(M4K_PAGE_SIZE - 1))
#define M4K_PAGE_SHIFT          12

/* 中断向量 */
#define M4K_INT_SYSCALL         0x4D    /* M4KK1独特系统调用 */
#define M4K_INT_TIMER           0x1B    /* ARM通用定时器 */
#define M4K_INT_KEYBOARD        0x3F    /* 键盘中断 */
#define M4K_INT_MOUSE           0x40    /* 鼠标中断 */

/* 处理器模式 */
#define M4K_MODE_USR            0x10    /* 用户模式 */
#define M4K_MODE_FIQ            0x11    /* 快速中断模式 */
#define M4K_MODE_IRQ            0x12    /* 普通中断模式 */
#define M4K_MODE_SVC            0x13    /* 管理模式 */
#define M4K_MODE_ABT            0x17    /* 中止模式 */
#define M4K_MODE_UND            0x1B    /* 未定义模式 */
#define M4K_MODE_SYS            0x1F    /* 系统模式 */

/* 协处理器 */
#define M4K_CP15                15      /* 系统控制协处理器 */
#define M4K_CP14                14      /* 调试协处理器 */

/* 系统控制寄存器 */
#define M4K_SCTLR               0       /* 系统控制寄存器 */
#define M4K_ACTLR               1       /* 辅助控制寄存器 */
#define M4K_CPACR               2       /* 协处理器访问控制 */

/* 内存管理寄存器 */
#define M4K_TTBR0               0       /* 转换表基地址0 */
#define M4K_TTBR1               1       /* 转换表基地址1 */
#define M4K_TTBCR               2       /* 转换表基地址控制 */
#define M4K_DACR               3       /* 域访问控制 */
#define M4K_DFSR               5       /* 数据故障状态 */
#define M4K_IFSR               6       /* 指令故障状态 */
#define M4K_DFAR               8       /* 数据故障地址 */
#define M4K_IFAR               9       /* 指令故障地址 */

/* 架构特定函数声明 */
void m4k_arch_init(void);
void m4k_arch_detect_features(void);
uint32_t m4k_arch_get_cpu_count(void);
void m4k_arch_send_ipi(uint32_t cpu, uint32_t vector);

/* 内存管理 */
void m4k_arch_paging_init(void);
void *m4k_arch_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void m4k_arch_unmap_page(uint32_t virtual_addr);

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
    uint32_t midr;          /* 主ID寄存器 */
    uint32_t mpidr;         /* 多处理器亲和寄存器 */
    uint32_t id_pfr0;       /* 处理器特性寄存器0 */
    uint32_t id_pfr1;       /* 处理器特性寄存器1 */
    uint32_t id_dfr0;       /* 调试特性寄存器0 */
    uint32_t cpu_count;
    uint32_t core_count;
    uint32_t thread_count;
    char cpu_name[32];
} m4k_cpu_info_t;

extern m4k_cpu_info_t m4k_cpu_info;

/* 内存信息 */
typedef struct {
    uint32_t total_memory;
    uint32_t free_memory;
    uint32_t used_memory;
    uint32_t kernel_memory;
    uint32_t user_memory;
} m4k_memory_info_t;

extern m4k_memory_info_t m4k_memory_info;

/* 架构特定内联函数 */
static inline uint32_t m4k_read_cp15(uint32_t opcode1, uint32_t crn, uint32_t crm, uint32_t opcode2) {
    uint32_t value;
    __asm__ volatile ("mrc p15, %1, %0, c%2, c%3, %4" : "=r"(value) : "i"(opcode1), "i"(crn), "i"(crm), "i"(opcode2));
    return value;
}

static inline void m4k_write_cp15(uint32_t value, uint32_t opcode1, uint32_t crn, uint32_t crm, uint32_t opcode2) {
    __asm__ volatile ("mcr p15, %1, %0, c%2, c%3, %4" : : "r"(value), "i"(opcode1), "i"(crn), "i"(crm), "i"(opcode2));
}

static inline uint32_t m4k_read_sctlr(void) {
    return m4k_read_cp15(0, M4K_SCTLR, 0, 0);
}

static inline void m4k_write_sctlr(uint32_t value) {
    m4k_write_cp15(value, 0, M4K_SCTLR, 0, 0);
}

static inline uint32_t m4k_read_ttbr0(void) {
    return m4k_read_cp15(0, M4K_TTBR0, 0, 0);
}

static inline void m4k_write_ttbr0(uint32_t value) {
    m4k_write_cp15(value, 0, M4K_TTBR0, 0, 0);
}

static inline void m4k_enable_interrupts(void) {
    __asm__ volatile ("cpsie i" : : : "memory");
}

static inline void m4k_disable_interrupts(void) {
    __asm__ volatile ("cpsid i" : : : "memory");
}

static inline void m4k_invalidate_tlb(void) {
    __asm__ volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r"(0) : "memory");
}

static inline void m4k_halt(void) {
    __asm__ volatile ("wfi" : : : "memory");
}

static inline void m4k_pause(void) {
    __asm__ volatile ("nop" : : : "memory");
}

#endif /* __M4K_ARM_ARCH_H__ */