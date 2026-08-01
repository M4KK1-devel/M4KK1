/*
 * M4KK1 4P1 - stdlib.c
 * Description: Minimal stdlib implementation for M4KK1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "include/stdlib.h"
#include "include/string.h"
#include "include/unistd.h"
#include "include/errno.h"

/* Simple heap allocator */
#define HEAP_SIZE (1024 * 1024)  /* 1MB heap */
static char heap[HEAP_SIZE];
static size_t heap_ptr = 0;

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
} block_header_t;

static block_header_t *heap_start = NULL;

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    /* Align to 8 bytes */
    size = (size + 7) & ~7;

    if (!heap_start) {
        /* First allocation */
        if (heap_ptr + sizeof(block_header_t) + size > HEAP_SIZE) {
            errno = ENOMEM;
            return NULL;
        }
        block_header_t *block = (block_header_t *)&heap[heap_ptr];
        block->size = size;
        block->free = 0;
        block->next = NULL;
        heap_start = block;
        heap_ptr += sizeof(block_header_t) + size;
        return (void *)((char *)block + sizeof(block_header_t));
    }

    /* Search for free block */
    block_header_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            current->free = 0;
            return (void *)((char *)current + sizeof(block_header_t));
        }
        if (!current->next) {
            /* Allocate new block at end */
            if (heap_ptr + sizeof(block_header_t) + size > HEAP_SIZE) {
                errno = ENOMEM;
                return NULL;
            }
            block_header_t *block = (block_header_t *)&heap[heap_ptr];
            block->size = size;
            block->free = 0;
            block->next = NULL;
            current->next = block;
            heap_ptr += sizeof(block_header_t) + size;
            return (void *)((char *)block + sizeof(block_header_t));
        }
        current = current->next;
    }

    errno = ENOMEM;
    return NULL;
}

void *calloc(size_t nmemb, size_t size)
{
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

    block_header_t *block = (block_header_t *)((char *)ptr - sizeof(block_header_t));
    if (block->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}

void free(void *ptr)
{
    if (!ptr) return;
    block_header_t *block = (block_header_t *)((char *)ptr - sizeof(block_header_t));
    block->free = 1;
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
