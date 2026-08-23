/*
 * M4KK1 4P1 - compositor.c
 * Description: Copland compositor core - object dispatch, surface
 *              state, damage tracking, atomic commit, input routing.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Cleanroom implementation per docs/copland/spec-protocol.md and
 * docs/copland/spec-event-flow.md.  Pure state machine over the
 * protocol layer; rendering goes through cp_backend hooks so this
 * file compiles on the host gcc for unit tests.
 */

#include "compositor.h"

/* Wire scratch shared by dispatch/event emission (the daemon is
 * single-threaded; one dispatch at a time). */
static uint8_t cp_scratch[CP_MSG_MAX];

struct cp_compositor cp_comp;

static const struct cp_backend *cp_backend;

/* ══════════ Event emission ══════════ */

static void
cp_evt_begin(struct cp_msgbuf *m, uint32_t obj, uint32_t opcode)
{
	cp_msg_start(m, cp_scratch, sizeof(cp_scratch), obj, opcode);
}

static void
cp_evt_send(struct cp_client *cl, struct cp_msgbuf *m)
{
	struct cp_ring r;

	if (cp_msg_finish(m) < 0)
		return;
	cp_ring_init(&r, &cl->conn->req_head, &cl->conn->evt_tail,
		     cl->conn->evt_data, cl->conn->evt_size);
	cp_ring_write(&r, cp_scratch, m->len);
}

/* ══════════ Object lookup ══════════ */

struct cp_surface_state *
cp_find_surface(struct cp_compositor *c, uint32_t obj_id)
{
	int i;

	for (i = 0; i < CP_MAX_SURFACES; i++)
		if (c->surfaces[i].obj_id == obj_id
		    && c->surfaces[i].obj_id != 0)
			return &c->surfaces[i];
	return 0;
}

struct cp_buffer_state *
cp_find_buffer(struct cp_compositor *c, uint32_t obj_id)
{
	int i;

	for (i = 0; i < CP_MAX_BUFFERS; i++)
		if (c->buffers[i].obj_id == obj_id
		    && c->buffers[i].obj_id != 0)
			return &c->buffers[i];
	return 0;
}

struct cp_pool_state *
cp_find_pool(struct cp_compositor *c, uint32_t obj_id)
{
	int i;

	for (i = 0; i < CP_MAX_POOLS; i++)
		if (c->pools[i].obj_id == obj_id
		    && c->pools[i].obj_id != 0)
			return &c->pools[i];
	return 0;
}

static struct cp_region_state *
cp_find_region(struct cp_compositor *c, uint32_t obj_id)
{
	int i;

	for (i = 0; i < CP_MAX_REGIONS; i++)
		if (c->regions[i].obj_id == obj_id
		    && c->regions[i].obj_id != 0)
			return &c->regions[i];
	return 0;
}

/* ══════════ Frame damage accounting ══════════ */

static void
cp_frame_accum(struct cp_compositor *c, int x, int y, int w, int h)
{
	int x2, y2;

	if (w <= 0 || h <= 0)
		return;
	if (!c->frame_dirty) {
		c->frame_dmg_x = x;
		c->frame_dmg_y = y;
		c->frame_dmg_w = w;
		c->frame_dmg_h = h;
		c->frame_dirty = 1;
		return;
	}
	x2 = x + w;
	y2 = y + h;
	if (x < c->frame_dmg_x)
		c->frame_dmg_x = x;
	if (y < c->frame_dmg_y)
		c->frame_dmg_y = y;
	if (x2 > c->frame_dmg_x + c->frame_dmg_w)
		c->frame_dmg_w = x2 - c->frame_dmg_x;
	if (y2 > c->frame_dmg_y + c->frame_dmg_h)
		c->frame_dmg_h = y2 - c->frame_dmg_y;
}

/* ══════════ Z-list maintenance ══════════ */

static void
cp_zlist_insert_top(struct cp_compositor *c, int surf_idx)
{
	int i, j;

	for (i = 0; i < (int)c->zcount; i++) {
		if (c->zlist[i] == (uint8_t)surf_idx) {
			for (j = i; j + 1 < (int)c->zcount; j++)
				c->zlist[j] = c->zlist[j + 1];
			c->zlist[c->zcount - 1] = (uint8_t)surf_idx;
			return;
		}
	}
	if (c->zcount < CP_MAX_SURFACES)
		c->zlist[c->zcount++] = (uint8_t)surf_idx;
}

static void
cp_zlist_remove(struct cp_compositor *c, int surf_idx)
{
	int i, j;

	for (i = 0; i < (int)c->zcount; i++) {
		if (c->zlist[i] == (uint8_t)surf_idx) {
			for (j = i; j + 1 < (int)c->zcount; j++)
				c->zlist[j] = c->zlist[j + 1];
			c->zcount--;
			return;
		}
	}
}

/* ══════════ Backend hooks ══════════ */

void
cp_compositor_set_backend(struct cp_compositor *c,
			  const struct cp_backend *b)
{
	(void)c;
	cp_backend = b;
}

/* ══════════ Init / client attach ══════════ */

void
cp_compositor_init(struct cp_compositor *c)
{
	int i;
	unsigned int k;

	for (k = 0; k < CP_MAX_CLIENTS; k++) {
		i = (int)k;
		c->clients[i].conn = 0;
		c->clients[i].active = 0;
		c->clients[i].client_id = 0;
		c->clients[i].protocol_errors = 0;
	}
	for (i = 0; i < CP_MAX_SURFACES; i++)
		c->surfaces[i].obj_id = 0;
	for (i = 0; i < CP_MAX_POOLS; i++)
		c->pools[i].obj_id = 0;
	for (i = 0; i < CP_MAX_BUFFERS; i++)
		c->buffers[i].obj_id = 0;
	for (i = 0; i < CP_MAX_CALLBACKS; i++)
		c->callbacks[i].obj_id = 0;
	for (i = 0; i < CP_MAX_REGIONS; i++)
		c->regions[i].obj_id = 0;
	c->frame_dirty = 0;
	c->zcount = 0;
	c->serial = 0;
	c->wm_client_id = 0;
}

int
cp_compositor_attach_client(struct cp_compositor *c, struct cp_conn *conn)
{
	int i;

	if (conn->client_id == 0 || conn->client_id > CP_MAX_CLIENTS)
		return -1;
	i = (int)(conn->client_id - 1);
	c->clients[i].conn = conn;
	c->clients[i].client_id = conn->client_id;
	c->clients[i].active = 1;
	c->clients[i].protocol_errors = 0;
	cp_objmap_init(&c->clients[i].map);
	cp_objmap_insert(&c->clients[i].map, CP_ID_DISPLAY,
			 CP_IF_DISPLAY);
	return i;
}

/* ══════════ display / registry requests ══════════ */

static void
cp_handle_display(struct cp_compositor *c, struct cp_client *cl,
		  uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_msgbuf m;

	switch (opcode) {
	case CP_DISPLAY_SYNC: {
		uint32_t cb = cp_get_u32(r);
		int i;

		for (i = 0; i < CP_MAX_CALLBACKS; i++) {
			if (c->callbacks[i].obj_id == 0) {
				c->callbacks[i].obj_id = cb;
				c->callbacks[i].client_id = cl->client_id;
				c->callbacks[i].surface_obj = 0;
				c->callbacks[i].fired = 0;
				return;
			}
		}
		break;
	}
	case CP_DISPLAY_GET_REGISTRY: {
		uint32_t reg_id = cp_get_u32(r);
		static const char *names[] = {
			"copland_compositor",
			"copland_shm",
			"copland_seat",
			"copland_output"
		};
		static const uint32_t ifaces[] = {
			CP_IF_COMPOSITOR, CP_IF_SHM,
			CP_IF_SEAT, CP_IF_OUTPUT
		};
		uint32_t k;

		if (cp_objmap_insert(&cl->map, reg_id, CP_IF_REGISTRY) < 0) {
			cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
			cp_put_u32(&m, 0);
			cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
			cp_evt_send(cl, &m);
			return;
		}
		for (k = 0; k < 4; k++) {
			cp_evt_begin(&m, reg_id, CP_REGISTRY_GLOBAL);
			cp_put_u32(&m, ifaces[k]);   /* name */
			cp_put_string(&m, names[k]);
			cp_put_u32(&m, 1);           /* version */
			cp_evt_send(cl, &m);
		}
		break;
	}
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

static void
cp_handle_registry(struct cp_compositor *c, struct cp_client *cl,
		   uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_msgbuf m;

	switch (opcode) {
	case CP_REGISTRY_BIND: {
		uint32_t name = cp_get_u32(r);
		uint32_t id = cp_get_u32(r);
		uint8_t iface = (uint8_t)name;  /* name == iface enum (v1) */

		switch (iface) {
		case CP_IF_COMPOSITOR:
		case CP_IF_SHM:
		case CP_IF_SEAT:
		case CP_IF_OUTPUT:
			if (cp_objmap_insert(&cl->map, id, iface) < 0) {
				cp_evt_begin(&m, CP_ID_DISPLAY,
					     CP_DISPLAY_ERROR);
				cp_put_u32(&m, 0);
				cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
				cp_evt_send(cl, &m);
				return;
			}
			if (iface == CP_IF_SHM) {
				cp_evt_begin(&m, id, CP_SHM_FORMAT);
				cp_put_u32(&m, 0);  /* xrgb8888 */
				cp_evt_send(cl, &m);
			} else if (iface == CP_IF_COMPOSITOR) {
				/* WM identity (spec §15): the FIRST client that
				 * binds copland_compositor owns the layout
				 * privilege (surface.set_position). */
				if (c->wm_client_id == 0)
					c->wm_client_id = cl->client_id;
			} else if (iface == CP_IF_SEAT) {
				cp_evt_begin(&m, id, CP_SEAT_CAPABILITIES);
				cp_put_u32(&m, 3);  /* pointer + keyboard */
				cp_evt_send(cl, &m);
			} else if (iface == CP_IF_OUTPUT) {
				cp_evt_begin(&m, id, CP_OUTPUT_GEOMETRY);
				cp_put_u32(&m, 0);   /* x */
				cp_put_u32(&m, 0);   /* y */
				cp_put_u32(&m, 800); /* w */
				cp_put_u32(&m, 600); /* h */
				cp_evt_send(cl, &m);
			}
			break;
		default:
			cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
			cp_put_u32(&m, name);
			cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
			cp_evt_send(cl, &m);
		}
		break;
	}
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
	(void)c;
}

/* ══════════ compositor requests ══════════ */

static void
cp_handle_compositor(struct cp_compositor *c, struct cp_client *cl,
		     uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_msgbuf m;
	int i;

	switch (opcode) {
	case CP_COMPOSITOR_CREATE_SURFACE: {
		uint32_t id = cp_get_u32(r);

		for (i = 0; i < CP_MAX_SURFACES; i++) {
			struct cp_surface_state *s;

			if (c->surfaces[i].obj_id != 0)
				continue;
			if (cp_objmap_insert(&cl->map, id,
					     CP_IF_SURFACE) < 0)
				break;
			s = &c->surfaces[i];
			s->obj_id = id;
			s->client_id = cl->client_id;
			s->x = 0;
			s->y = 0;
			s->w = 0;
			s->h = 0;
			s->buffer_obj = 0;
			s->pending_buffer = 0;
			s->dmg_w = 0;
			s->dmg_h = 0;
			s->pdmg_w = 0;
			s->pdmg_h = 0;
			s->pend_pos = 0;
			s->mapped = 0;
			s->visible = 0;
			s->has_input_region = 0;
			cp_zlist_insert_top(c, i);
			return;
		}
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_NO_MEMORY);
		cp_evt_send(cl, &m);
		break;
	}
	case CP_COMPOSITOR_CREATE_REGION: {
		uint32_t id = cp_get_u32(r);

		for (i = 0; i < CP_MAX_REGIONS; i++) {
			if (c->regions[i].obj_id != 0)
				continue;
			c->regions[i].obj_id = id;
			c->regions[i].client_id = cl->client_id;
			c->regions[i].infinite = 1;
			c->regions[i].x = 0;
			c->regions[i].y = 0;
			c->regions[i].w = 0;
			c->regions[i].h = 0;
			cp_objmap_insert(&cl->map, id, CP_IF_REGION);
			return;
		}
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_NO_MEMORY);
		cp_evt_send(cl, &m);
		break;
	}
	case CP_COMPOSITOR_RELEASE:
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

/* ══════════ shm / pool / buffer requests ══════════ */

static void
cp_handle_shm(struct cp_compositor *c, struct cp_client *cl,
	      uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_msgbuf m;
	int i;

	switch (opcode) {
	case CP_SHM_CREATE_POOL: {
		uint32_t id = cp_get_u32(r);
		uint32_t addr = cp_get_u32(r);
		uint32_t size = cp_get_u32(r);

		for (i = 0; i < CP_MAX_POOLS; i++) {
			if (c->pools[i].obj_id != 0)
				continue;
			if (cp_objmap_insert(&cl->map, id,
					     CP_IF_SHM_POOL) < 0)
				break;
			c->pools[i].obj_id = id;
			c->pools[i].client_id = cl->client_id;
			c->pools[i].addr = addr;
			c->pools[i].size = size;
			return;
		}
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_NO_MEMORY);
		cp_evt_send(cl, &m);
		break;
	}
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

static void
cp_handle_pool(struct cp_compositor *c, struct cp_client *cl,
	       uint32_t obj, uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_pool_state *p = cp_find_pool(c, obj);
	struct cp_msgbuf m;
	int i;

	if (!p) {
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
		cp_evt_send(cl, &m);
		return;
	}
	switch (opcode) {
	case CP_POOL_CREATE_BUFFER: {
		uint32_t id = cp_get_u32(r);
		int32_t off = cp_get_i32(r);
		int32_t w = cp_get_i32(r);
		int32_t h = cp_get_i32(r);
		int32_t stride = cp_get_i32(r);
		int32_t fmt = cp_get_i32(r);

		if (fmt != 0 || off < 0 || w <= 0 || h <= 0
		    || stride < w * 4
		    || (uint32_t)(off + h * stride) > p->size) {
			cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
			cp_put_u32(&m, obj);
			cp_put_u32(&m, CP_ERR_INVALID_METHOD);
			cp_evt_send(cl, &m);
			return;
		}
		for (i = 0; i < CP_MAX_BUFFERS; i++) {
			if (c->buffers[i].obj_id != 0)
				continue;
			if (cp_objmap_insert(&cl->map, id,
					     CP_IF_BUFFER) < 0)
				break;
			c->buffers[i].obj_id = id;
			c->buffers[i].client_id = cl->client_id;
			c->buffers[i].pool_obj = obj;
			c->buffers[i].offset = (uint32_t)off;
			c->buffers[i].w = w;
			c->buffers[i].h = h;
			c->buffers[i].stride = (uint32_t)stride;
			return;
		}
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_NO_MEMORY);
		cp_evt_send(cl, &m);
		break;
	}
	case CP_POOL_RESIZE:
		p->size = cp_get_u32(r);
		break;
	case CP_POOL_DESTROY:
		for (i = 0; i < CP_MAX_BUFFERS; i++) {
			if (c->buffers[i].obj_id != 0
			    && c->buffers[i].pool_obj == obj) {
				cp_objmap_remove(&cl->map,
						 c->buffers[i].obj_id);
				c->buffers[i].obj_id = 0;
			}
		}
		cp_objmap_remove(&cl->map, obj);
		p->obj_id = 0;
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

static void
cp_handle_buffer(struct cp_compositor *c, struct cp_client *cl,
		 uint32_t obj, uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_buffer_state *buf = cp_find_buffer(c, obj);
	struct cp_msgbuf m;
	int i;

	if (!buf) {
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
		cp_evt_send(cl, &m);
		return;
	}
	switch (opcode) {
	case CP_BUFFER_DESTROY:
		for (i = 0; i < CP_MAX_SURFACES; i++) {
			if (c->surfaces[i].obj_id != 0
			    && (c->surfaces[i].buffer_obj == obj
				|| c->surfaces[i].pending_buffer == obj)) {
				c->surfaces[i].buffer_obj = 0;
				c->surfaces[i].pending_buffer = 0;
			}
		}
		cp_objmap_remove(&cl->map, obj);
		buf->obj_id = 0;
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
	(void)r;
}

/* ══════════ surface requests ══════════ */

static void
cp_handle_surface(struct cp_compositor *c, struct cp_client *cl,
		  uint32_t obj, uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_surface_state *s = cp_find_surface(c, obj);
	struct cp_msgbuf m;
	int i;

	if (!s) {
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
		cp_evt_send(cl, &m);
		return;
	}
	switch (opcode) {
	case CP_SURFACE_DESTROY:
		cp_zlist_remove(c, (int)(s - c->surfaces));
		cp_frame_accum(c, s->x, s->y, s->w, s->h);
		cp_objmap_remove(&cl->map, obj);
		s->obj_id = 0;
		break;
	case CP_SURFACE_ATTACH: {
		uint32_t buf = cp_get_u32(r);

		s->pending_buffer = buf;  /* applied on COMMIT */
		break;
	}
	case CP_SURFACE_DAMAGE: {
		int32_t x = cp_get_i32(r);
		int32_t y = cp_get_i32(r);
		int32_t w = cp_get_i32(r);
		int32_t h = cp_get_i32(r);

		if (w > 0 && h > 0) {
			if (s->pdmg_w == 0) {
				s->pdmg_x = x;
				s->pdmg_y = y;
				s->pdmg_w = w;
				s->pdmg_h = h;
			} else {
				int x2 = x + w;
				int y2 = y + h;

				if (x < s->pdmg_x)
					s->pdmg_x = x;
				if (y < s->pdmg_y)
					s->pdmg_y = y;
				if (x2 > s->pdmg_x + s->pdmg_w)
					s->pdmg_w = x2 - s->pdmg_x;
				if (y2 > s->pdmg_y + s->pdmg_h)
					s->pdmg_h = y2 - s->pdmg_y;
			}
		}
		break;
	}
	case CP_SURFACE_FRAME: {
		uint32_t cb = cp_get_u32(r);

		for (i = 0; i < CP_MAX_CALLBACKS; i++) {
			if (c->callbacks[i].obj_id == 0) {
				c->callbacks[i].obj_id = cb;
				c->callbacks[i].client_id = cl->client_id;
				c->callbacks[i].surface_obj = obj;
				c->callbacks[i].fired = 0;
				return;
			}
		}
		break;
	}
	case CP_SURFACE_SET_OPAQUE_REGION:
		(void)cp_get_u32(r);   /* v1 ignores opaque regions */
		break;
	case CP_SURFACE_SET_INPUT_REGION: {
		uint32_t reg = cp_get_u32(r);

		if (reg == 0) {
			s->has_input_region = 0;
		} else {
			struct cp_region_state *rg = cp_find_region(c, reg);

			if (rg) {
				s->has_input_region = 1;
				s->in_x = rg->x;
				s->in_y = rg->y;
				s->in_w = rg->w;
				s->in_h = rg->h;
			}
		}
		break;
	}
	case CP_SURFACE_COMMIT: {
		uint32_t old = s->buffer_obj;
		int32_t old_x = s->x, old_y = s->y;

		s->buffer_obj = s->pending_buffer;
		s->pending_buffer = 0;
		if (s->buffer_obj) {
			struct cp_buffer_state *b =
				cp_find_buffer(c, s->buffer_obj);

			if (b) {
				s->w = b->w;
				s->h = b->h;
			}
			if (!s->mapped) {
				s->mapped = 1;
				s->visible = 1;
				cp_evt_begin(&m, obj, CP_SURFACE_ENTER);
				cp_put_u32(&m, 0);  /* output (v1 none) */
				cp_evt_send(cl, &m);
				cp_frame_accum(c, s->x, s->y,
					       s->w, s->h);
			}
		}
		/* Pending WM position applies atomically with the CU
		 * (spec §8 set_position: effective on next commit). */
		if (s->pend_pos) {
			s->x = s->pend_x;
			s->y = s->pend_y;
			s->pend_pos = 0;
			cp_frame_accum(c, old_x, old_y, s->w, s->h);
			cp_frame_accum(c, s->x, s->y, s->w, s->h);
		}
		if (s->pdmg_w > 0) {
			s->dmg_x = s->pdmg_x;
			s->dmg_y = s->pdmg_y;
			s->dmg_w = s->pdmg_w;
			s->dmg_h = s->pdmg_h;
			s->pdmg_w = 0;
			s->pdmg_h = 0;
		}
		if (old && old != s->buffer_obj) {
			cp_evt_begin(&m, old, CP_BUFFER_RELEASE);
			cp_evt_send(cl, &m);
		}
		break;
	}
	case CP_SURFACE_SET_POSITION: {
		int32_t x = cp_get_i32(r);
		int32_t y = cp_get_i32(r);

		/* WM-only (spec §15): the first compositor-bound
		 * client owns the layout privilege. */
		if (c->wm_client_id == 0 ||
		    c->wm_client_id != cl->client_id) {
			cp_evt_begin(&m, CP_ID_DISPLAY,
				     CP_DISPLAY_ERROR);
			cp_put_u32(&m, obj);
			cp_put_u32(&m, CP_ERR_ACCESS_DENIED);
			cp_evt_send(cl, &m);
			break;
		}
		s->pend_x = x;
		s->pend_y = y;
		s->pend_pos = 1;   /* applied on next COMMIT */
		break;
	}
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

/* ══════════ region / seat requests ══════════ */

static void
cp_handle_region(struct cp_compositor *c, struct cp_client *cl,
		 uint32_t obj, uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_region_state *rg = cp_find_region(c, obj);
	struct cp_msgbuf m;

	if (!rg) {
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
		cp_evt_send(cl, &m);
		return;
	}
	switch (opcode) {
	case CP_REGION_ADD: {
		int32_t x = cp_get_i32(r);
		int32_t y = cp_get_i32(r);
		int32_t w = cp_get_i32(r);
		int32_t h = cp_get_i32(r);

		if (rg->infinite) {
			rg->infinite = 0;
			rg->x = x;
			rg->y = y;
			rg->w = w;
			rg->h = h;
		} else {
			int x2 = x + w;
			int y2 = y + h;

			if (x < rg->x)
				rg->x = x;
			if (y < rg->y)
				rg->y = y;
			if (x2 > rg->x + rg->w)
				rg->w = x2 - rg->x;
			if (y2 > rg->y + rg->h)
				rg->h = y2 - rg->y;
		}
		break;
	}
	case CP_REGION_SUBTRACT: {
		int32_t x = cp_get_i32(r);
		int32_t y = cp_get_i32(r);
		int32_t w = cp_get_i32(r);
		int32_t h = cp_get_i32(r);

		if (!rg->infinite) {
			int cx2 = rg->x + rg->w;
			int cy2 = rg->y + rg->h;

			/* v1 approximation: full-edge holes only */
			if (y <= rg->y && y + h >= rg->y + rg->h) {
				if (x <= rg->x)
					rg->x = x + w;
				else if (x + w >= cx2)
					rg->w = x - rg->x;
			} else if (x <= rg->x && x + w >= cx2) {
				if (y <= rg->y)
					rg->y = y + h;
				else if (y + h >= cy2)
					rg->h = y - rg->y;
			}
		}
		break;
	}
	case CP_REGION_DESTROY:
		cp_objmap_remove(&cl->map, obj);
		rg->obj_id = 0;
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
}

static void
cp_handle_seat(struct cp_compositor *c, struct cp_client *cl,
	       uint32_t opcode, struct cp_msg_reader *r)
{
	struct cp_msgbuf m;

	switch (opcode) {
	case CP_SEAT_GET_POINTER:
		cp_objmap_insert(&cl->map, cp_get_u32(r),
				 CP_IF_POINTER);
		break;
	case CP_SEAT_GET_KEYBOARD:
		cp_objmap_insert(&cl->map, cp_get_u32(r),
				 CP_IF_KEYBOARD);
		break;
	case CP_SEAT_RELEASE:
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, 0);
		cp_put_u32(&m, CP_ERR_INVALID_METHOD);
		cp_evt_send(cl, &m);
	}
	(void)c;
}

/* ══════════ Dispatch ══════════ */

int
cp_compositor_dispatch(struct cp_compositor *c, struct cp_client *cl)
{
	struct cp_ring req;
	struct cp_msg_reader r;
	struct cp_msgbuf m;
	int rc;
	uint32_t obj, opcode;
	uint8_t iface;

	if (!cl || !cl->active || !cl->conn)
		return 0;
	cp_ring_init(&req, &cl->conn->req_head, &cl->conn->req_tail,
		     cl->conn->req_data, cl->conn->req_size);
	rc = cp_ring_read(&req, cp_scratch, sizeof(cp_scratch));
	if (rc <= 0)
		return rc;
	obj = (uint32_t)cp_scratch[0]
	    | ((uint32_t)cp_scratch[1] << 8)
	    | ((uint32_t)cp_scratch[2] << 16)
	    | ((uint32_t)cp_scratch[3] << 24);
	opcode = (uint32_t)cp_scratch[4]
	    | ((uint32_t)cp_scratch[5] << 8);
	iface = (uint8_t)cp_objmap_iface(&cl->map, obj);
	cp_reader_init(&r, cp_scratch, (uint32_t)rc);
	/* WM layout channel (spec §15): set_position addresses
	 * server-global surface ids, which may belong to OTHER
	 * clients and are therefore absent from the WM's object
	 * map.  Route it ahead of the per-client iface gate. */
	if (opcode == CP_SURFACE_SET_POSITION) {
		cp_handle_surface(c, cl, obj, opcode, &r);
		return 1;
	}
	switch (iface) {
	case CP_IF_DISPLAY:
		cp_handle_display(c, cl, opcode, &r);
		break;
	case CP_IF_REGISTRY:
		cp_handle_registry(c, cl, opcode, &r);
		break;
	case CP_IF_COMPOSITOR:
		cp_handle_compositor(c, cl, opcode, &r);
		break;
	case CP_IF_SHM:
		cp_handle_shm(c, cl, opcode, &r);
		break;
	case CP_IF_SHM_POOL:
		cp_handle_pool(c, cl, obj, opcode, &r);
		break;
	case CP_IF_BUFFER:
		cp_handle_buffer(c, cl, obj, opcode, &r);
		break;
	case CP_IF_SURFACE:
		cp_handle_surface(c, cl, obj, opcode, &r);
		break;
	case CP_IF_REGION:
		cp_handle_region(c, cl, obj, opcode, &r);
		break;
	case CP_IF_SEAT:
		cp_handle_seat(c, cl, opcode, &r);
		break;
	default:
		cp_evt_begin(&m, CP_ID_DISPLAY, CP_DISPLAY_ERROR);
		cp_put_u32(&m, obj);
		cp_put_u32(&m, CP_ERR_INVALID_OBJECT);
		cp_evt_send(cl, &m);
		cl->protocol_errors++;
		return -1;
	}
	return 1;
}

/* ══════════ Frame: atomic apply + composite + callbacks ══════════ */

void
cp_compositor_paint(struct cp_compositor *c)
{
	int i, zi;

	/* bottom .. top */
	for (zi = 0; zi < (int)c->zcount; zi++) {
		struct cp_surface_state *s;
		struct cp_buffer_state *b;
		struct cp_pool_state *p;

		i = c->zlist[zi];
		s = &c->surfaces[i];
		if (s->obj_id == 0 || !s->mapped || !s->visible)
			continue;
		b = cp_find_buffer(c, s->buffer_obj);
		if (!b)
			continue;
		p = cp_find_pool(c, b->pool_obj);
		if (!p)
			continue;
		if (cp_backend && cp_backend->blit)
			cp_backend->blit(s->x, s->y, s->w, s->h,
			    (const uint8_t *)(uintptr_t)(p->addr
			    + b->offset),
			    b->stride);
	}
}

void
cp_compositor_frame(struct cp_compositor *c)
{
	int i;
	struct cp_msgbuf m;
	struct cp_client *cl;

	/* 1. Composite the accumulated frame damage region. */
	if (c->frame_dirty) {
		int dx = c->frame_dmg_x;
		int dy = c->frame_dmg_y;
		int dw = c->frame_dmg_w;
		int dh = c->frame_dmg_h;

		if (cp_backend && cp_backend->wallpaper)
			cp_backend->wallpaper(dx, dy, dw, dh);
		cp_compositor_paint(c);
		c->frame_dirty = 0;
	}

	/* 2. Fire frame callbacks (serial == frame counter). */
	for (i = 0; i < CP_MAX_CALLBACKS; i++) {
		if (c->callbacks[i].obj_id == 0
		    || c->callbacks[i].fired)
			continue;
		cl = &c->clients[c->callbacks[i].client_id - 1];
		if (!cl->active)
			continue;
		c->callbacks[i].fired = 1;
		c->serial++;
		cp_evt_begin(&m, c->callbacks[i].obj_id,
			     CP_CALLBACK_DONE);
		cp_put_u32(&m, c->serial);
		cp_evt_send(cl, &m);
		c->callbacks[i].obj_id = 0;  /* one-shot */
	}

	/* 3. Clear per-surface damage (consumed by this frame). */
	for (i = 0; i < CP_MAX_SURFACES; i++)
		c->surfaces[i].dmg_w = 0;
}

/* ══════════ Input routing ══════════ */

int
cp_input_pick(int x, int y)
{
	struct cp_compositor *c = &cp_comp;
	int zi;

	/* top .. bottom */
	for (zi = (int)c->zcount - 1; zi >= 0; zi--) {
		struct cp_surface_state *s =
			&c->surfaces[c->zlist[zi]];
		int lx, ly;

		if (s->obj_id == 0 || !s->mapped || !s->visible)
			continue;
		if (x < s->x || y < s->y
		    || x >= s->x + s->w || y >= s->y + s->h)
			continue;
		lx = x - s->x;
		ly = y - s->y;
		if (s->has_input_region
		    && (lx < s->in_x || ly < s->in_y
			|| lx >= s->in_x + s->in_w
			|| ly >= s->in_y + s->in_h))
			continue;
		return c->zlist[zi];
	}
	return -1;
}
