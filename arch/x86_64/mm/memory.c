/*
 * M4KK1 4P1 - memory.c
 * Description: x86_64 memory management with 4-level paging.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../../include/m4k_arch.h"
#include "../../../include/memory.h"
#include "../../../include/console.h"
#include "../../../include/string.h"

typedef uint64_t pml4_t;
typedef uint64_t pdp_t;
typedef uint64_t pd_t;
typedef uint64_t pt_t;

#define PTE_PRESENT         (1ULL << 0)
#define PTE_WRITE           (1ULL << 1)
#define PTE_USER            (1ULL << 2)
#define PTE_PWT             (1ULL << 3)
#define PTE_PCD             (1ULL << 4)
#define PTE_ACCESSED        (1ULL << 5)
#define PTE_DIRTY           (1ULL << 6)
#define PTE_HUGE            (1ULL << 7)
#define PTE_GLOBAL          (1ULL << 8)
#define PTE_NX              (1ULL << 63)

static pml4_t *kernel_pml4 = NULL;
static uint64_t *physical_map = NULL;

#define PHYSICAL_MEMORY_BASE    0x100000
#define PHYSICAL_MEMORY_SIZE    0x40000000
#define PAGE_FRAME_COUNT        (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)

static uint8_t *page_frames = NULL;
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

/**
 * mkrn_memory_init - Initialize x86_64 memory management
 * @total_memory: Total physical memory size in bytes
 *
 * Set up the kernel PML4 page table, initialize the physical
 * memory bitmap, mark reserved pages, and load the new page
 * table into CR3.
 */
void mkrn_memory_init(uint64_t total_memory)
{
    uint64_t i;

    kernel_pml4 = (pml4_t *)PML4_BASE;
    mkrn_memset(kernel_pml4, 0, PAGE_SIZE);

    for (i = 0; i < 512; i++) {
        uint64_t entry = (i * 0x80000000ULL)
                         | PTE_PRESENT | PTE_WRITE | PTE_GLOBAL;
        kernel_pml4[i] = entry;
    }

    total_pages = total_memory / PAGE_SIZE;
    page_frames = (uint8_t *)PAGE_FRAMES_BASE;
    mkrn_memset(page_frames, 0, (total_pages + 7) / 8);
    free_pages = total_pages;

    for (i = 0; i < PHYSICAL_MEMORY_BASE / PAGE_SIZE; i++) {
        page_frames[i / 8] |= (1 << (i % 8));
        free_pages--;
    }

    __asm__ volatile ("movq %0, %%cr3" : : "r"(kernel_pml4));

    mkrn_console_write("M4KK1 x86_64 memory management initialized\n");
    mkrn_console_write("Total memory: ");
    mkrn_console_write_hex(total_memory / 1024 / 1024);
    mkrn_console_write(" MB\n");
    mkrn_console_write("Free memory: ");
    mkrn_console_write_hex((free_pages * PAGE_SIZE) / 1024 / 1024);
    mkrn_console_write(" MB\n");
}

/**
 * mkrn_alloc_physical_page - Allocate a single physical page
 *
 * Search the page frame bitmap for a free page and mark it used.
 *
 * Return: Physical address of the allocated page, or 0 on failure
 */
uint64_t mkrn_alloc_physical_page(void)
{
    uint64_t i, j;

    for (i = 0; i < total_pages / 8; i++) {
        if (page_frames[i] != 0xFF) {
            for (j = 0; j < 8; j++) {
                if (!(page_frames[i] & (1 << j))) {
                    page_frames[i] |= (1 << j);
                    free_pages--;
                    return (i * 8 + j) * PAGE_SIZE;
                }
            }
        }
    }

    return 0;
}

/**
 * mkrn_free_physical_page - Free a physical page
 * @address: Physical address of the page to free
 *
 * Clear the corresponding bit in the page frame bitmap.
 */
void mkrn_free_physical_page(uint64_t address)
{
    uint64_t page_index = address / PAGE_SIZE;

    if (page_index < total_pages) {
        uint64_t byte_index = page_index / 8;
        uint64_t bit_index = page_index % 8;

        page_frames[byte_index] &= ~(1 << bit_index);
        free_pages++;
    }
}

/**
 * mkrn_map_page - Map a virtual address to a physical address
 * @virtual_addr: Virtual address to map
 * @physical_addr: Target physical address
 * @flags: Page table flags (PTE_*)
 *
 * Walk the 4-level page table, allocating intermediate tables
 * as needed, and set the final PTE.
 */
void mkrn_map_page(uint64_t virtual_addr, uint64_t physical_addr,
                   uint64_t flags)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) {
        uint64_t pdp_addr = mkrn_alloc_physical_page();
        if (!pdp_addr) return;

        pml4[pml4_index] = pdp_addr
                           | PTE_PRESENT | PTE_WRITE | PTE_USER;
        pdp = (pdp_t *)pdp_addr;
        mkrn_memset(pdp, 0, PAGE_SIZE);
    } else {
        pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                        + MEM_BASE);
    }

    if (!(pdp[pdp_index] & PTE_PRESENT)) {
        uint64_t pd_addr = mkrn_alloc_physical_page();
        if (!pd_addr) return;

        pdp[pdp_index] = pd_addr
                         | PTE_PRESENT | PTE_WRITE | PTE_USER;
        pd = (pd_t *)pd_addr;
        mkrn_memset(pd, 0, PAGE_SIZE);
    } else {
        pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                      + MEM_BASE);
    }

    if (!(pd[pd_index] & PTE_PRESENT)) {
        uint64_t pt_addr = mkrn_alloc_physical_page();
        if (!pt_addr) return;

        pd[pd_index] = pt_addr
                       | PTE_PRESENT | PTE_WRITE | PTE_USER;
        pt = (pt_t *)pt_addr;
        mkrn_memset(pt, 0, PAGE_SIZE);
    } else {
        pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                      + MEM_BASE);
    }

    pt[pt_index] = physical_addr | flags | PTE_PRESENT;

    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr));
}

/**
 * mkrn_unmap_page - Unmap a virtual address
 * @virtual_addr: Virtual address to unmap
 *
 * Free the physical page and clear the PTE entry, then
 * invalidate the TLB for this address.
 */
void mkrn_unmap_page(uint64_t virtual_addr)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) return;
    pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                    + MEM_BASE);

    if (!(pdp[pdp_index] & PTE_PRESENT)) return;
    pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pd[pd_index] & PTE_PRESENT)) return;
    pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    uint64_t physical_addr = pt[pt_index] & 0xFFFFFFFFFF000;
    mkrn_free_physical_page(physical_addr);

    pt[pt_index] = 0;

    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr));
}

/**
 * mkrn_get_physical_address - Translate virtual to physical address
 * @virtual_addr: Virtual address to translate
 *
 * Walk the page tables to find the physical address mapping.
 *
 * Return: Physical address, or 0 if not mapped
 */
uint64_t mkrn_get_physical_address(uint64_t virtual_addr)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) return 0;
    pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                    + MEM_BASE);

    if (!(pdp[pdp_index] & PTE_PRESENT)) return 0;
    pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pd[pd_index] & PTE_PRESENT)) return 0;
    pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pt[pt_index] & PTE_PRESENT)) return 0;

    uint64_t offset = virtual_addr & 0xFFF;
    uint64_t physical_base = pt[pt_index] & 0xFFFFFFFFFF000;

    return physical_base + offset;
}

/**
 * mkrn_get_memory_stats - Get memory usage statistics
 * @total: Output pointer for total memory in bytes
 * @free: Output pointer for free memory in bytes
 * @used: Output pointer for used memory in bytes
 */
void mkrn_get_memory_stats(uint64_t *total, uint64_t *free,
                           uint64_t *used)
{
    if (total) *total = total_pages * PAGE_SIZE;
    if (free) *free = free_pages * PAGE_SIZE;
    if (used) *used = (total_pages - free_pages) * PAGE_SIZE;
}

/**
 * mkrn_copy_page_tables - Copy kernel page table entries
 * @dest_pml4: Destination PML4 table
 * @src_pml4: Source PML4 table
 *
 * Copy kernel-space mappings (entries 256-511) from source
 * to destination page table.
 */
void mkrn_copy_page_tables(pml4_t *dest_pml4, pml4_t *src_pml4)
{
    uint64_t i;

    for (i = 256; i < 512; i++) {
        dest_pml4[i] = src_pml4[i];
    }
}

/**
 * mkrn_switch_address_space - Switch to a different address space
 * @new_pml4: PML4 table of the target address space
 *
 * Load the new page table base into CR3.
 */
void mkrn_switch_address_space(pml4_t *new_pml4)
{
    __asm__ volatile ("movq %0, %%cr3" : : "r"(new_pml4));
}

/**
 * mkrn_flush_tlb - Flush the entire TLB
 *
 * Reload CR3 to invalidate all cached translations.
 */
void mkrn_flush_tlb(void)
{
    __asm__ volatile (
        "movq %%cr3, %%rax; movq %%rax, %%cr3" : : : "rax");
}

/**
 * mkrn_flush_tlb_entry - Flush TLB for a single address
 * @address: Virtual address to invalidate
 */
void mkrn_flush_tlb_entry(uint64_t address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(address));
}

/**
 * mkrn_get_memory_size - Get detected physical memory size
 *
 * Query BIOS or hardware for total physical memory.
 * Currently returns a fixed 1 GB value.
 *
 * Return: Physical memory size in bytes
 */
uint64_t mkrn_get_memory_size(void)
{
    return 0x40000000;
}

/**
 * mkrn_alloc_contiguous_pages - Allocate contiguous physical pages
 * @count: Number of pages to allocate
 *
 * Search the page frame bitmap for a run of free pages.
 *
 * Return: Physical address of the first page, or 0 on failure
 */
uint64_t mkrn_alloc_contiguous_pages(uint32_t count)
{
    uint64_t start_page = 0;
    uint64_t consecutive = 0;
    uint64_t i;

    for (i = 0; i < total_pages; i++) {
        uint64_t byte_index = i / 8;
        uint64_t bit_index = i % 8;

        if (!(page_frames[byte_index] & (1 << bit_index))) {
            if (consecutive == 0) {
                start_page = i;
            }
            consecutive++;

            if (consecutive == count) {
                uint64_t j;
                for (j = start_page; j < start_page + count;
                     j++) {
                    uint64_t b_idx = j / 8;
                    uint64_t b_bit = j % 8;
                    page_frames[b_idx] |= (1 << b_bit);
                }
                free_pages -= count;
                return start_page * PAGE_SIZE;
            }
        } else {
            consecutive = 0;
        }
    }

    return 0;
}

/**
 * mkrn_free_contiguous_pages - Free contiguous physical pages
 * @address: Physical address of the first page
 * @count: Number of pages to free
 */
void mkrn_free_contiguous_pages(uint64_t address, uint32_t count)
{
    uint64_t start_page = address / PAGE_SIZE;
    uint64_t i;

    for (i = start_page; i < start_page + count; i++) {
        uint64_t byte_index = i / 8;
        uint64_t bit_index = i % 8;
        page_frames[byte_index] &= ~(1 << bit_index);
    }

    free_pages += count;
}

/**
 * mkrn_is_virtual_address_valid - Check if a virtual address is mapped
 * @virtual_addr: Virtual address to check
 *
 * Return: true if the address has a valid mapping, false otherwise
 */
bool mkrn_is_virtual_address_valid(uint64_t virtual_addr)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) return false;
    pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                    + MEM_BASE);

    if (!(pdp[pdp_index] & PTE_PRESENT)) return false;
    pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pd[pd_index] & PTE_PRESENT)) return false;
    pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    return (pt[pt_index] & PTE_PRESENT) != 0;
}

/**
 * mkrn_get_page_flags - Get page table flags for a virtual address
 * @virtual_addr: Virtual address to query
 *
 * Return: Page table entry flags, or 0 if not mapped
 */
uint64_t mkrn_get_page_flags(uint64_t virtual_addr)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) return 0;
    pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                    + MEM_BASE);

    if (!(pdp[pdp_index] & PTE_PRESENT)) return 0;
    pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pd[pd_index] & PTE_PRESENT)) return 0;
    pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    return pt[pt_index] & 0xFFFFFFFFFF000FFF;
}

/**
 * mkrn_set_page_flags - Set page table flags for a virtual address
 * @virtual_addr: Virtual address to modify
 * @flags: New PTE flags
 *
 * Update the flags and invalidate the TLB entry.
 */
void mkrn_set_page_flags(uint64_t virtual_addr, uint64_t flags)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    pml4_t *pml4 = kernel_pml4;
    pdp_t *pdp;
    pd_t *pd;
    pt_t *pt;

    if (!(pml4[pml4_index] & PTE_PRESENT)) return;
    pdp = (pdp_t *)((pml4[pml4_index] & 0xFFFFFFFFFF000)
                    + MEM_BASE);

    if (!(pdp[pdp_index] & PTE_PRESENT)) return;
    pd = (pd_t *)((pdp[pdp_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    if (!(pd[pd_index] & PTE_PRESENT)) return;
    pt = (pt_t *)((pd[pd_index] & 0xFFFFFFFFFF000)
                  + MEM_BASE);

    uint64_t physical_addr = pt[pt_index] & 0xFFFFFFFFFF000;
    pt[pt_index] = physical_addr | flags | PTE_PRESENT;

    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr));
}

/**
 * mkrn_copy_page - Copy one page of memory
 * @dest: Destination physical/virtual address
 * @src: Source physical/virtual address
 *
 * Copy PAGE_SIZE bytes using 64-bit word moves.
 */
void mkrn_copy_page(uint64_t dest, uint64_t src)
{
    uint64_t *dest_ptr = (uint64_t *)dest;
    uint64_t *src_ptr = (uint64_t *)src;
    uint64_t i;

    for (i = 0; i < PAGE_SIZE / 8; i++) {
        dest_ptr[i] = src_ptr[i];
    }
}

/**
 * mkrn_zero_page - Zero out a page
 * @address: Address of the page to zero
 */
void mkrn_zero_page(uint64_t address)
{
    mkrn_memset((void *)address, 0, PAGE_SIZE);
}

/**
 * mkrn_compare_pages - Compare two pages for equality
 * @page1: Address of first page
 * @page2: Address of second page
 *
 * Return: true if pages are identical, false otherwise
 */
bool mkrn_compare_pages(uint64_t page1, uint64_t page2)
{
    uint64_t *ptr1 = (uint64_t *)page1;
    uint64_t *ptr2 = (uint64_t *)page2;
    uint64_t i;

    for (i = 0; i < PAGE_SIZE / 8; i++) {
        if (ptr1[i] != ptr2[i]) {
            return false;
        }
    }

    return true;
}

/**
 * mkrn_get_page_refcount - Get page reference count
 * @virtual_addr: Virtual address of the page
 *
 * Return: Current reference count (stub, returns 1)
 */
uint32_t mkrn_get_page_refcount(uint64_t virtual_addr)
{
    return 1;
}

/**
 * mkrn_inc_page_refcount - Increment page reference count
 * @virtual_addr: Virtual address of the page
 */
void mkrn_inc_page_refcount(uint64_t virtual_addr)
{
}

/**
 * mkrn_dec_page_refcount - Decrement page reference count
 * @virtual_addr: Virtual address of the page
 */
void mkrn_dec_page_refcount(uint64_t virtual_addr)
{
}

/**
 * mkrn_lock_page - Lock a page in memory
 * @virtual_addr: Virtual address of the page
 */
void mkrn_lock_page(uint64_t virtual_addr)
{
}

/**
 * mkrn_unlock_page - Unlock a previously locked page
 * @virtual_addr: Virtual address of the page
 */
void mkrn_unlock_page(uint64_t virtual_addr)
{
}

/**
 * mkrn_is_page_locked - Check if a page is locked
 * @virtual_addr: Virtual address to check
 *
 * Return: true if locked, false otherwise (stub returns false)
 */
bool mkrn_is_page_locked(uint64_t virtual_addr)
{
    return false;
}

/**
 * mkrn_get_page_mtime - Get page modification time
 * @virtual_addr: Virtual address of the page
 *
 * Return: Modification timestamp (stub, returns 0)
 */
uint64_t mkrn_get_page_mtime(uint64_t virtual_addr)
{
    return 0;
}

/**
 * mkrn_set_page_mtime - Set page modification time
 * @virtual_addr: Virtual address of the page
 * @mtime: Modification timestamp
 */
void mkrn_set_page_mtime(uint64_t virtual_addr, uint64_t mtime)
{
}

/**
 * mkrn_prefault_page - Pre-fault a page into the TLB
 * @virtual_addr: Virtual address to pre-fault
 */
void mkrn_prefault_page(uint64_t virtual_addr)
{
}

/**
 * mkrn_flush_page_cache - Flush cache for a page
 * @virtual_addr: Virtual address to flush
 */
void mkrn_flush_page_cache(uint64_t virtual_addr)
{
}

/**
 * mkrn_get_page_cache_state - Get page cache state
 * @virtual_addr: Virtual address to query
 *
 * Return: Cache state flags (stub, returns 0)
 */
uint32_t mkrn_get_page_cache_state(uint64_t virtual_addr)
{
    return 0;
}

/**
 * mkrn_set_page_cache_policy - Set page cache policy
 * @virtual_addr: Virtual address to configure
 * @policy: Cache policy value
 */
void mkrn_set_page_cache_policy(uint64_t virtual_addr,
                                uint32_t policy)
{
}

/**
 * mkrn_memory_statistics - Print memory usage statistics
 *
 * Display total, free, and used memory with percentage usage.
 */
void mkrn_memory_statistics(void)
{
    uint64_t total, free, used;

    mkrn_get_memory_stats(&total, &free, &used);

    mkrn_console_write("=== M4KK1 x86_64 Memory Statistics ===\n");
    mkrn_console_write("Total memory: ");
    mkrn_console_write_hex(total / 1024 / 1024);
    mkrn_console_write(" MB\n");
    mkrn_console_write("Free memory: ");
    mkrn_console_write_hex(free / 1024 / 1024);
    mkrn_console_write(" MB\n");
    mkrn_console_write("Used memory: ");
    mkrn_console_write_hex(used / 1024 / 1024);
    mkrn_console_write(" MB\n");
    mkrn_console_write("Usage: ");
    mkrn_console_write_dec((used * 100) / total);
    mkrn_console_write("%\n");
    mkrn_console_write("=====================================\n");
}

/**
 * mkrn_dump_page_table - Dump page table entries for an address
 * @virtual_addr: Virtual address to dump
 *
 * Display the PML4 and PDP entries for debugging.
 */
void mkrn_dump_page_table(uint64_t virtual_addr)
{
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdp_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;

    mkrn_console_write("Page table dump for address 0x");
    mkrn_console_write_hex(virtual_addr);
    mkrn_console_write("\n");
    mkrn_console_write("PML4[");
    mkrn_console_write_hex(pml4_index);
    mkrn_console_write("] = 0x");
    mkrn_console_write_hex(kernel_pml4[pml4_index]);
    mkrn_console_write("\n");

    if (kernel_pml4[pml4_index] & PTE_PRESENT) {
        pml4_t *pdp = (pml4_t *)(
            (kernel_pml4[pml4_index] & 0xFFFFFFFFFF000)
            + MEM_BASE);
        mkrn_console_write("PDP[");
        mkrn_console_write_hex(pdp_index);
        mkrn_console_write("] = 0x");
        mkrn_console_write_hex(pdp[pdp_index]);
        mkrn_console_write("\n");
    }

    mkrn_console_write("=====================================\n");
}

/**
 * mkrn_arch_memory_init - Initialize x86_64 arch memory subsystem
 *
 * Query total memory size and initialize the page frame allocator
 * and page table hierarchy.
 */
void mkrn_arch_memory_init(void)
{
    uint64_t total_memory = mkrn_get_memory_size();
    mkrn_memory_init(total_memory);

    mkrn_console_write(
        "M4KK1 x86_64 memory management initialized\n");
}
