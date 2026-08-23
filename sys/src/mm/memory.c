/*
 * M4KK1 4P1 - memory.c
 * Description: Memory management — buddy page allocator +
 *              kernel heap.  Rewritten on the classic binary
 *              buddy algorithm (free blocks per order, XOR buddy
 *              location, upward merge on free, expand-split on
 *              alloc), replacing the flat page bitmap.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <memory.h>
#include <console.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_MEMORY_REGIONS 64
static mkrn_mem_region_t
    memory_region_pool[MAX_MEMORY_REGIONS];
static uint32_t u32MemoryRegionCount = 0;
static mkrn_mem_region_t *pMemoryRegions = NULL;
static uint32_t u32TotalMemory = 0;
static uint32_t u32FreeMemory = 0;
static uint32_t u32UsedMemory = 0;

extern uint32_t __heap_start;
extern uint32_t __heap_end;

static uint32_t u32KernelHeapStart = 0;
static uint32_t u32KernelHeapEnd = 0;
static mkrn_mem_block_t *pKernelHeapBlocks = NULL;

/* ── Buddy page allocator ──────────────────────────────────── */

#define M4K_BUDDY_MAX_ORDER 11          /* orders 0..10: 4KB..4MB */
#define M4K_PFN_NONE        0xFFFFFFFFu
#define M4K_MAX_PFNS        (512u * 1024u * 1024u / 4096u) /* 512MB */

/* Free-list node embedded in the freed page itself */
struct mkrn_free_page {
    uint32_t next;                      /* pfn of next block */
    uint32_t prev;                      /* pfn of prev block */
};

struct mkrn_buddy_zone {
    uint32_t free_head[M4K_BUDDY_MAX_ORDER];
    uint32_t free_count[M4K_BUDDY_MAX_ORDER];
    uint32_t base_pfn;
    uint32_t nr_pages;
    uint32_t nr_free;                   /* in order-0 pages */
    uint8_t  *order_map;                /* per-pfn: order of free
                                           block headed here,
                                           0xFF = not free head */
};

static struct mkrn_buddy_zone buddy_zone;
static uint8_t buddy_order_map[M4K_MAX_PFNS]
    __attribute__((aligned(4096)));

static void *pfn_to_vaddr(uint32_t pfn)
{
    return (void *)(uintptr_t)(pfn << 12);
}

static uint32_t buddy_of(uint32_t pfn, uint32_t order)
{
    return pfn ^ (1u << order);
}

static void buddy_list_add(struct mkrn_buddy_zone *z,
                           uint32_t order, uint32_t pfn)
{
    struct mkrn_free_page *p = pfn_to_vaddr(pfn);
    uint32_t head = z->free_head[order];

    p->prev = M4K_PFN_NONE;
    p->next = head;
    if (head != M4K_PFN_NONE)
        ((struct mkrn_free_page *)
             pfn_to_vaddr(head))->prev = pfn;
    z->free_head[order] = pfn;
    z->free_count[order]++;
    z->order_map[pfn - z->base_pfn] = (uint8_t)order;
}

static void buddy_list_del(struct mkrn_buddy_zone *z,
                           uint32_t order, uint32_t pfn)
{
    struct mkrn_free_page *p = pfn_to_vaddr(pfn);

    if (p->prev != M4K_PFN_NONE)
        ((struct mkrn_free_page *)
             pfn_to_vaddr(p->prev))->next = p->next;
    else
        z->free_head[order] = p->next;
    if (p->next != M4K_PFN_NONE)
        ((struct mkrn_free_page *)
             pfn_to_vaddr(p->next))->prev = p->prev;
    z->free_count[order]--;
    z->order_map[pfn - z->base_pfn] = 0xFF;
}

/* Free one aligned block with upward merging */
static void buddy_free_block(struct mkrn_buddy_zone *z,
                             uint32_t order, uint32_t pfn)
{
    while (order < M4K_BUDDY_MAX_ORDER - 1) {
        uint32_t bpfn = buddy_of(pfn, order);

        if (bpfn < z->base_pfn ||
            bpfn >= z->base_pfn + z->nr_pages)
            break;

        if (z->order_map[bpfn - z->base_pfn] != (uint8_t)order)
            break;                      /* busy or different order */

        buddy_list_del(z, order, bpfn);
        z->nr_free -= 1u << order;
        if (bpfn < pfn)
            pfn = bpfn;
        order++;
    }

    z->nr_free += 1u << order;
    buddy_list_add(z, order, pfn);
}

/* Free a pfn range as maximal aligned blocks, largest first */
static void buddy_free_range(struct mkrn_buddy_zone *z,
                             uint32_t first_pfn, uint32_t nr)
{
    uint32_t end = first_pfn + nr;

    if (first_pfn < z->base_pfn)
        first_pfn = z->base_pfn;
    if (end > z->base_pfn + z->nr_pages)
        end = z->base_pfn + z->nr_pages;

    for (int order = M4K_BUDDY_MAX_ORDER - 1; order >= 0; order--) {
        uint32_t size = 1u << order;
        while (end - first_pfn >= size &&
               (first_pfn & (size - 1u)) == 0) {
            buddy_free_block(z, (uint32_t)order, first_pfn);
            first_pfn += size;
        }
    }
}

/* Allocate 2^order contiguous pages; returns pfn or M4K_PFN_NONE */
static uint32_t buddy_alloc(struct mkrn_buddy_zone *z,
                            uint32_t order)
{
    if (order >= M4K_BUDDY_MAX_ORDER)
        return M4K_PFN_NONE;

    uint32_t cur = order;
    while (cur < M4K_BUDDY_MAX_ORDER &&
           z->free_head[cur] == M4K_PFN_NONE)
        cur++;
    if (cur == M4K_BUDDY_MAX_ORDER)
        return M4K_PFN_NONE;

    uint32_t pfn = z->free_head[cur];
    buddy_list_del(z, cur, pfn);
    z->nr_free -= 1u << cur;

    while (cur > order) {              /* expand: split down */
        cur--;
        uint32_t half = pfn + (1u << cur);
        z->nr_free += 1u << cur;
        buddy_list_add(z, cur, half);
    }
    return pfn;
}

/* ── Region tracking / init ────────────────────────────────── */

static void
memory_add_region(uint32_t u32Start, uint32_t u32Size,
                  uint32_t u32Type)
{
    if (u32MemoryRegionCount >= MAX_MEMORY_REGIONS)
        return;

    mkrn_mem_region_t *pRegion =
        &memory_region_pool[u32MemoryRegionCount++];
    pRegion->start = u32Start;
    pRegion->size = u32Size;
    pRegion->type = u32Type;
    pRegion->next = pMemoryRegions;
    pMemoryRegions = pRegion;

    u32TotalMemory += u32Size;
    if (u32Type == M4K_MEM_TYPE_FREE)
        u32FreeMemory += u32Size;
}

void
mkrn_memory_init(multiboot_info_t *pMbInfo)
{
    /* mmap is the authoritative source when present: mem_lower/upper
     * AND mmap describe the SAME physical RAM, and both branches used
     * to run when both flags are set (QEMU sets both).  Seeding the
     * buddy twice put already-linked blocks back on the free lists a
     * second time — list self-loops, inflated free counts, and the
     * same page handed to two owners.  Only fall back to the coarse
     * mem_lower/upper values when no mmap exists. */
    if (!(pMbInfo->flags & M4K_MULTIBOOT_INFO_MEM_MAP)
        && (pMbInfo->flags & M4K_MULTIBOOT_INFO_MEMORY)) {
        if (pMbInfo->mem_lower > 0)
            memory_add_region(
                0, pMbInfo->mem_lower * 1024,
                M4K_MEM_TYPE_FREE);

        if (pMbInfo->mem_upper > 0)
            memory_add_region(
                0x100000,
                pMbInfo->mem_upper * 1024,
                M4K_MEM_TYPE_FREE);
    }

    if (pMbInfo->flags & M4K_MULTIBOOT_INFO_MEM_MAP) {
        multiboot_mmap_entry_t *pEntry =
            (multiboot_mmap_entry_t *)
                pMbInfo->mmap_addr;
        while ((uint32_t)pEntry
               < pMbInfo->mmap_addr
                     + pMbInfo->mmap_length)
        {
            if (pEntry->type
                == M4K_MULTIBOOT_MEMORY_AVAILABLE)
                memory_add_region(
                    (uint32_t)pEntry->addr,
                    (uint32_t)pEntry->len,
                    M4K_MEM_TYPE_FREE);
            else
                memory_add_region(
                    (uint32_t)pEntry->addr,
                    (uint32_t)pEntry->len,
                    M4K_MEM_TYPE_RESERVED);

            pEntry =
                (multiboot_mmap_entry_t *)
                    ((uint32_t)pEntry
                     + pEntry->size + 4);
        }
    }

    memory_add_region(M4K_KERNEL_BASE, 0x400000,
                      M4K_MEM_TYPE_RESERVED);
    mkrn_console_write("[BUDDY] regions added\n");

    u32KernelHeapStart = (uint32_t)&__heap_start;
    u32KernelHeapEnd = (uint32_t)&__heap_end;

    /* Buddy zone: manage physical RAM from ABOVE the kernel image +
     * linker heap + fixed-address ramdisk (0x2000000..0x3000000,
     * reserved for the YAFS root device) up to min(total, 512MB).
     * Free-list nodes are stored in the freed pages themselves, so
     * nothing below the highest permanent allocation may ever enter
     * the buddy.  The user ELF window (0x600000..0x127BE40) sits
     * below this zone start and never overlaps buddy pages either. */
    uint32_t zone_start =
        (u32KernelHeapEnd > 0x3000000u)
            ? ((u32KernelHeapEnd + M4K_PAGE_SIZE - 1)
               & ~(uint32_t)(M4K_PAGE_SIZE - 1))
            : 0x3000000u;
    uint32_t zone_end = u32TotalMemory;
    if (zone_end > 512u * 1024u * 1024u)
        zone_end = 512u * 1024u * 1024u;
    if (zone_end < zone_start)
        zone_end = zone_start;

    buddy_zone.base_pfn = zone_start >> 12;
    buddy_zone.nr_pages =
        (zone_end - zone_start) >> 12;
    buddy_zone.nr_free = 0;
    buddy_zone.order_map = buddy_order_map;
    for (int i = 0; i < M4K_BUDDY_MAX_ORDER; i++) {
        buddy_zone.free_head[i] = M4K_PFN_NONE;
        buddy_zone.free_count[i] = 0;
    }
    for (uint32_t i = 0; i < M4K_MAX_PFNS; i++)
        buddy_order_map[i] = 0xFF;
    mkrn_console_write("[BUDDY] map cleared\n");

    /* Free every AVAILABLE region inside the zone window... */
    mkrn_mem_region_t *pRegion = pMemoryRegions;
    while (pRegion != NULL) {
        if (pRegion->type == M4K_MEM_TYPE_FREE) {
            uint32_t s = pRegion->start >> 12;
            uint32_t e =
                (pRegion->start + pRegion->size) >> 12;
            if (e > (zone_end >> 12))
                e = zone_end >> 12;
            if (s < (zone_start >> 12))
                s = zone_start >> 12;
            if (s < e)
                buddy_free_range(&buddy_zone, s, e - s);
        }
        pRegion = pRegion->next;
    }
    mkrn_console_write("[BUDDY] free ranges seeded\n");

    /* Recompute free counters from the buddy's own view */
    u32FreeMemory = buddy_zone.nr_free * M4K_PAGE_SIZE;
    u32UsedMemory = u32TotalMemory - u32FreeMemory;
    mkrn_console_write("[BUDDY] ready\n");
}

uint32_t
mkrn_memory_get_total(void)
{
    return u32TotalMemory;
}

uint32_t
mkrn_memory_get_free(void)
{
    /* Buddy free pages minus whatever the heap carved later */
    return u32FreeMemory;
}

uint32_t
mkrn_memory_get_used(void)
{
    return u32UsedMemory;
}

static uint32_t
allocate_pages(uint32_t u32Pages)
{
    if (u32Pages == 0)
        return 0;
    uint32_t order = 0;
    while ((1u << order) < u32Pages)
        order++;
    uint32_t pfn = buddy_alloc(&buddy_zone, order);
    if (pfn == M4K_PFN_NONE)
        return 0;

    /* Oversized tail of the last block is freed back (the caller
     * tracks only u32Pages — keep accounting identical to the
     * bitmap allocator by releasing the excess). */
    uint32_t got = 1u << order;
    if (got > u32Pages) {
        buddy_free_range(&buddy_zone, pfn + u32Pages,
                         got - u32Pages);
        u32FreeMemory +=
            (got - u32Pages) * M4K_PAGE_SIZE;
    }
    u32FreeMemory -= u32Pages * M4K_PAGE_SIZE;
    u32UsedMemory += u32Pages * M4K_PAGE_SIZE;
    return pfn << 12;
}

static void
free_pages(uint32_t u32Address, uint32_t u32Pages)
{
    if (u32Pages == 0)
        return;
    uint32_t pfn = u32Address >> 12;
    buddy_free_range(&buddy_zone, pfn, u32Pages);
    u32FreeMemory += u32Pages * M4K_PAGE_SIZE;
    u32UsedMemory -= u32Pages * M4K_PAGE_SIZE;
}

void *
mkrn_memory_alloc(size_t size)
{
    return mkrn_alloc(size);
}

void
mkrn_memory_free(void *pPtr)
{
    mkrn_free(pPtr);
}

void *
mkrn_memory_alloc_page(size_t pages)
{
    return (void *)(uintptr_t)allocate_pages((uint32_t)pages);
}

void
mkrn_memory_free_page(void *pPtr, size_t pages)
{
    free_pages((uint32_t)(uintptr_t)pPtr, (uint32_t)pages);
}

/* ── Kernel heap (free-list, unchanged ABI) ────────────────── */

void *
mkrn_alloc(size_t size)
{
    if (size == 0)
        return NULL;

    size = (size + 7) & ~7;

    mkrn_mem_block_t *pBlock = pKernelHeapBlocks;

    while (pBlock != NULL) {
        if (!pBlock->used
            && pBlock->size >= size)
        {
            if (pBlock->size
                > size + sizeof(mkrn_mem_block_t) + 8)
            {
                mkrn_mem_block_t *pNewBlock =
                    (mkrn_mem_block_t *)
                        ((uint32_t)pBlock
                         + sizeof(mkrn_mem_block_t)
                         + size);
                pNewBlock->start =
                    pBlock->start
                    + sizeof(mkrn_mem_block_t) + size;
                pNewBlock->size =
                    pBlock->size
                    - sizeof(mkrn_mem_block_t) - size;
                pNewBlock->used = 0;
                pNewBlock->next = pBlock->next;
                pNewBlock->prev = pBlock;
                if (pNewBlock->next)
                    pNewBlock->next->prev = pNewBlock;

                pBlock->size = size;
                pBlock->next = pNewBlock;
            }

            pBlock->used = 1;
            u32UsedMemory += (uint32_t)size;
            u32FreeMemory -= (uint32_t)size;

            return (void *)((uint32_t)pBlock
                            + sizeof(mkrn_mem_block_t));
        }

        pBlock = pBlock->next;
    }

    if (u32KernelHeapStart + sizeof(mkrn_mem_block_t)
            + size
        <= u32KernelHeapEnd)
    {
        pBlock =
            (mkrn_mem_block_t *)u32KernelHeapStart;
        pBlock->start =
            u32KernelHeapStart
            + sizeof(mkrn_mem_block_t);
        pBlock->size = size;
        pBlock->used = 1;
        pBlock->next = pKernelHeapBlocks;
        pBlock->prev = NULL;
        if (pKernelHeapBlocks)
            pKernelHeapBlocks->prev = pBlock;
        pKernelHeapBlocks = pBlock;
        u32KernelHeapStart +=
            sizeof(mkrn_mem_block_t) + size;

        u32UsedMemory += (uint32_t)size;
        u32FreeMemory -= (uint32_t)size;

        return (void *)pBlock->start;
    }

    return NULL;
}

void
mkrn_free(void *pPtr)
{
    if (pPtr == NULL)
        return;

    /* O(1) fast path: every mkrn_alloc hands out
     * (block + sizeof(mkrn_mem_block_t)), so the owning header sits
     * directly below the returned pointer.  Validate the back-computed
     * header and fall back to the full-list scan if it does not match
     * (defensive against foreign pointers). */
    mkrn_mem_block_t *pBlock =
        (mkrn_mem_block_t *)pPtr - 1;
    if ((void *)pBlock->start != pPtr || !pBlock->used)
        pBlock = pKernelHeapBlocks;
    while (pBlock != NULL) {
        if ((void *)pBlock->start == pPtr
            && pBlock->used)
        {
            pBlock->used = 0;
            u32UsedMemory -= pBlock->size;
            u32FreeMemory += pBlock->size;

            /* Merge every adjacent free neighbour (right, then
             * left).  A single-block merge leaves split holes that
             * never recombine, so the list — and the O(n) first-fit
             * scan in mkrn_alloc — grows on every alloc/free cycle
             * (fragmentation + perf leak). */
            mkrn_mem_block_t *pNext = pBlock->next;
            while (pNext != NULL && !pNext->used) {
                pBlock->size +=
                    sizeof(mkrn_mem_block_t)
                    + pNext->size;
                pBlock->next = pNext->next;
                if (pBlock->next)
                    pBlock->next->prev = pBlock;
                pNext = pBlock->next;
            }

            if (pBlock->prev != NULL && !pBlock->prev->used) {
                mkrn_mem_block_t *pPrevB = pBlock->prev;
                pPrevB->size +=
                    sizeof(mkrn_mem_block_t)
                    + pBlock->size;
                pPrevB->next = pBlock->next;
                if (pPrevB->next)
                    pPrevB->next->prev = pPrevB;
            }

            return;
        }
        pBlock = pBlock->next;
    }
}
