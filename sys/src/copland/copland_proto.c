/*
 * M4KK1 4P1 - copland_proto.c
 * Description: Copland protocol layer - serialization, SPSC rings,
 *              object map, connection discovery.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Cleanroom implementation per docs/copland/spec-wire-format.md.
 * Pure computation only - no syscalls, safe to link anywhere and to
 * compile with the host gcc for unit tests.
 */

#include "copland_proto.h"

/* ══════════ Message builder ══════════ */

void
cp_msg_start(struct cp_msgbuf *m, uint8_t *mem, uint32_t cap,
	     uint32_t obj, uint32_t opcode)
{
	m->b = mem;
	m->cap = cap;
	m->len = CP_MSG_HDR;      /* reserve the 8B header */
	m->obj = obj;
	m->op = opcode & 0xFFFFu;
}

void
cp_put_u32(struct cp_msgbuf *m, uint32_t v)
{
	if (m->len + 4u > m->cap)
		return;               /* cp_msg_finish reports failure */
	m->b[m->len] = (uint8_t)(v & 0xFFu);
	m->b[m->len + 1u] = (uint8_t)((v >> 8) & 0xFFu);
	m->b[m->len + 2u] = (uint8_t)((v >> 16) & 0xFFu);
	m->b[m->len + 3u] = (uint8_t)((v >> 24) & 0xFFu);
	m->len += 4u;
}

void
cp_put_i32(struct cp_msgbuf *m, int32_t v)
{
	cp_put_u32(m, (uint32_t)v);
}

int
cp_put_string(struct cp_msgbuf *m, const char *s)
{
	uint32_t n = 0;
	uint32_t pad;

	if (s == 0) {
		cp_put_u32(m, 0);
		return 0;
	}
	while (s[n] != 0)
		n++;
	/* length counts content + NUL, padded to a 4B multiple */
	cp_put_u32(m, n + 1u);
	pad = ((n + 1u) + 3u) & ~3u;
	if (m->len + pad > m->cap)
		return -1;
	{
		uint32_t i;
		for (i = 0; i < pad; i++)
			m->b[m->len + i] =
				(i < n) ? (uint8_t)s[i] : 0;
	}
	m->len += pad;
	return 0;
}

int
cp_msg_finish(struct cp_msgbuf *m)
{
	if (m->len > CP_MSG_MAX)
		return -1;
	/* header: word0 object-id, word1 size<<16 | opcode */
	m->b[0] = (uint8_t)(m->obj & 0xFFu);
	m->b[1] = (uint8_t)((m->obj >> 8) & 0xFFu);
	m->b[2] = (uint8_t)((m->obj >> 16) & 0xFFu);
	m->b[3] = (uint8_t)((m->obj >> 24) & 0xFFu);
	m->b[4] = (uint8_t)(m->op & 0xFFu);
	m->b[5] = (uint8_t)((m->op >> 8) & 0xFFu);
	m->b[6] = (uint8_t)(m->len & 0xFFu);
	m->b[7] = (uint8_t)((m->len >> 8) & 0xFFu);
	return (int)m->len;
}

/* ══════════ Message reader ══════════ */

void
cp_reader_init(struct cp_msg_reader *r, const uint8_t *msg,
	       uint32_t size)
{
	if (size < CP_MSG_HDR) {
		r->p = 0;
		r->left = 0;
		return;
	}
	r->p = msg + CP_MSG_HDR;
	r->left = size - CP_MSG_HDR;
}

uint32_t
cp_get_u32(struct cp_msg_reader *r)
{
	uint32_t v;

	if (r->left < 4u) {
		r->left = 0;
		return 0;
	}
	v = (uint32_t)r->p[0]
	    | ((uint32_t)r->p[1] << 8)
	    | ((uint32_t)r->p[2] << 16)
	    | ((uint32_t)r->p[3] << 24);
	r->p += 4;
	r->left -= 4u;
	return v;
}

int32_t
cp_get_i32(struct cp_msg_reader *r)
{
	return (int32_t)cp_get_u32(r);
}

int
cp_get_string(struct cp_msg_reader *r, char *dst, uint32_t dstcap)
{
	uint32_t wl;
	uint32_t i;

	if (r->left < 4u) {
		r->left = 0;
		return -1;
	}
	wl = (uint32_t)r->p[0]
	     | ((uint32_t)r->p[1] << 8)
	     | ((uint32_t)r->p[2] << 16)
	     | ((uint32_t)r->p[3] << 24);
	r->p += 4;
	r->left -= 4u;
	if (wl == 0) {
		if (dstcap > 0)
			dst[0] = 0;
		return 0;
	}
	if (r->left < wl)
		return -1;
	if (dstcap < wl) {
		/* consume but report truncation */
		r->p += wl;
		r->left -= wl;
		return -1;
	}
	for (i = 0; i < wl; i++)
		dst[i] = (char)r->p[i];
	dst[wl - 1u] = 0;          /* enforce NUL termination */
	r->p += wl;
	r->left -= wl;
	/* swallow pad to 4B multiple (length includes NUL already) */
	{
		uint32_t pad = (wl + 3u) & ~3u;
		if (pad > wl) {
			uint32_t skip = pad - wl;
			if (r->left >= skip) {
				r->p += skip;
				r->left -= skip;
			}
		}
	}
	return (int)wl;
}

/* ══════════ SPSC byte ring ══════════ */

void
cp_ring_init(struct cp_ring *r, uint32_t *head, uint32_t *tail,
	     uint8_t *data, uint32_t size)
{
	r->head = head;
	r->tail = tail;
	r->data = data;
	r->size = size;
}

uint32_t
cp_ring_used(const struct cp_ring *r)
{
	return *r->tail - *r->head;
}

uint32_t
cp_ring_free(const struct cp_ring *r)
{
	return r->size - cp_ring_used(r);
}

int
cp_ring_write(struct cp_ring *r, const uint8_t *src, uint32_t len)
{
	uint32_t used;
	uint32_t off;
	uint32_t i;

	if (len > r->size)
		return -1;
	used = cp_ring_used(r);
	if (r->size - used < len)
		return -1;
	off = *r->tail & (r->size - 1u);
	for (i = 0; i < len; i++) {
		r->data[(off + i) & (r->size - 1u)] = src[i];
	}
	*r->tail += len;            /* publish */
	return (int)len;
}

int
cp_ring_read(struct cp_ring *r, uint8_t *dst, uint32_t cap)
{
	uint32_t used;
	uint32_t size;
	uint32_t off;
	uint32_t i;

	used = cp_ring_used(r);
	if (used < CP_MSG_HDR)
		return 0;
	off = *r->head & (r->size - 1u);
	/* header word1 (size<<16 | opcode) lives at offset 4..7 */
	{
		uint32_t w1 = (uint32_t)r->data[(off + 4u) & (r->size - 1u)]
		    | ((uint32_t)r->data[(off + 5u) & (r->size - 1u)] << 8)
		    | ((uint32_t)r->data[(off + 6u) & (r->size - 1u)] << 16)
		    | ((uint32_t)r->data[(off + 7u) & (r->size - 1u)] << 24);
		size = w1 >> 16;
		if (size < CP_MSG_HDR || size > CP_MSG_MAX
		    || (size & 3u) != 0)
			return -1;
		if (used < size)
			return 0;       /* partial message, wait */
		if (cap < size)
			return -2;
	}
	for (i = 0; i < size; i++)
		dst[i] = r->data[(off + i) & (r->size - 1u)];
	*r->head += size;           /* consume */
	return (int)size;
}

/* ══════════ Object map ══════════ */

void
cp_objmap_init(struct cp_objmap *m)
{
	uint32_t i;

	for (i = 0; i < CP_OBJMAP_MAX; i++) {
		m->id[i] = 0;
		m->iface[i] = 0;
		m->used[i] = 0;
	}
	m->next_id = 2;              /* 1 is the display singleton */
}

uint32_t
cp_objmap_new_id(struct cp_objmap *m)
{
	uint32_t id = m->next_id;

	if (m->next_id < CP_ID_MAX_CLIENT)
		m->next_id++;
	return id;
}

int
cp_objmap_insert(struct cp_objmap *m, uint32_t id, uint8_t iface)
{
	uint32_t i;
	int free_slot = -1;

	for (i = 0; i < CP_OBJMAP_MAX; i++) {
		if (m->used[i] && m->id[i] == id)
			return -1;      /* duplicate */
		if (!m->used[i] && free_slot < 0)
			free_slot = (int)i;
	}
	if (free_slot < 0)
		return -1;
	m->id[free_slot] = id;
	m->iface[free_slot] = iface;
	m->used[free_slot] = 1;
	return free_slot;
}

int
cp_objmap_iface(const struct cp_objmap *m, uint32_t id)
{
	uint32_t i;

	for (i = 0; i < CP_OBJMAP_MAX; i++) {
		if (m->used[i] && m->id[i] == id)
			return m->iface[i];
	}
	return CP_IF_NONE;
}

int
cp_objmap_remove(struct cp_objmap *m, uint32_t id)
{
	uint32_t i;

	for (i = 0; i < CP_OBJMAP_MAX; i++) {
		if (m->used[i] && m->id[i] == id) {
			m->used[i] = 0;
			m->id[i] = 0;
			return 0;
		}
	}
	return -1;
}

/* ══════════ Connection discovery ══════════ */

static struct cp_conn_table *
cp_table_get(void)
{
	return (struct cp_conn_table *)COPLAND_CONN_BASE;
}

void
cp_conn_table_init(void)
{
	struct cp_conn_table *t = cp_table_get();
	uint32_t i;

	for (i = 0; i < CP_CONN_SLOTS; i++) {
		t->slots[i].in_use = 0;
		t->slots[i].client_pid = 0;
		t->slots[i].conn_addr = 0;
		t->slots[i].pad = 0;
	}
	t->slot_count = CP_CONN_SLOTS;
	t->version = COPLAND_PROTO_VERSION;
	t->magic = COPLAND_CONN_MAGIC;      /* publish last */
}

int
cp_conn_table_poll(struct cp_conn *out[], int max)
{
	struct cp_conn_table *t = cp_table_get();
	uint32_t i;
	int n = 0;
	static uint32_t next_client_id = 1;

	if (t->magic != COPLAND_CONN_MAGIC)
		return 0;
	for (i = 0; i < CP_CONN_SLOTS && n < max; i++) {
		struct cp_conn_slot *s = &t->slots[i];
		struct cp_conn *c;

		if (!s->in_use || s->conn_addr == 0)
			continue;
		c = (struct cp_conn *)(uintptr_t)s->conn_addr;
		if (c->magic != COPLAND_CONN_MAGIC
		    || c->version != COPLAND_PROTO_VERSION)
			continue;       /* not ready yet */
		if (c->client_id != 0)
			continue;       /* already adopted */
		c->client_id = next_client_id++;
		c->server_pid = 0;  /* filled by daemon wrapper */
		out[n++] = c;
	}
	return n;
}

void
cp_client_conn_init(struct cp_conn *conn)
{
	uint32_t i;

	conn->magic = 0;                 /* not yet published */
	conn->version = COPLAND_PROTO_VERSION;
	conn->client_id = 0;
	conn->req_head = 0;
	conn->req_tail = 0;
	conn->req_size = CP_REQ_RING_SIZE;
	for (i = 0; i < CP_REQ_RING_SIZE; i += 4u) {
		conn->req_data[i] = 0;
	}
	conn->evt_head = 0;
	conn->evt_tail = 0;
	conn->evt_size = CP_EVT_RING_SIZE;
	conn->server_pid = 0;
	conn->client_pid = 0;
	conn->disconnect = 0;
	conn->pad = 0;
	conn->magic = COPLAND_CONN_MAGIC; /* publish last */
}

int
cp_client_connect(struct cp_conn *conn, uint32_t client_pid)
{
	struct cp_conn_table *t = cp_table_get();
	uint32_t i;

	if (t->magic != COPLAND_CONN_MAGIC
	    || conn->magic != COPLAND_CONN_MAGIC)
		return -1;
	conn->client_pid = client_pid;
	for (i = 0; i < CP_CONN_SLOTS; i++) {
		if (!t->slots[i].in_use) {
			t->slots[i].client_pid = client_pid;
			t->slots[i].conn_addr = (uint32_t)(uintptr_t)conn;
			t->slots[i].pad = 0;
			t->slots[i].in_use = 1;    /* publish last */
			return 0;
		}
	}
	return -1;                          /* table full */
}
