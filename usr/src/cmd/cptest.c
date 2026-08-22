/*
 * M4KK1 4P1 - cptest.c
 * Description: Copland protocol smoke-test client (phase 2).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Exercises the new object protocol end-to-end inside QEMU:
 * connect -> get_registry -> bind compositor/shm -> create pool
 * -> create buffer -> create surface -> attach -> damage -> commit
 * -> frame callback -> draw pattern -> commit again.
 *
 * Prints CPTEST lines on the serial console for the smoke script.
 */

#include "../lib/libgui.h"
#include "../../../sys/src/copland/copland_proto.h"

/* Global variables required by m4sh.h functions */
int out_fd = 1;
char cwd[256] = "/";

/* Static protocol state (client-owned, published by address). */
static struct cp_conn conn;
static struct cp_objmap map;
static uint32_t pool_mem[128 * 128];   /* 64KB pixel pool */

/* Shared client-side helpers (mirrors of daemon-side send paths). */

static uint8_t reqbuf[CP_MSG_MAX];
static struct cp_msgbuf mb;

static void send_req(uint32_t obj, uint32_t op)
{
    struct cp_ring r;

    if (cp_msg_finish(&mb) < 0) {
        ser_puts("[CPTEST] marshal failed\n");
        return;
    }
    cp_ring_init(&r, &conn.req_head, &conn.req_tail,
                 conn.req_data, conn.req_size);
    if (cp_ring_write(&r, reqbuf, mb.len) < 0)
        ser_puts("[CPTEST] req ring full!\n");
}

static void req_begin(uint32_t obj, uint32_t op)
{
    cp_msg_start(&mb, reqbuf, sizeof(reqbuf), obj, op);
}

static int evt_pop(uint32_t *obj, uint32_t *op, uint32_t *a0)
{
    uint8_t buf[CP_MSG_MAX];
    struct cp_ring r;
    int rc;

    cp_ring_init(&r, &conn.evt_head, &conn.evt_tail,
                 conn.evt_data, conn.evt_size);
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
    }
    return rc;
}

/* Drain events; returns 1 when an expected (obj,op) pair arrived. */
static int wait_event(uint32_t obj, uint32_t op, int spins)
{
    uint32_t eo, ep, ea;
    int s;

    for (s = 0; s < spins; s++) {
        while (evt_pop(&eo, &ep, &ea) > 0) {
            if (eo == obj && ep == op)
                return 1;
        }
        m4k_yield();
    }
    return 0;
}

void _start(void)
{
    uint32_t eo, ep, ea;
    uint32_t id_registry, id_comp, id_shm, id_pool, id_buf, id_surf;
    uint32_t id_cb;
    int i;
    struct m4k_framebuffer_info fb;
    uint32_t *fb_pixels = 0;

    /* Serial dies early in the boot chain (MDM-era); use the
     * framebuffer as a status beacon so headless smoke tests can
     * see how far the handshake got.  Beacon column at x=620,
     * one 8px row per milestone, colors fixed. */
    if (m4k_get_framebuffer_info(&fb) == 0)
        fb_pixels = (uint32_t *)(uintptr_t)fb.phys_addr;

    ser_puts("[CPTEST] copland protocol smoke client\n");
    cp_objmap_init(&map);
    cp_client_conn_init(&conn);
    /* milestone beacons: milestone(n) paints row n of the beacon
     * column.  Smoke tests scan x=620, y=290..310 headlessly. */
#define BEACON_X 620
#define BEACON_Y0 290
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 0) * fb.width + BEACON_X] = 0xFF00FF00u;
    if (cp_client_connect(&conn, 0) < 0) {
        ser_puts("[CPTEST] FAIL: conn table full/missing\n");
        if (fb_pixels)
            fb_pixels[(BEACON_Y0 + 1) * fb.width + BEACON_X] = 0xFFFF0000u;
        m4k_exit(1);
    }
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 0) * fb.width + BEACON_X] = 0xFF00FF00u;
    ser_puts("[CPTEST] connected, waiting adoption...\n");

    /* 1. registry */
    id_registry = cp_objmap_new_id(&map);
    req_begin(CP_ID_DISPLAY, CP_DISPLAY_GET_REGISTRY);
    cp_put_u32(&mb, id_registry);
    send_req(CP_ID_DISPLAY, CP_DISPLAY_GET_REGISTRY);
    if (!wait_event(id_registry, CP_REGISTRY_GLOBAL, 4000)) {
        ser_puts("[CPTEST] FAIL: no registry.global\n");
        m4k_exit(1);
    }
    ser_puts("[CPTEST] PASS: registry.global received\n");
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 2) * fb.width + BEACON_X] = 0xFF00FFFFu;
    /* drain remaining globals */
    while (evt_pop(&eo, &ep, &ea) > 0)
        ;

    /* 2. bind compositor + shm (name == iface enum, v1) */
    id_comp = cp_objmap_new_id(&map);
    req_begin(2, CP_REGISTRY_BIND);
    cp_put_u32(&mb, CP_IF_COMPOSITOR);
    cp_put_u32(&mb, id_comp);
    send_req(2, CP_REGISTRY_BIND);
    id_shm = cp_objmap_new_id(&map);
    req_begin(2, CP_REGISTRY_BIND);
    cp_put_u32(&mb, CP_IF_SHM);
    cp_put_u32(&mb, id_shm);
    send_req(2, CP_REGISTRY_BIND);
    if (!wait_event(id_shm, CP_SHM_FORMAT, 4000)) {
        ser_puts("[CPTEST] FAIL: no shm.format\n");
        m4k_exit(1);
    }
    ser_puts("[CPTEST] PASS: shm bound + format event\n");
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 4) * fb.width + BEACON_X] = 0xFFFFFF00u;
    while (evt_pop(&eo, &ep, &ea) > 0)
        ;

    /* 3. pool + buffer (128x128) */
    id_pool = cp_objmap_new_id(&map);
    req_begin(id_shm, CP_SHM_CREATE_POOL);
    cp_put_u32(&mb, id_pool);
    cp_put_u32(&mb, (uint32_t)(uintptr_t)pool_mem);
    cp_put_u32(&mb, sizeof(pool_mem));
    send_req(id_shm, CP_SHM_CREATE_POOL);
    id_buf = cp_objmap_new_id(&map);
    req_begin(id_pool, CP_POOL_CREATE_BUFFER);
    cp_put_u32(&mb, id_buf);
    cp_put_i32(&mb, 0);
    cp_put_i32(&mb, 128);
    cp_put_i32(&mb, 128);
    cp_put_i32(&mb, 128 * 4);
    cp_put_i32(&mb, 0);
    send_req(id_pool, CP_POOL_CREATE_BUFFER);
    ser_puts("[CPTEST] pool+buffer requests sent\n");

    /* 4. surface + attach + damage + commit */
    id_surf = cp_objmap_new_id(&map);
    req_begin(id_comp, CP_COMPOSITOR_CREATE_SURFACE);
    cp_put_u32(&mb, id_surf);
    send_req(id_comp, CP_COMPOSITOR_CREATE_SURFACE);
    req_begin(id_surf, CP_SURFACE_ATTACH);
    cp_put_u32(&mb, id_buf);
    send_req(id_surf, CP_SURFACE_ATTACH);
    req_begin(id_surf, CP_SURFACE_DAMAGE);
    cp_put_i32(&mb, 0);
    cp_put_i32(&mb, 0);
    cp_put_i32(&mb, 128);
    cp_put_i32(&mb, 128);
    send_req(id_surf, CP_SURFACE_DAMAGE);
    req_begin(id_surf, CP_SURFACE_FRAME);
    id_cb = cp_objmap_new_id(&map);
    cp_put_u32(&mb, id_cb);
    send_req(id_surf, CP_SURFACE_FRAME);
    req_begin(id_surf, CP_SURFACE_COMMIT);
    send_req(id_surf, CP_SURFACE_COMMIT);
    if (!wait_event(id_surf, CP_SURFACE_ENTER, 8000)) {
        ser_puts("[CPTEST] FAIL: no surface.enter\n");
        m4k_exit(1);
    }
    ser_puts("[CPTEST] PASS: surface mapped (enter event)\n");
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 6) * fb.width + BEACON_X] = 0xFFFF9600u;

    /* 5. frame callback round */
    if (!wait_event(id_cb, CP_CALLBACK_DONE, 8000)) {
        ser_puts("[CPTEST] FAIL: no frame callback\n");
        m4k_exit(1);
    }
    ser_puts("[CPTEST] PASS: frame callback done\n");
    while (evt_pop(&eo, &ep, &ea) > 0)
        ;

    /* 6. paint a pattern into the pool and commit again */
    for (i = 0; i < 128 * 128; i++)
        pool_mem[i] = 0xFF0000FFu | ((uint32_t)(i & 127) << 8);
    req_begin(id_surf, CP_SURFACE_DAMAGE);
    cp_put_i32(&mb, 0);
    cp_put_i32(&mb, 0);
    cp_put_i32(&mb, 128);
    cp_put_i32(&mb, 128);
    send_req(id_surf, CP_SURFACE_DAMAGE);
    req_begin(id_surf, CP_SURFACE_FRAME);
    cp_put_u32(&mb, id_cb);
    send_req(id_surf, CP_SURFACE_FRAME);
    req_begin(id_surf, CP_SURFACE_COMMIT);
    send_req(id_surf, CP_SURFACE_COMMIT);
    if (!wait_event(id_cb, CP_CALLBACK_DONE, 8000)) {
        ser_puts("[CPTEST] FAIL: no 2nd frame callback\n");
        m4k_exit(1);
    }
    ser_puts("[CPTEST] ALL PASS - protocol stack verified\n");
    if (fb_pixels)
        fb_pixels[(BEACON_Y0 + 8) * fb.width + BEACON_X] = 0xFFFFFFFFu;

    /* Park: keep the surface alive so the smoke screenshot shows it.
 * Re-paint the beacon + re-commit damage every few yields so the
 * legacy compositor's periodic full-screen redraw (menubar clock)
 * cannot erase the evidence. */
{
    int guard = 0;
    for (;;) {
        if (fb_pixels) {
            fb_pixels[(BEACON_Y0 + 0) * fb.width + BEACON_X] =
                0xFF00FF00u;
            fb_pixels[(BEACON_Y0 + 2) * fb.width + BEACON_X] =
                0xFF00FFFFu;
            fb_pixels[(BEACON_Y0 + 4) * fb.width + BEACON_X] =
                0xFFFFFF00u;
            fb_pixels[(BEACON_Y0 + 6) * fb.width + BEACON_X] =
                0xFFFF9600u;
            fb_pixels[(BEACON_Y0 + 8) * fb.width + BEACON_X] =
                0xFFFFFFFFu;
        }
        if (++guard >= 64) {
            guard = 0;
            req_begin(id_surf, CP_SURFACE_DAMAGE);
            cp_put_i32(&mb, 0);
            cp_put_i32(&mb, 0);
            cp_put_i32(&mb, 128);
            cp_put_i32(&mb, 128);
            send_req(id_surf, CP_SURFACE_DAMAGE);
            req_begin(id_surf, CP_SURFACE_FRAME);
            cp_put_u32(&mb, id_cb);
            send_req(id_surf, CP_SURFACE_FRAME);
            req_begin(id_surf, CP_SURFACE_COMMIT);
            send_req(id_surf, CP_SURFACE_COMMIT);
        }
        m4k_yield();
    }
}
}
