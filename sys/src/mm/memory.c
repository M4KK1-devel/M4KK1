/*
 * M4KK1 4P1 - memory.c
 * Description: Memory management — region tracking,
 *              page bitmap allocation, kernel heap.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/memory.h"
#include "../include/console.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

static uint32_t u32PageBitmap[32768] = {0};
static uint32_t u32TotalPages = 0;
static uint32_t u32FreePagesCount = 0;

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
    if (pMbInfo->flags & M4K_MULTIBOOT_INFO_MEMORY) {
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

    u32KernelHeapStart = (uint32_t)&__heap_start;
    u32KernelHeapEnd = (uint32_t)&__heap_end;

    u32TotalPages = u32TotalMemory >> 12;
    u32FreePagesCount = u32FreeMemory >> 12;

    for (uint32_t i = 0; i < u32TotalPages / 32; i++)
        u32PageBitmap[i] = 0xFFFFFFFF;

    mkrn_mem_region_t *pRegion = pMemoryRegions;
    while (pRegion != NULL) {
        if (pRegion->type == M4K_MEM_TYPE_FREE) {
            uint32_t u32StartPage =
                pRegion->start / M4K_PAGE_SIZE;
            uint32_t u32EndPage =
                (pRegion->start + pRegion->size)
                / M4K_PAGE_SIZE;

            for (uint32_t page = u32StartPage;
                 page < u32EndPage; page++)
            {
                if (page < u32TotalPages)
                    u32PageBitmap[page / 32]
                        &= ~(1 << (page % 32));
            }
        }
        pRegion = pRegion->next;
    }
}

uint32_t
mkrn_memory_get_total(void)
{
    return u32TotalMemory;
}

uint32_t
mkrn_memory_get_free(void)
{
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
    uint32_t u32Consecutive = 0;
    uint32_t u32StartPage = 0;

    for (uint32_t i = 0; i < u32TotalPages; i++) {
        if ((u32PageBitmap[i / 32]
             & (1 << (i % 32)))
            == 0)
        {
            if (u32Consecutive == 0)
                u32StartPage = i;
            u32Consecutive++;

            if (u32Consecutive == u32Pages) {
                for (uint32_t j = u32StartPage;
                     j < u32StartPage + u32Pages;
                     j++)
                    u32PageBitmap[j / 32]
                        |= (1 << (j % 32));

                u32FreePagesCount -= u32Pages;
                u32UsedMemory
                    += u32Pages * M4K_PAGE_SIZE;
                u32FreeMemory
                    -= u32Pages * M4K_PAGE_SIZE;

                return u32StartPage * M4K_PAGE_SIZE;
            }
        } else {
            u32Consecutive = 0;
        }
    }

    return 0;
}

static void
free_pages(uint32_t u32Address, uint32_t u32Pages)
{
    uint32_t u32StartPage = u32Address / M4K_PAGE_SIZE;

    for (uint32_t i = u32StartPage;
         i < u32StartPage + u32Pages; i++)
        u32PageBitmap[i / 32]
            &= ~(1 << (i % 32));

    u32FreePagesCount += u32Pages;
    u32UsedMemory -= u32Pages * M4K_PAGE_SIZE;
    u32FreeMemory += u32Pages * M4K_PAGE_SIZE;
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
    uint32_t u32Address = allocate_pages(
        (uint32_t)pages);
    return (void *)u32Address;
}

void
mkrn_memory_free_page(void *pPtr, size_t pages)
{
    free_pages((uint32_t)pPtr, (uint32_t)pages);
}

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

    mkrn_mem_block_t *pBlock = pKernelHeapBlocks;
    while (pBlock != NULL) {
        if ((void *)pBlock->start == pPtr
            && pBlock->used)
        {
            pBlock->used = 0;
            u32UsedMemory -= pBlock->size;
            u32FreeMemory += pBlock->size;

            mkrn_mem_block_t *pNext = pBlock->next;
            if (pNext != NULL && !pNext->used) {
                pBlock->size +=
                    sizeof(mkrn_mem_block_t)
                    + pNext->size;
                pBlock->next = pNext->next;
            }

            return;
        }
        pBlock = pBlock->next;
    }
}
