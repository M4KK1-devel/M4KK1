/*
 * M4KK1 4P1 - test_proto_host.c
 * Description: Host-side unit tests for the Copland protocol layer
 *              (sys/src/copland/copland_proto.c).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Build (from repo root, inside WSL):
 *   gcc -Wall -Wextra -o /tmp/cp_test tools/testing/copland/test_proto_host.c \
 *       sys/src/copland/copland_proto.c && /tmp/cp_test
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "copland_proto.h"

static int pass = 0, fail = 0;

#define CHECK(cond, name) do { \
	if (cond) { pass++; printf("[PASS] %s\n", name); } \
	else      { fail++; printf("[FAIL] %s\n", name); } \
} while (0)

int main(void)
{
	uint8_t wire[CP_MSG_MAX];

	/* ── 1. damage message round-trip (spec example) ── */
	{
		struct cp_msgbuf m;
		int len;

		cp_msg_start(&m, wire, sizeof(wire), 7, CP_SURFACE_DAMAGE);
		cp_put_i32(&m, 10);
		cp_put_i32(&m, 20);
		cp_put_i32(&m, 100);
		cp_put_i32(&m, 200);
		len = cp_msg_finish(&m);
		CHECK(len == 24, "damage msg size = 24");
		CHECK(wire[0] == 7 && wire[1] == 0 && wire[2] == 0
		      && wire[3] == 0, "word0 = object id 7");
		CHECK(wire[4] == 2 && wire[5] == 0, "word1 low16 = opcode 2");
		CHECK(((uint32_t)wire[6] << 16 | wire[7] << 24) == 24u << 16,
		      "word1 high16 = size");
		{
			struct cp_msg_reader r;
			cp_reader_init(&r, wire, (uint32_t)len);
			CHECK(cp_get_i32(&r) == 10, "arg x = 10");
			CHECK(cp_get_i32(&r) == 20, "arg y = 20");
			CHECK(cp_get_i32(&r) == 100, "arg w = 100");
			CHECK(cp_get_i32(&r) == 200, "arg h = 200");
			CHECK(r.left == 0, "reader fully consumed");
		}
	}

	/* ── 2. string codec ── */
	{
		struct cp_msgbuf m;
		int len;
		char out[64];

		cp_msg_start(&m, wire, sizeof(wire), 3,
			     CP_REGISTRY_GLOBAL);
		cp_put_u32(&m, 1);                        /* name */
		CHECK(cp_put_string(&m, "copland_compositor") == 0,
		      "put_string ok");
		cp_put_u32(&m, 1);                        /* version */
		len = cp_msg_finish(&m);
		CHECK(len == 40, "global msg size = 40 (0x28)");
		{
			struct cp_msg_reader r;
			cp_reader_init(&r, wire, (uint32_t)len);
			CHECK(cp_get_u32(&r) == 1, "global name = 1");
			CHECK(cp_get_string(&r, out, sizeof(out)) == 19,
			      "string wire len = 19 (18 + NUL)");
			CHECK(strcmp(out, "copland_compositor") == 0,
			      "string round-trip");
			CHECK(r.left == 4, "version still pending");
			CHECK(cp_get_u32(&r) == 1, "version = 1");
			CHECK(r.left == 0, "reader fully consumed");
		}
		/* NULL string */
		{
			struct cp_msgbuf m2;
			char o2[8];
			cp_msg_start(&m2, wire, sizeof(wire), 1, 0);
			cp_put_string(&m2, 0);
			cp_msg_finish(&m2);
			{
				struct cp_msg_reader r2;
				cp_reader_init(&r2, wire, 12);
				CHECK(cp_get_string(&r2, o2, sizeof(o2))
				      == 0, "NULL string wire len 0");
				CHECK(o2[0] == 0, "NULL string -> empty");
			}
		}
	}

	/* ── 3. SPSC ring: write/read with wraparound ── */
	{
		static uint8_t rdata[64];
		uint32_t head = 0, tail = 0;
		struct cp_ring r;
		uint8_t msg[24];
		uint8_t out[24];
		int i, rc;

		cp_ring_init(&r, &head, &tail, rdata, sizeof(rdata));
		/* craft a 24B damage message */
		{
			struct cp_msgbuf m;
			cp_msg_start(&m, msg, sizeof(msg), 7,
				     CP_SURFACE_DAMAGE);
			cp_put_i32(&m, 1);
			cp_put_i32(&m, 2);
			cp_put_i32(&m, 3);
			cp_put_i32(&m, 4);
			cp_msg_finish(&m);
		}
		/* fill 2x24 = 48 bytes, then consume 24, write 24 more
		 * -> tail offset 72 wraps past 64 */
		rc = cp_ring_write(&r, msg, 24);
		CHECK(rc == 24, "ring write 1");
		rc = cp_ring_write(&r, msg, 24);
		CHECK(rc == 24, "ring write 2");
		CHECK(cp_ring_used(&r) == 48, "ring used = 48");
		rc = cp_ring_read(&r, out, sizeof(out));
		CHECK(rc == 24, "ring read 1 returns 24");
		CHECK(memcmp(out, msg, 24) == 0, "ring read 1 content");
		rc = cp_ring_write(&r, msg, 24);
		CHECK(rc == 24, "ring write 3 (wrap)");
		CHECK(tail == 72, "tail advanced past wrap");
		for (i = 0; i < 2; i++) {
			rc = cp_ring_read(&r, out, sizeof(out));
			CHECK(rc == 24 && memcmp(out, msg, 24) == 0,
			      "wrap read intact");
		}
		CHECK(cp_ring_used(&r) == 0, "ring empty after reads");
		rc = cp_ring_read(&r, out, sizeof(out));
		CHECK(rc == 0, "empty ring read = 0");
		/* partial message stays pending */
		rc = cp_ring_write(&r, msg, 16);
		CHECK(rc == 16, "write partial");
		rc = cp_ring_read(&r, out, sizeof(out));
		CHECK(rc == 0, "partial message not delivered");
		/* fullness: 64B ring can hold 2x24 then refuses 3rd */
		head = tail = 0;
		(void)cp_ring_write(&r, msg, 24);
		(void)cp_ring_write(&r, msg, 24);
		rc = cp_ring_write(&r, msg, 24);
		CHECK(rc == -1, "ring refuses when full");
		/* malformed header detection: corrupt the size byte */
		head = tail = 0;
		(void)cp_ring_write(&r, msg, 24);
		rdata[6] = 0xFF;           /* size = 0xFFxx > CP_MSG_MAX */
		rc = cp_ring_read(&r, out, sizeof(out));
		CHECK(rc == -1, "malformed header rejected");
	}

	/* ── 4. object map ── */
	{
		struct cp_objmap map;
		uint32_t a, b;

		cp_objmap_init(&map);
		a = cp_objmap_new_id(&map);
		b = cp_objmap_new_id(&map);
		CHECK(a == 2 && b == 3, "ids start at 2, increment");
		CHECK(cp_objmap_insert(&map, a, CP_IF_SURFACE) >= 0,
		      "insert surface");
		CHECK(cp_objmap_iface(&map, a) == CP_IF_SURFACE,
		      "lookup surface");
		CHECK(cp_objmap_insert(&map, a, CP_IF_SURFACE) == -1,
		      "duplicate insert rejected");
		CHECK(cp_objmap_iface(&map, b) == CP_IF_NONE,
		      "unknown id -> CP_IF_NONE");
		CHECK(cp_objmap_remove(&map, a) == 0, "remove ok");
		CHECK(cp_objmap_iface(&map, a) == CP_IF_NONE,
		      "removed id not found");
		CHECK(cp_objmap_remove(&map, a) == -1,
		      "double remove rejected");
	}

	printf("== copland proto host tests: %d pass, %d fail ==\n",
	       pass, fail);
	return fail ? 1 : 0;
}
