/**
 * M4KK1 PowerPC Architecture Definitions
 * PowerPC架构特定定义和接口
 *
 * 文件: m4k_arch.h
 * 作者: M4KK1 Development Team
 * 版本: v0.2.0-multarch
 * 日期: 2025-10-16
 *
 * 描述:
 *   定义PowerPC架构特定的常量、宏和接口函数
 *   提供与硬件抽象层的接口
 *
 * 架构特性:
 *   - 64位PowerPC架构
 *   - Power ISA 3.0
 *   - 散列页表
 *   - 多处理器支持
 *
 * 内存布局:
 *   - 内核基地址: 0xC0000000
 *   - 用户空间: 0x00000000 - 0xBFFFFFFF
 *   - 内核空间: 0xC0000000 - 0xFFFFFFFF
 *
 * 中断处理:
 *   - 使用OpenPIC中断控制器
 *   - 0x4D: M4KK1独特系统调用
 *   - 异常向量表
 *
 * 修改历史:
 *   v0.2.0: 实现PowerPC架构支持
 *   v0.1.0: 初始架构定义
 */

#ifndef __M4K_POWERPC_ARCH_H__
#define __M4K_POWERPC_ARCH_H__

#include <stdint.h>

/* 架构标识 */
#define M4K_ARCH_POWERPC        1
#define M4K_ARCH_NAME           "powerpc"
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
#define M4K_INT_TIMER           0x00    /* 递减器中断 */
#define M4K_INT_KEYBOARD        0x01    /* 键盘中断 */
#define M4K_INT_MOUSE           0x02    /* 鼠标中断 */

/* 特殊寄存器 */
#define M4K_SPR_XER             1
#define M4K_SPR_LR              8
#define M4K_SPR_CTR             9
#define M4K_SPR_DSISR           18
#define M4K_SPR_DAR             19
#define M4K_SPR_DEC             22
#define M4K_SPR_SDR1            25
#define M4K_SPR_SRR0            26
#define M4K_SPR_SRR1            27
#define M4K_SPR_HID0            1008
#define M4K_SPR_HID1            1009

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
    uint32_t pvr;           /* 处理器版本寄存器 */
    uint32_t pir;           /* 处理器ID寄存器 */
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
static inline uint32_t m4k_read_spr(uint32_t spr) {
    uint32_t value;
    __asm__ volatile ("mfspr %0, %1" : "=r"(value) : "i"(spr));
    return value;
}

static inline void m4k_write_spr(uint32_t spr, uint32_t value) {
    __asm__ volatile ("mtspr %0, %1" : : "i"(spr), "r"(value));
}

static inline uint32_t m4k_read_pvr(void) {
    return m4k_read_spr(M4K_SPR_PVR);
}

static inline uint32_t m4k_read_pir(void) {
    return m4k_read_spr(M4K_SPR_PIR);
}

static inline void m4k_enable_interrupts(void) {
    __asm__ volatile ("mfmsr %r3; ori %r3, %r3, 0x8000; mtmsr %r3" : : : "r3");
}

static inline void m4k_disable_interrupts(void) {
    __asm__ volatile ("mfmsr %r3; rlwinm %r3, %r3, 0, 17, 15; mtmsr %r3" : : : "r3");
}

static inline void m4k_invalidate_tlb(void) {
    __asm__ volatile ("tlbie %r0" : : : "memory");
    __asm__ volatile ("eieio" : : : "memory");
    __asm__ volatile ("tlbsync" : : : "memory");
}

static inline void m4k_halt(void) {
    __asm__ volatile ("stop" : : : "memory");
}

static inline void m4k_pause(void) {
    __asm__ volatile ("yield" : : : "memory");
}

#endif /* __M4K_POWERPC_ARCH_H__ */