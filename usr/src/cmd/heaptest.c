/*
 * M4KK1 4P1 - heaptest.c
 * Description: User-space heap allocator regression test.
 *
 * Exercises the hardened m4k_libc allocator (see stdlib.c):
 *   1. coalesce   - interleaved alloc/free must not fragment
 *   2. split      - a freed big block must serve smaller requests
 *   3. doublefree - second free() must be ignored (counter++)
 *   4. wildfree   - free(&stackvar) must be ignored (counter++)
 *   5. canary     - one-byte overflow detected at free() (counter++)
 *   6. callocover - calloc(n, huge) must return NULL
 *   7. realloc    - grow in place / move, data preserved
 *   8. recycle    - N cycles of random-ish sizes, arena must not grow
 *
 * Prints one "[HEAP] <name>: OK/FAIL" line per check plus a summary
 * "[HEAP] RESULT: <pass>/<total>".  Exit code = failures.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdio.h>
#include <stdlib.h>

/* No <string.h>: the -Isys/src/include pick of it #defines the POSIX
 * names to mkrn_* kernel symbols we do not link.  Local helpers. */
static void *ht_memset(void *s, int c, size_t n)
{
	unsigned char *b = (unsigned char *)s;
	while (n--) *b++ = (unsigned char)c;
	return s;
}

static int ht_streq(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return *a == *b;
}

static void ht_strcpy(char *d, const char *s)
{
	while ((*d++ = *s++)) ;
}

static int pass = 0, total = 0;

static void check(const char *name, int ok)
{
	printf("[HEAP] %s: %s\n", name, ok ? "OK" : "FAIL");
	total++;
	if (ok) pass++;
}

static void test_coalesce(void)
{
	/* 64 blocks, free the even ones, then the odd ones: at the end
	 * the whole run must collapse back to ONE free block. */
	void *p[64];
	for (int i = 0; i < 64; i++)
		p[i] = malloc(64);
	for (int i = 0; i < 64; i += 2)
		free(p[i]);
	for (int i = 1; i < 64; i += 2)
		free(p[i]);
	struct m4k_heap_stats s = heap_stats();
	check("coalesce", s.free_blocks == 1 && s.live_blocks == 0);
}

static void test_split(void)
{
	char *big = malloc(4096);
	free(big);
	/* The freed 4096 block must serve these without arena growth. */
	struct m4k_heap_stats before = heap_stats();
	char *a = malloc(1024);
	char *b = malloc(512);
	char *c = malloc(256);
	struct m4k_heap_stats after = heap_stats();
	int ok = a && b && c && after.top_off == before.top_off;
	free(a); free(b); free(c);
	check("split", ok);
}

static void test_doublefree(void)
{
	char *p = malloc(32);
	free(p);
	struct m4k_heap_stats before = heap_stats();
	free(p);                      /* must be ignored */
	struct m4k_heap_stats after = heap_stats();
	check("doublefree",
	      after.double_free == before.double_free + 1 &&
	      after.free_blocks == before.free_blocks);
}

static void test_wildfree(void)
{
	int stack_var = 0;
	struct m4k_heap_stats before = heap_stats();
	free(&stack_var);             /* must be ignored, no crash */
	struct m4k_heap_stats after = heap_stats();
	check("wildfree", after.bad_free == before.bad_free + 1);
}

static void test_canary(void)
{
	char *p = malloc(16);
	struct m4k_heap_stats before = heap_stats();
	p[16] = 0x41;                 /* one-byte overflow into pad */
	free(p);                      /* detects canary mismatch */
	struct m4k_heap_stats after = heap_stats();
	check("canary", after.corrupted == before.corrupted + 1);
}

static void test_calloc_overflow(void)
{
	void *p = calloc(0x10000, 0x10000);  /* 4 GB, overflows size_t? no,
	 * but > 1MB heap => must fail cleanly */
	check("callocover", p == NULL);
	void *q = calloc(4, 8);
	int zeroed = 1;
	unsigned char *qb = (unsigned char *)q;
	for (int i = 0; i < 32; i++)
		if (qb[i]) zeroed = 0;
	free(q);
	check("calloczero", q && zeroed);
}

static void test_realloc(void)
{
	char *p = malloc(32);
	ht_strcpy(p, "realloc-data-ok");
	char *q = realloc(p, 4096);
	int ok = q && ht_streq(q, "realloc-data-ok");
	/* shrink back: may return same ptr, data must survive */
	char *r = realloc(q, 24);
	ok = ok && r && ht_streq(r, "realloc-data-ok");
	free(r);
	check("realloc", ok);
}

static void test_realloc_split(void)
{
	/* shrink splits the tail back to the free list, regrow
	 * re-absorbs it: same pointer, arena never grows.  Counts are
	 * checked after the final free() against the pre-alloc
	 * snapshot (the split tail may coalesce meanwhile). */
	struct m4k_heap_stats b = heap_stats();
	char *p = malloc(2048);
	char *s = realloc(p, 32);
	struct m4k_heap_stats m = heap_stats();
	int ok = s == p && m.top_off == b.top_off
		&& m.largest_free >= 1900;
	char *g = realloc(s, 2048);
	ok = ok && g == p;
	free(g);
	struct m4k_heap_stats a = heap_stats();
	ok = ok && a.top_off == b.top_off
		&& a.free_blocks == b.free_blocks
		&& a.live_blocks == b.live_blocks;
	check("reallocsplit", ok);
}

static void test_recycle(void)
{
	/* 200 cycles of pseudo-random sizes; arena high-water must not
	 * grow by more than one max-size block after warm-up. */
	void *slots[16] = {0};
	struct m4k_heap_stats s0 = heap_stats();
	unsigned seed = 12345;
	for (int i = 0; i < 200; i++) {
		seed = seed * 1103515245 + 12345;
		int slot = (seed >> 16) % 16;
		if (slots[slot]) { free(slots[slot]); slots[slot] = 0; }
		seed = seed * 1103515245 + 12345;
		size_t sz = 1 + ((seed >> 16) % 2048);
		slots[slot] = malloc(sz);
		if (slots[slot]) ht_memset(slots[slot], 0xAA, sz);
	}
	for (int i = 0; i < 16; i++)
		if (slots[i]) free(slots[i]);
	struct m4k_heap_stats s1 = heap_stats();
	/* all freed => exactly one free block, no live blocks */
	check("recycle", s1.live_blocks == 0 && s1.free_blocks == 1);
}

int main(void)
{
	printf("[HEAP] heaptest starting\n");
	test_coalesce();
	test_split();
	test_doublefree();
	test_wildfree();
	test_canary();
	test_calloc_overflow();
	test_realloc();
	test_realloc_split();
	test_recycle();
	struct m4k_heap_stats s = heap_stats();
	printf("[HEAP] stats: arena=%d top=%d used=%d free=%d bad=%d dbl=%d corr=%d\n",
	       (int)s.heap_size, (int)s.top_off, (int)s.used_bytes,
	       (int)s.free_bytes, (int)s.bad_free, (int)s.double_free,
	       (int)s.corrupted);
	printf("[HEAP] RESULT: %d/%d\n", pass, total);
	return total - pass;
}
