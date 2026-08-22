/*
 * M4KK1 4P1 - copland_proto.h
 * Description: Copland display-server protocol layer - public API.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Cleanroom implementation of docs/copland/spec-protocol.md and
 * docs/copland/spec-wire-format.md (phase 0 specs).  Pure C, no
 * syscalls, no libc: linked into the Copland daemon and every
 * Copland client, and buildable by the host gcc for unit tests.
 *
 * Layout (single-pass compiler friendly - no flex arrays):
 *   message wire format : 8B header (object-id, size<<16|opcode)
 *   transport           : SPSC byte rings inside struct cp_conn
 *   discovery           : connection table at COPLAND_CONN_BASE
 */

#ifndef _M4KK1_COPLAND_PROTO_H_
#define _M4KK1_COPLAND_PROTO_H_

#include <stdint.h>

/* ── Fixed shared addresses (see spec-wire-format §4.2) ── */

#define COPLAND_CONN_BASE      0x00720000u
#define COPLAND_CONN_MAGIC     0x43434F4Eu   /* 'CCON' */
#define COPLAND_CTBL_MAGIC     0x4354424Cu   /* 'CTBL' */
#define COPLAND_PROTO_VERSION  1u

/* ── Wire constants (spec-wire-format §2) ── */

#define CP_MSG_HDR        8u
#define CP_MSG_MAX        4088u

/* ── Interfaces (spec-protocol §1) ── */

enum cp_interface {
	CP_IF_NONE = 0,
	CP_IF_DISPLAY = 1,
	CP_IF_REGISTRY,
	CP_IF_CALLBACK,
	CP_IF_COMPOSITOR,
	CP_IF_SHM,
	CP_IF_SHM_POOL,
	CP_IF_BUFFER,
	CP_IF_SURFACE,
	CP_IF_REGION,
	CP_IF_SEAT,
	CP_IF_POINTER,
	CP_IF_KEYBOARD,
	CP_IF_OUTPUT
};

/* ── Object ID rules (spec-wire-format §2.x) ── */

#define CP_ID_DISPLAY        1u
#define CP_SERVER_ID_START   0xFF000000u
#define CP_ID_MAX_CLIENT     0xFEFFFFFFu

/* ── Opcodes per interface (spec-protocol §2-13) ── */

enum cp_display_req {
	CP_DISPLAY_SYNC = 0,
	CP_DISPLAY_GET_REGISTRY = 1
};
enum cp_display_evt {
	CP_DISPLAY_ERROR = 0,
	CP_DISPLAY_DELETE_ID = 1
};
enum cp_registry_req {
	CP_REGISTRY_BIND = 0
};
enum cp_registry_evt {
	CP_REGISTRY_GLOBAL = 0,
	CP_REGISTRY_GLOBAL_REMOVE = 1
};
enum cp_callback_evt {
	CP_CALLBACK_DONE = 0
};
enum cp_compositor_req {
	CP_COMPOSITOR_CREATE_SURFACE = 0,
	CP_COMPOSITOR_CREATE_REGION = 1,
	CP_COMPOSITOR_RELEASE = 2
};
enum cp_shm_req {
	CP_SHM_CREATE_POOL = 0,
	CP_SHM_RELEASE = 1
};
enum cp_shm_evt {
	CP_SHM_FORMAT = 0
};
enum cp_pool_req {
	CP_POOL_CREATE_BUFFER = 0,
	CP_POOL_RESIZE = 1,
	CP_POOL_DESTROY = 2
};
enum cp_buffer_req {
	CP_BUFFER_DESTROY = 0
};
enum cp_buffer_evt {
	CP_BUFFER_RELEASE = 0
};
enum cp_surface_req {
	CP_SURFACE_DESTROY = 0,
	CP_SURFACE_ATTACH = 1,
	CP_SURFACE_DAMAGE = 2,
	CP_SURFACE_FRAME = 3,
	CP_SURFACE_SET_OPAQUE_REGION = 4,
	CP_SURFACE_SET_INPUT_REGION = 5,
	CP_SURFACE_COMMIT = 6
};
enum cp_surface_evt {
	CP_SURFACE_ENTER = 0,
	CP_SURFACE_LEAVE = 1
};
enum cp_region_req {
	CP_REGION_DESTROY = 0,
	CP_REGION_ADD = 1,
	CP_REGION_SUBTRACT = 2
};
enum cp_seat_req {
	CP_SEAT_GET_POINTER = 0,
	CP_SEAT_GET_KEYBOARD = 1,
	CP_SEAT_RELEASE = 2
};
enum cp_seat_evt {
	CP_SEAT_CAPABILITIES = 0
};
enum cp_pointer_req {
	CP_POINTER_SET_CURSOR = 0,
	CP_POINTER_RELEASE = 1
};
enum cp_pointer_evt {
	CP_POINTER_ENTER = 0,
	CP_POINTER_LEAVE = 1,
	CP_POINTER_MOTION = 2,
	CP_POINTER_BUTTON = 3,
	CP_POINTER_AXIS = 4
};
enum cp_keyboard_req {
	CP_KEYBOARD_RELEASE = 0
};
enum cp_keyboard_evt {
	CP_KEYBOARD_ENTER = 0,
	CP_KEYBOARD_LEAVE = 1,
	CP_KEYBOARD_KEY = 2,
	CP_KEYBOARD_MODIFIERS = 3
};
enum cp_output_req {
	CP_OUTPUT_RELEASE = 0
};
enum cp_output_evt {
	CP_OUTPUT_GEOMETRY = 0,
	CP_OUTPUT_MODE = 1,
	CP_OUTPUT_SCALE = 2,
	CP_OUTPUT_DONE = 3
};

/* ── Global error codes (spec-protocol §2) ── */

#define CP_ERR_INVALID_OBJECT   0u
#define CP_ERR_INVALID_METHOD   1u
#define CP_ERR_NO_MEMORY        2u
#define CP_ERR_IMPLEMENTATION   3u

/* ══════════ Message builder (producer side) ══════════ */

struct cp_msgbuf {
	uint8_t *b;        /* backing store */
	uint32_t cap;      /* capacity bytes */
	uint32_t len;      /* current bytes used */
	uint32_t obj;      /* target object id */
	uint32_t op;       /* opcode */
};

void cp_msg_start(struct cp_msgbuf *m, uint8_t *mem, uint32_t cap,
		  uint32_t obj, uint32_t opcode);
void cp_put_u32(struct cp_msgbuf *m, uint32_t v);
void cp_put_i32(struct cp_msgbuf *m, int32_t v);
int  cp_put_string(struct cp_msgbuf *m, const char *s); /* NULL -> len 0 */
int  cp_msg_finish(struct cp_msgbuf *m);                /* backfill hdr;
							   returns len or -1 */

/* ══════════ Message reader (consumer side) ══════════ */

struct cp_msg_reader {
	const uint8_t *p;
	uint32_t left;
};

void cp_reader_init(struct cp_msg_reader *r, const uint8_t *msg,
		    uint32_t size);   /* size includes 8B header */
uint32_t cp_get_u32(struct cp_msg_reader *r);
int32_t  cp_get_i32(struct cp_msg_reader *r);
/* Copies string (without NUL guarantee by wire; adds NUL) into dst
 * of dstcap bytes.  Returns wire length or -1 on overflow/trunc. */
int      cp_get_string(struct cp_msg_reader *r, char *dst,
		       uint32_t dstcap);

/* ══════════ SPSC byte ring (spec-wire-format §4.1) ══════════ */

struct cp_ring {
	uint32_t *head;    /* consumer advances (monotonic byte count) */
	uint32_t *tail;    /* producer advances (monotonic byte count) */
	uint8_t  *data;    /* ring storage, size bytes */
	uint32_t  size;    /* capacity bytes, power of two */
};

void     cp_ring_init(struct cp_ring *r, uint32_t *head, uint32_t *tail,
		      uint8_t *data, uint32_t size);
uint32_t cp_ring_used(const struct cp_ring *r);
uint32_t cp_ring_free(const struct cp_ring *r);
/* Returns byte count written, or -1 when the ring is too full. */
int      cp_ring_write(struct cp_ring *r, const uint8_t *src,
		       uint32_t len);
/* Reads one COMPLETE message into dst.  Returns message size,
 * 0 when no complete message is pending, -1 malformed header,
 * -2 dst too small (message left in ring). */
int      cp_ring_read(struct cp_ring *r, uint8_t *dst, uint32_t cap);

/* ══════════ Per-client object map (spec-wire-format §6) ══════════ */

#define CP_OBJMAP_MAX 64

struct cp_objmap {
	uint32_t id[CP_OBJMAP_MAX];
	uint8_t  iface[CP_OBJMAP_MAX];
	uint8_t  used[CP_OBJMAP_MAX];
	uint32_t next_id;   /* next client-side id to hand out */
};

void     cp_objmap_init(struct cp_objmap *m);
uint32_t cp_objmap_new_id(struct cp_objmap *m);
int      cp_objmap_insert(struct cp_objmap *m, uint32_t id,
			  uint8_t iface);
int      cp_objmap_iface(const struct cp_objmap *m, uint32_t id);
int      cp_objmap_remove(struct cp_objmap *m, uint32_t id);

/* ══════════ Transport: connection + discovery table ══════════ */

#define CP_REQ_RING_SIZE 4096u
#define CP_EVT_RING_SIZE 4096u
#define CP_CONN_SLOTS    8u

struct cp_conn {
	uint32_t magic;
	uint32_t version;
	uint32_t client_id;    /* server assigned, from 1 */
	/* request ring: client -> server */
	uint32_t req_head;     /* server advances */
	uint32_t req_tail;     /* client advances */
	uint32_t req_size;
	uint8_t  req_data[CP_REQ_RING_SIZE];
	/* event ring: server -> client */
	uint32_t evt_head;     /* client advances */
	uint32_t evt_tail;     /* server advances */
	uint32_t evt_size;
	uint8_t  evt_data[CP_EVT_RING_SIZE];
	/* auxiliary */
	uint32_t server_pid;
	uint32_t client_pid;
	uint32_t disconnect;   /* 1 = client requests teardown */
	uint32_t pad;
};

struct cp_conn_slot {
	uint32_t in_use;
	uint32_t client_pid;
	uint32_t conn_addr;    /* flat address of struct cp_conn */
	uint32_t pad;
};

struct cp_conn_table {
	uint32_t magic;        /* COPLAND_CTBL_MAGIC */
	uint32_t version;
	uint32_t slot_count;   /* CP_CONN_SLOTS */
	struct cp_conn_slot slots[CP_CONN_SLOTS];
};

/* Server: zero + publish the table at COPLAND_CONN_BASE. */
void cp_conn_table_init(void);

/* Server: adopt newly registered connections into out[] (pointers
 * into the flat address space).  Returns how many were adopted this
 * call.  Assigns client_id, stamps server_pid. */
int  cp_conn_table_poll(struct cp_conn *out[], int max);

/* Client: publish the caller-owned (static, zeroed) conn and
 * register it in a free table slot.  conn rings must be initialized
 * by the caller first (cp_client_conn_init).  Returns 0 or -1 when
 * the table is full/invalid. */
int  cp_client_connect(struct cp_conn *conn, uint32_t client_pid);

/* Client helper: initialize a static conn to valid empty state. */
void cp_client_conn_init(struct cp_conn *conn);

#endif /* _M4KK1_COPLAND_PROTO_H_ */
