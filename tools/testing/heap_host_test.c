/*
 * M4KK1 4P1 - heap_host_test.c
 * Description: Host-side unit test for the hardened m4k_libc
 *              allocator.  Compiled against glibc headers; the
 *              allocator under test (m4k_libc/stdlib.c) is linked in
 *              separately (see heap_host_test.sh), so its malloc/
 *              free/calloc/realloc override glibc's.
 *              Dev/QA tool — NOT shipped in the ISO.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Interface of the allocator under test (mirrors
 * usr/src/lib/m4k_libc/include/stdlib.h — kept in sync manually so
 * this file compiles with glibc headers). */
struct m4k_heap_stats {
    size_t heap_size, top_off, used_bytes, free_bytes;
    size_t live_blocks, free_blocks, largest_free;
    size_t bad_free, double_free, corrupted;
};
struct m4k_heap_stats heap_stats(void);

static int pass = 0, total = 0;
static void check(const char *name, int ok)
{
	fprintf(stderr, "[HOSTHEAP] %s: %s\n", name, ok ? "OK" : "FAIL");
	total++;
	if (ok) pass++;
}

int main(void)
{
	/* stderr is unbuffered on the host: no hidden heap allocs from
	 * glibc's stdout buffer would pollute the assertions. */
	fprintf(stderr, "[HOSTHEAP] warmup\n");
	/* 1. coalesce: full collapse to one free block */
	{
		void *p[64];
		for (int i = 0; i < 64; i++) p[i] = malloc(64);
		for (int i = 0; i < 64; i += 2) free(p[i]);
		for (int i = 1; i < 64; i += 2) free(p[i]);
		struct m4k_heap_stats s = heap_stats();
		check("coalesce", s.free_blocks == 1 && s.live_blocks == 0);
	}
	/* 2. split: freed big block serves smaller without arena growth */
	{
		void *big = malloc(4096);
		free(big);
		struct m4k_heap_stats b = heap_stats();
		void *a = malloc(1024), *c = malloc(512), *d = malloc(256);
		struct m4k_heap_stats a2 = heap_stats();
		int ok = a && c && d && a2.top_off == b.top_off;
		free(a); free(c); free(d);
		check("split", ok);
	}
	/* 3. double free ignored */
	{
		void *p = malloc(32);
		free(p);
		struct m4k_heap_stats b = heap_stats();
		free(p);
		struct m4k_heap_stats a = heap_stats();
		check("doublefree", a.double_free == b.double_free + 1);
	}
	/* 4. wild free ignored */
	{
		static int data_var = 0;   /* static: not a stack-address
		 * warning, still outside the heap */
		struct m4k_heap_stats b = heap_stats();
		free(&data_var);
		struct m4k_heap_stats a = heap_stats();
		check("wildfree", a.bad_free == b.bad_free + 1);
	}
	/* 5. canary detects one-byte overflow */
	{
		char *p = malloc(16);
		struct m4k_heap_stats b = heap_stats();
		p[16] = 0x41;
		free(p);
		struct m4k_heap_stats a = heap_stats();
		check("canary", a.corrupted == b.corrupted + 1);
	}
	/* 6. calloc overflow / zeroing */
	{
		void *p = calloc(0x10000, 0x10000);
		void *q = calloc(4, 8);
		int zeroed = 1;
		unsigned char *qb = (unsigned char *)q;
		for (int i = 0; i < 32; i++) if (qb[i]) zeroed = 0;
		free(q);
		check("callocover", p == NULL);
		check("calloczero", q && zeroed);
	}
	/* 7. realloc preserves data */
	{
		char *p = malloc(32);
		strcpy(p, "realloc-data-ok");
		char *q = realloc(p, 4096);
		int ok = q && strcmp(q, "realloc-data-ok") == 0;
		char *r = realloc(q, 24);
		ok = ok && r && strncmp(r, "realloc-data-ok", 15) == 0;
		free(r);
		check("realloc", ok);
	}
	/* 7b. realloc shrink splits the tail back to the free list and
	 * regrow re-absorbs it in place: same pointer, zero arena growth.
	 * (The split tail may coalesce with the following free block —
	 * counts are therefore checked after the final free(), against
	 * the pre-alloc snapshot, not in between.) */
	{
		struct m4k_heap_stats b = heap_stats();
		char *p = malloc(2048);
		char *s = realloc(p, 32);   /* shrink: tail becomes free */
		struct m4k_heap_stats m = heap_stats();
		int ok = s == p && m.top_off == b.top_off
			&& m.largest_free >= 1900;
		char *g = realloc(s, 2048); /* regrow: absorb own tail back */
		ok = ok && g == p;
		free(g);                    /* full restoration */
		struct m4k_heap_stats a = heap_stats();
		ok = ok && a.top_off == b.top_off
			&& a.free_blocks == b.free_blocks
			&& a.live_blocks == b.live_blocks;
		check("reallocsplit", ok);
	}
	/* 8. recycle does not grow the arena */
	{
		void *slots[16] = {0};
		unsigned seed = 12345;
		for (int i = 0; i < 200; i++) {
			seed = seed * 1103515245u + 12345u;
			int slot = (seed >> 16) % 16;
			if (slots[slot]) { free(slots[slot]); slots[slot] = 0; }
			seed = seed * 1103515245u + 12345u;
			size_t sz = 1 + ((seed >> 16) % 2048);
			slots[slot] = malloc(sz);
			if (slots[slot]) memset(slots[slot], 0xAA, sz);
		}
		for (int i = 0; i < 16; i++) if (slots[i]) free(slots[i]);
		struct m4k_heap_stats s = heap_stats();
		check("recycle", s.live_blocks == 0 && s.free_blocks == 1);
	}
	/* 9. huge-size rejection: align8 must not wrap size_t — the
	 * pre-fix allocator segfaulted on malloc((size_t)-1) because
	 * the wrapped capacity bypassed the arena check while
	 * req_size kept the giant value (canary wrote far out). */
	{
		char *p = malloc(32);
		strcpy(p, "guard-intact");
		void *a = malloc((size_t)-1);
		void *b = malloc((size_t)-7);
		void *c = realloc(p, (size_t)-1);
		check("hugesize", a == NULL && b == NULL && c == NULL
			&& p != NULL && strcmp(p, "guard-intact") == 0);
		free(p);
	}
	/* 10. realloc of a foreign pointer must fail cleanly */
	{
		static int data_var = 0;
		void *r = realloc(&data_var, 64);
		check("reallocwild", r == NULL);
	}
	fprintf(stderr, "[HOSTHEAP] RESULT: %d/%d\n", pass, total);
	return total - pass;
}
