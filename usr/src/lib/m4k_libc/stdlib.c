/*
 * M4KK1 4P1 - stdlib.c
 * Description: Minimal stdlib implementation for M4KK1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "include/stdlib.h"
#include "include/stdint.h"
#include "include/string.h"
#include "include/unistd.h"
#include "include/errno.h"

/* Hardened heap allocator (2026-09-20 heap-hardening round).
 *
 * Guarantees:
 *  - free() coalesces with the next (and previous) free neighbour, so
 *    interleaved alloc/free cycles do not fragment the 1 MB arena.
 *  - malloc() splits oversized free blocks (remainder >= header + 16).
 *  - double-free and wild-free (pointer that is not a live block) are
 *    detected and ignored, never corrupt the list.
 *  - a canary byte after every user region catches off-by-one / small
 *    overflows at free() time (recorded in heap_stats(), no crash —
 *    the corruption is already done, but it is now observable).
 *  - calloc() rejects nmemb*size overflow.
 *
 * The free list is address-ordered (bump allocation appends at the
 * top), so next-block coalescing is a simple ->next walk and the
 * previous free neighbour is found by list order.
 */
#define HEAP_SIZE (1024 * 1024)  /* 1MB heap */
static char heap[HEAP_SIZE] __attribute__((aligned(8)));
static size_t heap_ptr = 0;      /* bump offset of the next new block */

#define HEAP_MAGIC_USED 0x4D484B31u  /* "MHK1" */
#define HEAP_MAGIC_FREE 0x4D484B30u  /* same, low bit clear = free */
#define HEAP_CANARY     0xC7u
#define HEAP_SPLIT_MIN  16          /* smallest worth-while split */

typedef struct block_header {
    uint32_t magic;      /* HEAP_MAGIC_USED / HEAP_MAGIC_FREE */
    uint32_t canary;     /* copy of the canary value at data[req] */
    size_t size;         /* capacity of the user area (8-aligned) */
    size_t req_size;     /* exact requested bytes (canary anchor) */
    struct block_header *next;  /* address-ordered free list */
} block_header_t;

static block_header_t *heap_start = NULL;

/* Observability counters — struct lives in stdlib.h so callers
 * (heaptest, sysmon) can read it. */
static struct m4k_heap_stats hstats;

struct m4k_heap_stats heap_stats(void)
{
    /* Refresh the derived counters (kept O(n) but only on demand). */
    size_t used = 0, freeb = 0, live = 0, fb = 0, largest = 0;
    block_header_t *b = heap_start;
    while (b) {
        if (b->magic == HEAP_MAGIC_USED) { used += b->size; live++; }
        else {
            freeb += b->size; fb++;
            if (b->size > largest) largest = b->size;
        }
        b = b->next;
    }
    hstats.used_bytes = used;
    hstats.free_bytes = freeb;
    hstats.live_blocks = live;
    hstats.free_blocks = fb;
    hstats.largest_free = largest;
    hstats.heap_size = HEAP_SIZE;
    hstats.top_off = heap_ptr;
    return hstats;
}

static void block_set_canary(block_header_t *b)
{
    unsigned char *data = (unsigned char *)(b + 1);
    data[b->req_size] = HEAP_CANARY;
    b->canary = HEAP_CANARY;
}

static int block_canary_ok(block_header_t *b)
{
    unsigned char *data = (unsigned char *)(b + 1);
    return data[b->req_size] == (unsigned char)b->canary;
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    /* Capacity = align8(request + 1): guarantees at least one pad
     * byte after the user area for the canary, so it can never
     * spill into the next block's header.  The guard rejects
     * sizes whose 8-alignment would wrap size_t: (size_t)-1 would
     * otherwise align to 0, pass the arena check, then write the
     * canary 4 GB away via req_size (host-reproduced segfault). */
    if (size > HEAP_SIZE) {
        errno = ENOMEM;
        return NULL;
    }
    size_t orig = size;
    size = (orig + 8) & ~(size_t)7;
    if (size > HEAP_SIZE) {
        errno = ENOMEM;
        return NULL;
    }

    /* First-fit over the address-ordered list, splitting when the
     * remainder can host header + a usable block. */
    block_header_t *current = heap_start;
    while (current) {
        if (current->magic == HEAP_MAGIC_FREE && current->size >= size) {
            size_t rem = current->size - size;
            if (rem >= sizeof(block_header_t) + HEAP_SPLIT_MIN) {
                /* Split: new free block right after the taken part. */
                block_header_t *split =
                    (block_header_t *)((char *)(current + 1) + size);
                split->magic = HEAP_MAGIC_FREE;
                split->canary = 0;
                split->size = rem - sizeof(block_header_t);
                split->req_size = 0;
                split->next = current->next;
                current->next = split;
                current->size = size;
            }
            current->magic = HEAP_MAGIC_USED;
            current->req_size = orig;
            block_set_canary(current);
            return (void *)(current + 1);
        }
        if (!current->next) {
            /* Allocate new block at the top (capacity already has
             * the canary pad baked in — no extra slack needed) */
            if (heap_ptr + sizeof(block_header_t) + size > HEAP_SIZE) {
                errno = ENOMEM;
                return NULL;
            }
            block_header_t *block = (block_header_t *)&heap[heap_ptr];
            heap_ptr += sizeof(block_header_t) + size;
            block->magic = HEAP_MAGIC_USED;
            block->size = size;
            block->req_size = orig;
            block->next = NULL;
            block_set_canary(block);
            current->next = block;
            return (void *)(block + 1);
        }
        current = current->next;
    }

    /* Empty list: first allocation ever. */
    if (heap_ptr + sizeof(block_header_t) + size > HEAP_SIZE) {
        errno = ENOMEM;
        return NULL;
    }
    block_header_t *block = (block_header_t *)&heap[heap_ptr];
    heap_ptr += sizeof(block_header_t) + size;
    block->magic = HEAP_MAGIC_USED;
    block->size = size;
    block->req_size = orig;
    block->next = NULL;
    heap_start = block;
    block_set_canary(block);
    return (void *)(block + 1);
}

void *calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > (size_t)-1 / nmemb) {
        errno = ENOMEM;
        return NULL;    /* nmemb*size would overflow */
    }
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t *)ptr - 1;
    if (block->magic != HEAP_MAGIC_USED) {
        errno = EINVAL;
        return NULL;    /* not a live allocation */
    }
    if (size > HEAP_SIZE) {
        errno = ENOMEM;
        return NULL;    /* would wrap in align8 below; original
                         * block stays valid (POSIX: failed realloc
                         * leaves the old allocation untouched) */
    }
    size_t want = (size + 8) & ~(size_t)7;
    if (block->size >= want) {
        /* Shrink in place; split the tail back to the free list when
         * the remainder can host header + a usable block, so a big
         * allocation shrunk to tiny does not hog the arena. */
        size_t rem = block->size - want;
        if (rem >= sizeof(block_header_t) + HEAP_SPLIT_MIN) {
            block_header_t *split =
                (block_header_t *)((char *)(block + 1) + want);
            split->magic = HEAP_MAGIC_FREE;
            split->canary = 0;
            split->size = rem - sizeof(block_header_t);
            split->req_size = 0;
            split->next = block->next;
            block->next = split;
            block->size = want;
            /* forward-coalesce the new free block with any free
             * neighbours already following it in list order */
            block_header_t *pNext = split->next;
            while (pNext && pNext->magic == HEAP_MAGIC_FREE) {
                split->size += sizeof(block_header_t) + pNext->size;
                split->next = pNext->next;
                pNext = split->next;
            }
        }
        block->req_size = size;
        block_set_canary(block);
        return ptr;
    }

    /* Grow in place by absorbing the following free neighbour(s):
     * pass 1 computes the total without touching the list, pass 2
     * unlinks — a partial absorb that still falls short would
     * otherwise corrupt the chain. */
    {
        size_t total = block->size;
        block_header_t *it;
        for (it = block->next;
             it && it->magic == HEAP_MAGIC_FREE;
             it = it->next)
            total += sizeof(block_header_t) + it->size;
        if (total >= want) {
            it = block->next;
            while (it && it->magic == HEAP_MAGIC_FREE) {
                block->next = it->next;
                it = block->next;
            }
            size_t rem = total - want;
            if (rem >= sizeof(block_header_t) + HEAP_SPLIT_MIN) {
                block_header_t *split =
                    (block_header_t *)((char *)(block + 1) + want);
                split->magic = HEAP_MAGIC_FREE;
                split->canary = 0;
                split->size = rem - sizeof(block_header_t);
                split->req_size = 0;
                split->next = block->next;
                block->next = split;
            } else {
                want = total;   /* keep the slack, not worth a split */
            }
            block->size = want;
            block->req_size = size;
            block_set_canary(block);
            return ptr;
        }
    }

    size_t copy = block->req_size < size ? block->req_size : size;
    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, copy);
        free(ptr);
    }
    return new_ptr;
}

void free(void *ptr)
{
    if (!ptr) return;
    block_header_t *block = (block_header_t *)ptr - 1;
    if (block->magic != HEAP_MAGIC_USED &&
        block->magic != HEAP_MAGIC_FREE) {
        hstats.bad_free++;      /* wild free: not a heap block */
        return;
    }
    if (block->magic == HEAP_MAGIC_FREE) {
        hstats.double_free++;   /* already freed: ignore */
        return;
    }
    if (!block_canary_ok(block)) {
        hstats.corrupted++;     /* overflow detected; still reclaim */
    }
    block->magic = HEAP_MAGIC_FREE;

    /* Coalesce forward: list is address-ordered. */
    block_header_t *pNext = block->next;
    while (pNext && pNext->magic == HEAP_MAGIC_FREE) {
        block->size += sizeof(block_header_t) + pNext->size;
        block->next = pNext->next;
        pNext = block->next;
    }

    /* Coalesce backward: find the list predecessor and absorb this
     * block if it is free too (O(n) walk — the arena is one page
     * list, at most a few hundred blocks; correctness first). */
    if (block != heap_start) {
        block_header_t *p = heap_start;
        while (p && p->next != block)
            p = p->next;
        if (p && p->magic == HEAP_MAGIC_FREE) {
            p->size += sizeof(block_header_t) + block->size;
            p->next = block->next;
            /* the predecessor may now touch further free blocks that
             * were already merged above — it cannot: forward merge
             * consumed every adjacent free block after `block`. */
        }
    }
}

void exit(int status)
{
    _exit(status);
}

void abort(void)
{
    exit(1);
}

int atexit(void (*function)(void))
{
    (void)function;
    return 0;
}

int system(const char *command)
{
    (void)command;
    return -1;
}

int atoi(const char *nptr)
{
    int v = 0, sign = 1;
    while (*nptr == ' ') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;
    while (*nptr >= '0' && *nptr <= '9')
        v = v * 10 + (*nptr++ - '0');
    return v * sign;
}

long atol(const char *nptr)
{
    return (long)atoi(nptr);
}

double atof(const char *nptr)
{
    (void)nptr;
    return 0.0;
}

long strtol(const char *nptr, char **endptr, int base)
{
    long v = 0;
    int sign = 1;
    while (*nptr == ' ') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;

    if (base == 16 && nptr[0] == '0' && nptr[1] == 'x') nptr += 2;

    while (*nptr) {
        int digit;
        if (*nptr >= '0' && *nptr <= '9') digit = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'f') digit = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'F') digit = *nptr - 'A' + 10;
        else break;
        if (digit >= base) break;
        v = v * base + digit;
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return v * sign;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)strtol(nptr, endptr, base);
}

double strtod(const char *nptr, char **endptr)
{
    (void)nptr;
    (void)endptr;
    return 0.0;
}

static unsigned long rand_seed = 1;

int rand(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & 0x7fff);
}

void srand(unsigned int seed)
{
    rand_seed = seed;
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite)
{
    (void)name;
    (void)value;
    (void)overwrite;
    return -1;
}

int unsetenv(const char *name)
{
    (void)name;
    return -1;
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *))
{
    size_t left = 0, right = nmemb;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        const void *mid_ptr = (const char *)base + mid * size;
        int cmp = compar(key, mid_ptr);
        if (cmp == 0) return (void *)mid_ptr;
        if (cmp < 0) right = mid;
        else left = mid + 1;
    }
    return NULL;
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    if (nmemb <= 1) return;

    char *pivot = (char *)base;
    size_t i = 1, j = nmemb - 1;
    while (i <= j) {
        while (i <= j && compar((char *)base + i * size, pivot) < 0) i++;
        while (j > 0 && compar((char *)base + j * size, pivot) > 0) j--;
        if (i < j) {
            char tmp[256];
            memcpy(tmp, (char *)base + i * size, size);
            memcpy((char *)base + i * size, (char *)base + j * size, size);
            memcpy((char *)base + j * size, tmp, size);
        }
        i++;
        if (j > 0) j--;
    }
}

int abs(int j)
{
    return j < 0 ? -j : j;
}

long labs(long j)
{
    return j < 0 ? -j : j;
}
