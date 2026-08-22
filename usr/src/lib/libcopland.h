/*
 * M4KK1 4P1 - libcopland.h
 * Description: Copland display server shared-memory IPC protocol.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Both the Copland daemon (/usr/bin/copland) and its clients (e.g. the
 * Sprach desktop shell) include this header.  Because M4KK1 has no MMU,
 * every user process sees the same flat physical address space, so the
 * server state lives at a fixed address (COPLAND_SHM_BASE) inside RAM
 * (QEMU default 128 MiB; 7 MiB sits between the kernel heap and the
 * user binary load zone at 0x800000) and is directly readable/writable
 * by every process.
 */

#ifndef LIBCOPLAND_H
#define LIBCOPLAND_H

#include <stdint.h>

/* ── Fixed shared-memory region ── */

#define COPLAND_SHM_BASE      0x00700000u
#define COPLAND_SHM_MAGIC     0x434F504Cu   /* 'COPL' */
#define COPLAND_VERSION       1

/* ── Limits ── */

#define COPLAND_MAX_SURFACES  16
#define COPLAND_CMD_RING_SIZE 32

/* ── Terminal input mailbox ──
 *
 * The Sprach WM owns the keyboard (it polls m4k_get_keyboard_event in
 * its main loop, which is the only sane routing point in a single
 * address-space kernel without a TTY layer).  When the active window
 * is the terminal, Sprach forwards every keystroke into this fixed
 * shared ring; the /bin/terminal process consumes it.  Single
 * producer (Sprach) / single consumer (terminal) under cooperative
 * scheduling, so no locks are required.  Address sits just above the
 * Copland shared region, in free user RAM. */

#define TERM_MAILBOX_BASE   0x00710000u
#define TERM_MAILBOX_MAGIC  0x5445524Du   /* 'TERM' */
#define TERM_MAILBOX_SIZE   256

struct term_mailbox {
    uint32_t magic;        /* TERM_MAILBOX_MAGIC when the terminal lives */
    uint32_t write_idx;    /* producer: Sprach */
    uint32_t read_idx;     /* consumer: /bin/terminal */
    uint8_t  buf[TERM_MAILBOX_SIZE];
};

/* ── Surface flags ── */

#define COPLAND_SURF_VISIBLE  0x00000001u

/* ── Window surface ── */

struct copland_surface {
    uint32_t in_use;      /* 1 = slot occupied */
    int32_t  x, y;        /* top-left on screen */
    int32_t  w, h;        /* size in pixels */
    uint32_t buffer_ptr;  /* client pixel buffer (flat addr), 0 = none */
    uint32_t color;       /* fill color (BGRA) */
    uint32_t flags;       /* COPLAND_SURF_* */
    /* Incremental damage rect (screen coords, half-open union/
     * intersect rectangle semantics).  Set by the client after
     * writing its
     * buffer; Copland unions all pending damage and re-composites
     * only that region (m4k_flip_rect), then clears it.  dmg_w = 0
     * means no pending damage. */
    int32_t  dmg_x, dmg_y;
    int32_t  dmg_w, dmg_h;
};

/* ── IPC commands ── */

enum copland_cmd {
    COPLAND_CMD_NONE = 0,
    COPLAND_CMD_CREATE_SURFACE,   /* args: x, y, w, h, color, flags */
    COPLAND_CMD_MOVE_SURFACE,     /* args: id, x, y */
    COPLAND_CMD_RESIZE_SURFACE,   /* args: id, w, h */
    COPLAND_CMD_REDRAW_SURFACE,   /* args: id, color */
    COPLAND_CMD_DESTROY_SURFACE,  /* args: id */
    COPLAND_CMD_STATUS            /* args: (none) -> ready flag */
};

struct copland_command {
    uint32_t cmd;         /* enum copland_cmd */
    int32_t  args[6];
};

/* ── Shared server state ── */

struct copland_shm {
    uint32_t magic;               /* COPLAND_SHM_MAGIC */
    uint32_t version;             /* COPLAND_VERSION */
    uint32_t ready;               /* 1 = server initialized */
    uint32_t heartbeat;           /* bumped by the WM client (watchdog) */
    uint32_t surface_count;       /* number of live surfaces */
    uint32_t dirty;               /* 1 = scene changed, composite+flip needed */
    uint32_t shutdown;            /* 1 = WM requests session end: the
                                     server exits so MDM regains the
                                     screen (lock/logout/shutdown) */

    struct copland_surface
        surfaces[COPLAND_MAX_SURFACES];

    uint32_t cmd_write_idx;       /* producer (client) side */
    uint32_t cmd_read_idx;        /* consumer (server) side */
    struct copland_command
        cmd_ring[COPLAND_CMD_RING_SIZE];
};

/* ── Accessors ── */

static inline struct copland_shm *copland_shm_get(void)
{
    return (struct copland_shm *)COPLAND_SHM_BASE;
}

/* Find a free surface slot; returns index or -1.  The caller fills
 * the geometry afterwards — in_use is set to 1 HERE only because the
 * compositor (same process for CREATE commands) is the one filling
 * it; cross-process creators must fill fields first and publish
 * in_use LAST themselves (see Sprach's create_launchpad). */
static inline int copland_surface_alloc(struct copland_shm *shm)
{
    int i;
    for (i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (!shm->surfaces[i].in_use) {
            shm->surfaces[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

/* Push one command into the ring; returns 0 on success, -1 when full. */
static inline int copland_cmd_push(struct copland_shm *shm, uint32_t cmd,
                                   int32_t a0, int32_t a1, int32_t a2,
                                   int32_t a3, int32_t a4, int32_t a5)
{
    uint32_t next = (shm->cmd_write_idx + 1) & (COPLAND_CMD_RING_SIZE - 1);
    if (next == shm->cmd_read_idx)
        return -1;
    struct copland_command *c = &shm->cmd_ring[shm->cmd_write_idx];
    c->cmd = cmd;
    c->args[0] = a0; c->args[1] = a1; c->args[2] = a2;
    c->args[3] = a3; c->args[4] = a4; c->args[5] = a5;
    shm->cmd_write_idx = next;
    return 0;
}

/* Pop one command; returns 1 if a command was returned, else 0. */
static inline int copland_cmd_pop(struct copland_shm *shm,
                                  struct copland_command *out)
{
    if (shm->cmd_read_idx == shm->cmd_write_idx)
        return 0;
    *out = shm->cmd_ring[shm->cmd_read_idx];
    shm->cmd_read_idx =
        (shm->cmd_read_idx + 1) & (COPLAND_CMD_RING_SIZE - 1);
    return 1;
}

#endif /* LIBCOPLAND_H */
