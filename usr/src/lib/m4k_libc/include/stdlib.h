/*
 * M4KK1 4P1 - stdlib.h
 * Description: Minimal stdlib implementation for M4KK1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4K_STDLIB_H
#define _M4K_STDLIB_H

#include <stddef.h>

/* Memory allocation */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

/* Heap allocator statistics (hardened allocator, 2026-09-20) */
struct m4k_heap_stats {
    size_t heap_size;    /* arena size */
    size_t top_off;      /* bump offset (high-water growth) */
    size_t used_bytes;   /* sum of user capacities, used blocks */
    size_t free_bytes;   /* sum of user capacities, free blocks */
    size_t live_blocks;  /* used blocks */
    size_t free_blocks;
    size_t bad_free;     /* wild free() calls ignored */
    size_t double_free;  /* double free() calls ignored */
    size_t corrupted;    /* canary mismatches detected at free() */
};
struct m4k_heap_stats heap_stats(void);

/* Process control */
void exit(int status);
void abort(void);
int atexit(void (*function)(void));
int system(const char *command);

/* String conversion */
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);

/* Random numbers */
int rand(void);
void srand(unsigned int seed);

/* Environment */
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

/* Searching and sorting */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

/* Integer arithmetic */
int abs(int j);
long labs(long j);

/* Multibyte characters */
int mblen(const char *s, size_t n);
int mbtowc(int *pwc, const char *s, size_t n);
int wctomb(char *s, int wc);

/* Constants */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
#define MB_CUR_MAX 1

#endif /* _M4K_STDLIB_H */
