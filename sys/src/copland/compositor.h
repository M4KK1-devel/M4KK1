/*
 * M4KK1 4P1 - compositor.c
 * Description: Copland compositor core - object dispatch, surface
 *              state, damage tracking, atomic commit, input routing.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Cleanroom implementation per docs/copland/spec-protocol.md and
 * docs/copland/spec-event-flow.md.  State-machine code only; the
 * daemon (usr/src/cmd/copland.c) provides blit/fill backend hooks so
 * this file stays buildable by the host gcc for unit tests.
 *
 * The compositor runs INSIDE the copland daemon process.  Legacy
 * sprach/terminal surfaces keep using the old copland_shm channel in
 * parallel (spec-wire-format §7 compatibility layer) while new
 * clients talk the object protocol over cp_conn rings.
 */

#ifndef _M4KK1_COPLAND_COMPOSITOR_H_
#define _M4KK1_COPLAND_COMPOSITOR_H_

#include "copland_proto.h"

/* ── Limits ── */

#define CP_MAX_SURFACES   16
#define CP_MAX_POOLS      8
#define CP_MAX_REGIONS    16
#define CP_MAX_BUFFERS    16
#define CP_MAX_CALLBACKS  16
#define CP_MAX_CLIENTS    CP_CONN_SLOTS

/* Surface buffer scale is xrgb8888 only (v1). */

/* ── Compositor-owned state per surface (committed = visible) ── */

struct cp_surface_state {
	uint32_t obj_id;      /* copland_surface object id, 0 = free */
	uint32_t client_id;   /* owning connection */
	int32_t  x, y;        /* compositor position (screen coords) */
	int32_t  w, h;        /* buffer size */
	uint32_t buffer_obj;  /* attached copland_buffer id, 0 = none */
	uint32_t pending_buffer; /* next frame's buffer (double buffer) */
	int32_t  dmg_x, dmg_y, dmg_w, dmg_h;  /* pending damage */
	int32_t  pdmg_x, pdmg_y, pdmg_w, pdmg_h; /* damage in pending CU */
	/* WM layout (spec §8/§15): set_position lands in pend_x/pend_y
	 * and is promoted to x/y atomically on the next commit. */
	int32_t  pend_x, pend_y;
	uint8_t  pend_pos;
	uint8_t  mapped;      /* has a committed buffer */
	uint8_t  visible;     /* mapped and not hidden by z-reorder */
	/* input region: 0 = whole surface accepts input */
	int32_t  in_x, in_y, in_w, in_h;
	uint8_t  has_input_region;
};

/* Pool: flat shared memory range owned by a client. */

struct cp_pool_state {
	uint32_t obj_id;
	uint32_t client_id;
	uintptr_t addr;       /* flat address of the pool memory */
	uint32_t size;        /* bytes */
};

struct cp_buffer_state {
	uint32_t obj_id;
	uint32_t client_id;
	uint32_t pool_obj;
	uint32_t offset;      /* offset inside the pool */
	int32_t  w, h;
	uint32_t stride;
};

struct cp_callback_state {
	uint32_t obj_id;
	uint32_t client_id;
	uint32_t surface_obj; /* which surface's frame, 0 = sync cb */
	uint8_t  fired;
};

struct cp_region_state {
	uint32_t obj_id;
	uint32_t client_id;
	/* v1: single rect (add/subtract keep the bounding box) */
	int32_t  x, y, w, h;
	uint8_t  infinite;
};

/* ── Per-client compositor context ── */

struct cp_client {
	struct cp_conn *conn;
	struct cp_objmap map;
	uint32_t client_id;
	uint8_t  active;
	/* error counters for diagnostics */
	uint32_t protocol_errors;
};

/* ── Compositor instance ── */

struct cp_compositor {
	struct cp_client clients[CP_MAX_CLIENTS];
	struct cp_surface_state surfaces[CP_MAX_SURFACES];
	struct cp_pool_state pools[CP_MAX_POOLS];
	struct cp_buffer_state buffers[CP_MAX_BUFFERS];
	struct cp_callback_state callbacks[CP_MAX_CALLBACKS];
	struct cp_region_state regions[CP_MAX_REGIONS];
	/* Frame accumulator: union of all damage for this frame. */
	int32_t  frame_dmg_x, frame_dmg_y, frame_dmg_w, frame_dmg_h;
	uint8_t  frame_dirty;
	/* Z-order: bottom .. top (surface indexes into surfaces[]) */
	uint8_t  zlist[CP_MAX_SURFACES];
	uint32_t zcount;
	/* Serial counter for input events. */
	uint32_t serial;
	/* WM identity (spec §15): the FIRST client that successfully
	 * binds copland_compositor owns the layout privilege.  0 = none;
	 * cleared when that client's connection is torn down. */
	uint32_t wm_client_id;
};

/* ── Backend hooks (implemented by the daemon; NULL = no-op) ── */

struct cp_backend {
	/* Blit a client buffer to the screen.  dst rect = surface
	 * position/size; src = buffer pixels (xrgb8888, stride). */
	void (*blit)(int x, int y, int w, int h,
		     const uint8_t *src, uint32_t stride);
	/* Fill a rect with a solid color. */
	void (*fill)(int x, int y, int w, int h, uint32_t argb);
	/* Wallpaper (gradient) fill for exposure. */
	void (*wallpaper)(int x, int y, int w, int h);
};

/* ── API ── */

void cp_compositor_init(struct cp_compositor *c);
void cp_compositor_set_backend(struct cp_compositor *c,
			       const struct cp_backend *b);

/* Attach a newly discovered connection (adopted via
 * cp_conn_table_poll).  Returns client index or -1. */
int  cp_compositor_attach_client(struct cp_compositor *c,
				 struct cp_conn *conn);

/* Dispatch ONE message from a client's request ring.
 * Returns:
 *   1 = message processed
 *   0 = no complete message pending
 *  -1 = protocol error (error event sent, client kept)
 */
int  cp_compositor_dispatch(struct cp_compositor *c,
			    struct cp_client *cl);

/* Flush one frame: apply all pending commit updates atomically
 * (pending buffer -> current, pending damage -> frame damage),
 * composite the frame damage region, send frame callbacks and
 * buffer releases.  Called once per daemon loop iteration. */
void cp_compositor_frame(struct cp_compositor *c);

/* Repaint ALL mapped protocol surfaces (bottom..top) regardless of
 * damage state.  The daemon calls this after the legacy composite
 * pass so new-protocol surfaces survive legacy full-screen redraws. */
void cp_compositor_paint(struct cp_compositor *c);

/* Input routing (spec-event-flow §2): find the topmost surface at
 * (x,y).  Returns surface index or -1. */
int  cp_input_pick(int x, int y);

/* Internal helpers exposed for the daemon/tests */
struct cp_surface_state *
cp_find_surface(struct cp_compositor *c, uint32_t obj_id);
struct cp_buffer_state *
cp_find_buffer(struct cp_compositor *c, uint32_t obj_id);
struct cp_pool_state *
cp_find_pool(struct cp_compositor *c, uint32_t obj_id);

/* Global compositor singleton (the daemon runs one instance). */
extern struct cp_compositor cp_comp;

#endif /* _M4KK1_COPLAND_COMPOSITOR_H_ */
