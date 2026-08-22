/*
 * M4KK1 4P1 - test_compositor_host.c
 * Description: Host-side unit tests for the Copland compositor core
 *              (sys/src/copland/compositor.c).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Build (from repo root, inside WSL):
 *   gcc -Wall -Wextra -Isys/src/copland -o /tmp/cp_comp_test \
 *       tools/testing/copland/test_compositor_host.c \
 *       sys/src/copland/compositor.c sys/src/copland/copland_proto.c \
 *       && /tmp/cp_comp_test
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "compositor.h"

static int pass = 0, fail = 0;

#define CHECK(cond, name) do { \
	if (cond) { pass++; printf("[PASS] %s\n", name); } \
	else      { fail++; printf("[FAIL] %s\n", name); } \
} while (0)

/* Test fixtures: one client connection (static, like a real client). */
static struct cp_conn test_conn;
static uint8_t pool_mem[320 * 240 * 4];

/* Backend recording */
static int blit_calls, fill_calls, wp_calls;
static int last_blit_x, last_blit_y, last_blit_w, last_blit_h;
static uint32_t last_blit_stride;
static const uint8_t *last_blit_src;

static void t_blit(int x, int y, int w, int h, const uint8_t *src,
		   uint32_t stride)
{
	blit_calls++;
	last_blit_x = x; last_blit_y = y;
	last_blit_w = w; last_blit_h = h;
	last_blit_src = src; last_blit_stride = stride;
}

static void t_fill(int x, int y, int w, int h, uint32_t argb)
{
	fill_calls++;
	(void)x; (void)y; (void)w; (void)h; (void)argb;
}

static void t_wallpaper(int x, int y, int w, int h)
{
	wp_calls++;
	(void)x; (void)y; (void)w; (void)h;
}

static const struct cp_backend t_backend = {
	t_blit, t_fill, t_wallpaper
};

/* Client request helper: build + push one request into the conn. */
static void req_u32s(struct cp_conn *c, uint32_t obj, uint32_t op,
		     const uint32_t *args, int n)
{
	uint8_t buf[CP_MSG_MAX];
	struct cp_msgbuf m;
	struct cp_ring r;
	int i;

	cp_msg_start(&m, buf, sizeof(buf), obj, op);
	for (i = 0; i < n; i++)
		cp_put_u32(&m, args[i]);
	cp_msg_finish(&m);
	cp_ring_init(&r, &c->req_head, &c->req_tail,
		     c->req_data, c->req_size);
	if (cp_ring_write(&r, buf, m.len) < 0)
		printf("!! req ring write failed\n");
}

/* Event reader: pop one event from the conn, return opcode+obj. */
static int evt_pop(struct cp_conn *c, uint32_t *obj, uint32_t *op,
		   uint32_t *a0, uint32_t *a1)
{
	uint8_t buf[CP_MSG_MAX];
	struct cp_ring r;
	int rc;

	cp_ring_init(&r, &c->evt_head, &c->evt_tail,
		     c->evt_data, c->evt_size);
	rc = cp_ring_read(&r, buf, sizeof(buf));
	if (rc <= 0)
		return rc;
	*obj = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8)
	    | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
	*op = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8);
	if (a0) {
		struct cp_msg_reader rd;
		cp_reader_init(&rd, buf, (uint32_t)rc);
		*a0 = cp_get_u32(&rd);
		if (a1)
			*a1 = cp_get_u32(&rd);
	}
	return rc;
}

int main(void)
{
	struct cp_compositor *c = &cp_comp;
	struct cp_client *cl;
	uint32_t obj, op, a0, a1;

	/* ── setup ── */
	memset(&test_conn, 0, sizeof(test_conn));
	cp_client_conn_init(&test_conn);
	test_conn.client_id = 1;   /* normally assigned by the server */
	memset(pool_mem, 0x5A, sizeof(pool_mem));

	cp_compositor_init(c);
	cp_compositor_set_backend(c, &t_backend);
	{
		int ci = cp_compositor_attach_client(c, &test_conn);
		CHECK(ci == 0, "attach client -> index 0");
		cl = &c->clients[0];
	}

	/* ── 1. registry round trip ── */
	{
		uint32_t args[1] = { 2 };  /* registry id */
		req_u32s(&test_conn, CP_ID_DISPLAY,
			 CP_DISPLAY_GET_REGISTRY, args, 1);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch get_registry");
		/* 4 global events expected */
		{
			int k, got = 0;
			for (k = 0; k < 4; k++) {
				CHECK(evt_pop(&test_conn, &obj, &op, &a0,
					      &a1) > 0
				      && obj == 2
				      && op == CP_REGISTRY_GLOBAL,
				      "registry.global event");
				got++;
			}
			CHECK(got == 4, "4 globals advertised");
			CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1)
			      == 0, "event ring drained");
		}
	}

	/* ── 2. bind compositor + shm, create pool ── */
	{
		uint32_t bind_args[2] = { CP_IF_COMPOSITOR, 3 };
		uint32_t pool_args[3] = { 4, (uint32_t)(uintptr_t)pool_mem,
					  (uint32_t)sizeof(pool_mem) };
		req_u32s(&test_conn, 2, CP_REGISTRY_BIND, bind_args, 2);
		CHECK(cp_compositor_dispatch(c, cl) == 1, "dispatch bind");
		req_u32s(&test_conn, 2, CP_REGISTRY_BIND,
			 (uint32_t[2]){ CP_IF_SHM, 5 }, 2);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch bind shm");
		CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1) > 0
		      && op == CP_SHM_FORMAT,
		      "shm.format event after bind");
		req_u32s(&test_conn, 5, CP_SHM_CREATE_POOL, pool_args, 3);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch create_pool");
		CHECK(cp_find_pool(c, 4) != 0, "pool registered");
	}

	/* ── 3. buffer + surface + attach/damage/commit ── */
	{
		uint32_t buf_args[6] = { 6, 0, 320, 240, 320 * 4, 0 };
		uint32_t surf_args[1] = { 7 };
		uint32_t att_args[1] = { 6 };
		uint32_t dmg_args[4] = { 10, 20, 100, 50 };
		req_u32s(&test_conn, 4, CP_POOL_CREATE_BUFFER, buf_args, 6);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch create_buffer");
		CHECK(cp_find_buffer(c, 6) != 0, "buffer registered");
		req_u32s(&test_conn, 3, CP_COMPOSITOR_CREATE_SURFACE,
			 surf_args, 1);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch create_surface");
		CHECK(cp_find_surface(c, 7) != 0, "surface registered");
		CHECK(c->zcount == 1, "surface entered z-list");
		/* attach + damage + commit */
		req_u32s(&test_conn, 7, CP_SURFACE_ATTACH, att_args, 1);
		CHECK(cp_compositor_dispatch(c, cl) == 1, "dispatch attach");
		req_u32s(&test_conn, 7, CP_SURFACE_DAMAGE, dmg_args, 4);
		CHECK(cp_compositor_dispatch(c, cl) == 1, "dispatch damage");
		CHECK(cp_find_surface(c, 7)->pending_buffer == 6,
		      "attach staged in pending");
		CHECK(cp_find_surface(c, 7)->buffer_obj == 0,
		      "commit not yet applied");
		req_u32s(&test_conn, 7, CP_SURFACE_COMMIT, 0, 0);
		CHECK(cp_compositor_dispatch(c, cl) == 1, "dispatch commit");
		{
			struct cp_surface_state *s = cp_find_surface(c, 7);
			CHECK(s->buffer_obj == 6, "commit applied buffer");
			CHECK(s->w == 320 && s->h == 240,
			      "surface size from buffer");
			CHECK(s->mapped, "surface mapped");
		}
		CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1) > 0
		      && obj == 7 && op == CP_SURFACE_ENTER,
		      "surface.enter event on first commit");
	}

	/* ── 4. frame: composite fires blit, callbacks fire ── */
	{
		uint32_t cb_args[1] = { 8 };
		req_u32s(&test_conn, 7, CP_SURFACE_FRAME, cb_args, 1);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch frame request");
		CHECK(c->frame_dirty, "frame damage accumulated");
		cp_compositor_frame(c);
		CHECK(blit_calls == 1, "backend blit called once");
		/* 32-bit flat addressing: compare truncated addresses
		 * (host gcc builds run at 64-bit pointers; the wire
		 * protocol carries 32-bit addresses). */
		CHECK((uint32_t)(uintptr_t)last_blit_src
		      == (uint32_t)(uintptr_t)pool_mem,
		      "blit src = pool addr + offset 0");
		CHECK(last_blit_stride == 320 * 4, "blit stride");
		CHECK(wp_calls == 1, "wallpaper called for exposure");
		CHECK(!c->frame_dirty, "frame damage cleared");
		CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1) > 0
		      && obj == 8 && op == CP_CALLBACK_DONE && a0 == 1,
		      "frame callback fired with serial 1");
	}

	/* ── 5. input routing ── */
	{
		struct cp_surface_state *s = cp_find_surface(c, 7);
		CHECK(s != 0, "surface alive for input");
		s->x = 100;
		s->y = 100;
		CHECK(cp_input_pick(150, 200) == (s - c->surfaces),
		      "pick inside surface");
		CHECK(cp_input_pick(50, 50) == -1, "pick outside -> -1");
		CHECK(cp_input_pick(100, 100) == (s - c->surfaces),
		      "pick on top-left corner");
		CHECK(cp_input_pick(419, 339) == (s - c->surfaces),
		      "pick on bottom-right corner (inclusive)");
		CHECK(cp_input_pick(420, 340) == -1,
		      "pick just outside bottom-right");
		/* input region excludes clicks */
		s->has_input_region = 1;
		s->in_x = 0;
		s->in_y = 0;
		s->in_w = 10;
		s->in_h = 10;
		CHECK(cp_input_pick(150, 200) == -1,
		      "outside input region -> not picked");
		CHECK(cp_input_pick(105, 105) == (s - c->surfaces),
		      "inside input region -> picked");
		s->has_input_region = 0;
	}

	/* ── 6. invalid object / method errors ── */
	{
		req_u32s(&test_conn, 999, CP_SURFACE_COMMIT, 0, 0);
		CHECK(cp_compositor_dispatch(c, cl) == -1,
		      "unknown object -> -1");
		CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1) > 0
		      && op == CP_DISPLAY_ERROR
		      && a0 == 999
		      && a1 == CP_ERR_INVALID_OBJECT,
		      "display.error invalid_object");
		/* bad buffer geometry rejected */
		req_u32s(&test_conn, 4, CP_POOL_CREATE_BUFFER,
			 (uint32_t[6]){ 9, 0, 10, 10, 10 * 4, 1 }, 6);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch bad create_buffer");
		CHECK(evt_pop(&test_conn, &obj, &op, &a0, &a1) > 0
		      && op == CP_DISPLAY_ERROR
		      && a1 == CP_ERR_INVALID_METHOD,
		      "bad format rejected with error");
	}

	/* ── 7. surface destroy cleans up z-list and damage ── */
	{
		req_u32s(&test_conn, 7, CP_SURFACE_DESTROY, 0, 0);
		CHECK(cp_compositor_dispatch(c, cl) == 1,
		      "dispatch destroy");
		CHECK(cp_find_surface(c, 7) == 0, "surface gone");
		CHECK(c->zcount == 0, "z-list empty");
		CHECK(cp_input_pick(150, 200) == -1,
		      "no surface to pick after destroy");
	}

	printf("== copland compositor host tests: %d pass, %d fail ==\n",
	       pass, fail);
	return fail ? 1 : 0;
}
