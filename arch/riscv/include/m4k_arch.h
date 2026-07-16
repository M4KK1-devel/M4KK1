/**
 * M4KK1 RISC-V Architecture Definitions
 * RISC-V架构特定定义和接口
 *
 * 文件: m4k_arch.h
 * 作者: M4KK1 Development Team
 * 版本: v0.2.0-multarch
 * 日期: 2025-10-16
 *
 * 描述:
 *   定义RISC-V架构特定的常量、宏和接口函数
 *   提供与硬件抽象层的接口
 *
 * 架构特性:
 *   - 64位RISC-V架构
 *   - RV64GC指令集
 *   - 特权级架构
 *   - 多处理器支持
 *
 * 内存布局:
 *   - 内核基地址: 0xC0000000
 *   - 用户空间: 0x00000000 - 0xBFFFFFFF
 *   - 内核空间: 0xC0000000 - 0xFFFFFFFF
 *
 * 中断处理:
 *   - 使用RISC-V PLIC中断控制器
 *   - 0x4D: M4KK1独特系统调用
 *   - 特权级别M/S/U
 *
 * 修改历史:
 *   v0.2.0: 实现RISC-V架构支持
 *   v0.1.0: 初始架构定义
 */

#ifndef __M4K_RISCV_ARCH_H__
#define __M4K_RISCV_ARCH_H__

#include <stdint.h>

/* 架构标识 */
#define M4K_ARCH_RISCV          1
#define M4K_ARCH_NAME           "riscv"
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
#define M4K_INT_TIMER           0x05    /* RISC-V定时器 */
#define M4K_INT_KEYBOARD        0x0A    /* 键盘中断 */
#define M4K_INT_MOUSE           0x0B    /* 鼠标中断 */

/* 特权级别 */
#define M4K_PRIV_M              3       /* 机器模式 */
#define M4K_PRIV_S              1       /* 监督模式 */
#define M4K_PRIV_U              0       /* 用户模式 */

/* CSR寄存器 */
#define M4K_CSR_MSTATUS         0x300
#define M4K_CSR_MIE             0x304
#define M4K_CSR_MTVEC           0x305
#define M4K_CSR_MSCRATCH        0x340
#define M4K_CSR_MEPC            0x341
#define M4K_CSR_MCAUSE          0x342
#define M4K_CSR_MTVAL           0x343
#define M4K_CSR_MIP             0x344
#define M4K_CSR_SATP            0x180
#define M4K_CSR_SSTATUS         0x100
#define M4K_CSR_SIE             0x104
#define M4K_CSR_STVEC           0x105
#define M4K_CSR_SSCRATCH        0x140
#define M4K_CSR_SEPC            0x141
#define M4K_CSR_SCAUSE          0x142
#define M4K_CSR_STVAL           0x143
#define M4K_CSR_SIP             0x144

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
    uint64_t misa;          /* ISA寄存器 */
    uint64_t mvendorid;     /* 供应商ID */
    uint64_t marchid;       /* 架构ID */
    uint64_t mimpid;        /* 实现ID */
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
static inline uint64_t m4k_read_csr(uint32_t csr) {
    uint64_t value;
    __asm__ volatile ("csrr %0, %1" : "=r"(value) : "i"(csr));
    return value;
}

static inline void m4k_write_csr(uint32_t csr, uint64_t value) {
    __asm__ volatile ("csrw %0, %1" : : "i"(csr), "r"(value));
}

static inline uint64_t m4k_read_mstatus(void) {
    return m4k_read_csr(M4K_CSR_MSTATUS);
}

static inline void m4k_write_mstatus(uint64_t value) {
    m4k_write_csr(M4K_CSR_MSTATUS, value);
}

static inline uint64_t m4k_read_mie(void) {
    return m4k_read_csr(M4K_CSR_MIE);
}

static inline void m4k_write_mie(uint64_t value) {
    m4k_write_csr(M4K_CSR_MIE, value);
}

static inline uint64_t m4k_read_mtvec(void) {
    return m4k_read_csr(M4K_CSR_MTVEC);
}

static inline void m4k_write_mtvec(uint64_t value) {
    m4k_write_csr(M4K_CSR_MTVEC, value);
}

static inline void m4k_enable_interrupts(void) {
    __asm__ volatile ("csrsi mstatus, 0x8" : : : "memory");
}

static inline void m4k_disable_interrupts(void) {
    __asm__ volatile ("csrci mstatus, 0x8" : : : "memory");
}

static inline void m4k_invalidate_tlb(void) {
    __asm__ volatile ("sfence.vma" : : : "memory");
}

static inline void m4k_halt(void) {
    __asm__ volatile ("wfi" : : : "memory");
}

static inline void m4k_pause(void) {
    __asm__ volatile ("nop" : : : "memory");
}

#endif /* __M4K_RISCV_ARCH_H__ */