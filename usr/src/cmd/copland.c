/*
 * M4KK1 4P1 - copland.c
 * Description: Copland display server - window surface manager
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Copland is the graphics hardware abstraction layer.  It owns the
 * framebuffer and maintains the shared window-surface table (see
 * libcopland.h) that clients such as the Sprach desktop shell drive
 * through the fixed shared-memory region at 0x00700000.
 *
 * Note (4P1): the kernel's m4k_spawn() has no argv support, so the
 * `copland --status` verification is emulated: when the daemon is
 * installed as /usr/bin/copland_status, it reports its readiness with
 * a "Copland ready" line; in all cases it prints "Copland ready" once
 * the shared region and compositor are up.
 */

#include "../lib/libgui.h"
#include "../lib/libcopland.h"
#include "../lib/mkrn_rect.h"
#include "../../../sys/src/copland/compositor.h"

/* Global variables required by m4sh.h functions */
int out_fd = 1;
char cwd[256] = "/";

/* Border / chrome colors */
#define COPLAND_COLOR_WHITE  0x00FFFFFF
#define COPLAND_COLOR_DARK   0x00404040
#define COPLAND_COLOR_TITLE  0x00303080

static int copland_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* The kernel spawn ABI carries no argv; detect `--status` by the
 * process name the kernel derived from the spawn path. */
static int copland_status_mode(void)
{
    struct procinfo pi[8];
    int n = musr_sc_getprocs(pi, 8);
    if (n > 0 && copland_streq(pi[0].name, "copland_status"))
        return 1;
    return 0;
}

static void copland_init_shm(struct copland_shm *shm)
{
    memset(shm, 0, sizeof(*shm));
    shm->magic = COPLAND_SHM_MAGIC;
    shm->version = COPLAND_VERSION;
    shm->ready = 1;
    ser_puts("[COPLAND] shared region at 0x");
    print_u32((uint32_t)shm);
    ser_puts(" ready\n");
}

static void copland_render_surface(const struct copland_surface *s)
{
    if (!(s->flags & COPLAND_SURF_VISIBLE))
        return;
    /* Snapshot geometry ONCE: a concurrent MOVE/RESIZE landing
     * between the blit and the border draws would otherwise mix
     * two different w/h values (torn frame) — and in the worst
     * case blit with a stale-bigger w against a fresh buffer. */
    const int sw = s->w;
    const int sh = s->h;
    if (sw <= 0 || sh <= 0)
        return;

    /* Client-rendered window content (Sprach model): blit the
     * surface's pixel buffer.  Otherwise fall back to a flat fill. */
    if (s->buffer_ptr)
        m4k_gfx_blit(s->x, s->y, sw, sh,
                     (const void *)(uintptr_t)s->buffer_ptr);
    else
        gui_draw_rect(s->x, s->y, sw, sh, s->color);

    /* 3D-ish border: light top/left, dark bottom/right */
    gui_draw_rect(s->x, s->y, sw, 1, COPLAND_COLOR_WHITE);
    gui_draw_rect(s->x, s->y, 1, sh, COPLAND_COLOR_WHITE);
    gui_draw_rect(s->x, s->y + sh - 1, sw, 1, COPLAND_COLOR_DARK);
    gui_draw_rect(s->x + sw - 1, s->y, 1, sh, COPLAND_COLOR_DARK);
}

/* Wallpaper gradient fill clipped to a region (incremental path).
 * One kernel fill_gradient syscall paints the clipped rect with the
 * same full-screen gradient math (top at y=0, bottom at fb height-1)
 * the boot composite uses. */
static void copland_wallpaper_rect(int x, int y, int w, int h)
{
    struct m4k_framebuffer_info fb;
    if (m4k_get_framebuffer_info(&fb) != 0 || fb.height <= 1)
        return;
    /* Same palette as the boot composite's gui_draw_gradient call —
     * the old code used top green=0x44 here vs 0x00 in the full
     * composite, leaving a visible color seam on incremental
     * repaints.  Unified now that both paths share the kernel
     * gradient primitive. */
    m4k_fill_gradient(x, y, w, h, 0x00000044, 0x000066FF);
}

static void copland_composite_region(struct copland_shm *shm,
                                     const struct mkrn_rect *r)
{
    int i;
    int rx = r->left, ry = r->top;
    int rw = r->right - r->left, rh = r->bottom - r->top;

    copland_wallpaper_rect(rx, ry, rw, rh);

    for (i = 0; i < COPLAND_MAX_SURFACES; i++) {
        const struct copland_surface *s = &shm->surfaces[i];
        if (!s->in_use || !(s->flags & COPLAND_SURF_VISIBLE))
            continue;
        if (s->w <= 0 || s->h <= 0)
            continue;
        /* intersect surface rect with the damage region
         * (mkrn_rect_intersect semantics) */
        struct mkrn_rect isect = { s->x, s->y, s->x + s->w, s->y + s->h };
        mkrn_rect_intersect(&isect, r);
        if (mkrn_rect_is_empty(&isect))
            continue;
        if (s->buffer_ptr) {
            /* STRIDE CONTRACT (see m4kk1-graphics-stack skill): the
             * kernel derives the source row stride from the PASSED w.
             * Passing the clipped sub-rect width made it read the
             * client buffer with the wrong pitch (skewed/torn rows
             * for any partially off-screen surface).  Pass the FULL
             * surface rect from its origin instead — the kernel clips
             * to the screen itself, keeping the source stride equal
             * to s->w.  Overdraw past the damage region is bounded
             * by the surface size and composited in slot order, so
             * occlusion stays correct. */
            m4k_gfx_blit(s->x, s->y, s->w, s->h,
                         (const void *)(uintptr_t)s->buffer_ptr);
        } else {
            gui_draw_rect(isect.left, isect.top,
                          isect.right - isect.left,
                          isect.bottom - isect.top, s->color);
        }
    }

    /* New-protocol surfaces must ride on top of the legacy pass in
     * the INCREMENTAL path too: this repaint redraws the wallpaper
     * and every legacy surface in the damaged region, which erases
     * any chrome (menubar/dock) the protocol compositor previously
     * blitted there.  Without this call the legacy clock/demo
     * animation wiped the chrome within one frame of it appearing
     * (observed: chrome pixels present in sprach BSS, blits running,
     * screen showing only wallpaper).  paint() blits full surfaces
     * (bounded overdraw), occlusion stays correct. */
    cp_compositor_paint(&cp_comp);

    m4k_flip_rect(rx, ry, rw, rh);
}

static void copland_composite(struct copland_shm *shm)
{
    int i;

    /* Desktop background: the unified system wallpaper (see
     * gui_draw_wallpaper). */
    gui_draw_wallpaper();

    /* All live surfaces, back to front */
    for (i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (shm->surfaces[i].in_use)
            copland_render_surface(&shm->surfaces[i]);
    }

    /* New-protocol surfaces layer on top of the legacy pass so they
     * survive the legacy full-screen wallpaper redraw (phase 2). */
    cp_compositor_paint(&cp_comp);

    gui_flip();
}

static void copland_handle_commands(struct copland_shm *shm)
{
    struct copland_command c;

    while (copland_cmd_pop(shm, &c)) {
        switch (c.cmd) {
        case COPLAND_CMD_CREATE_SURFACE: {
            int id = copland_surface_alloc(shm);
            if (id >= 0) {
                struct copland_surface *s = &shm->surfaces[id];
                s->x = c.args[0];
                s->y = c.args[1];
                s->w = c.args[2];
                s->h = c.args[3];
                s->color = (uint32_t)c.args[4];
                s->flags = (uint32_t)c.args[5] | COPLAND_SURF_VISIBLE;
                s->buffer_ptr = 0;
                shm->surface_count++;
                ser_puts("[COPLAND] created surface id=");
                print_u32((uint32_t)id);
                ser_puts("\n");
            }
            break;
        }
        case COPLAND_CMD_MOVE_SURFACE:
            if (c.args[0] >= 0 &&
                c.args[0] < COPLAND_MAX_SURFACES &&
                shm->surfaces[c.args[0]].in_use) {
                shm->surfaces[c.args[0]].x = c.args[1];
                shm->surfaces[c.args[0]].y = c.args[2];
            }
            break;
        case COPLAND_CMD_RESIZE_SURFACE:
            if (c.args[0] >= 0 &&
                c.args[0] < COPLAND_MAX_SURFACES &&
                shm->surfaces[c.args[0]].in_use) {
                shm->surfaces[c.args[0]].w = c.args[1];
                shm->surfaces[c.args[0]].h = c.args[2];
            }
            break;
        case COPLAND_CMD_REDRAW_SURFACE:
            if (c.args[0] >= 0 &&
                c.args[0] < COPLAND_MAX_SURFACES &&
                shm->surfaces[c.args[0]].in_use) {
                shm->surfaces[c.args[0]].color = (uint32_t)c.args[1];
            }
            break;
        case COPLAND_CMD_DESTROY_SURFACE:
            if (c.args[0] >= 0 &&
                c.args[0] < COPLAND_MAX_SURFACES &&
                shm->surfaces[c.args[0]].in_use) {
                /* Zero the WHOLE slot before releasing it.  A stale
                 * in_use=1 window between destroy and the next
                 * create lets the compositor blit a dead client's
                 * buffer_ptr (the EIP-into-lp_buf crash class). */
                struct copland_surface *d =
                    &shm->surfaces[c.args[0]];
                d->buffer_ptr = 0;
                d->flags = 0;
                d->w = 0;
                d->h = 0;
                d->x = 0;
                d->y = 0;
                d->color = 0;
                d->dmg_w = 0;
                d->in_use = 0;      /* publish last */
                if (shm->surface_count > 0)
                    shm->surface_count--;
                ser_puts("[COPLAND] destroyed surface id=");
                print_u32((uint32_t)c.args[0]);
                ser_puts("\n");
            }
            break;
        case COPLAND_CMD_STATUS:
        case COPLAND_CMD_NONE:
        default:
            break;
        }
    }
}

/* ── New-protocol backend hooks (phase 2) ── */

static void cp_daemon_blit(int x, int y, int w, int h,
			   const uint8_t *src, uint32_t stride)
{
#ifdef COPLAND_FRAME_DEBUG
    {
        static int nblit;
        if (nblit++ < 12) {
            ser_puts("[COPLAND] blit ");
            print_u32((uint32_t)x); ser_puts(",");
            print_u32((uint32_t)y); ser_puts(" ");
            print_u32((uint32_t)w); ser_puts("x");
            print_u32((uint32_t)h); ser_puts(" src=0x");
            print_u32((uint32_t)(uintptr_t)src); ser_puts("\n");
        }
    }
#endif
    /* The kernel blit uses w*4 stride; new-protocol buffers may
     * carry a larger stride.  Row-by-row fallback keeps correctness
     * (v1: only when stride != w*4). */
    if (stride == (uint32_t)(w * 4)) {
        m4k_gfx_blit(x, y, w, h, src);
        return;
    }
    {
        int row;
        for (row = 0; row < h; row++) {
            const uint8_t *r = src + (uint32_t)row * stride;
            /* one-row blits: correctness over speed in v1 */
            m4k_gfx_blit(x, y + row, w, 1, r);
        }
    }
}

static void cp_daemon_fill(int x, int y, int w, int h, uint32_t argb)
{
    gui_draw_rect(x, y, w, h, argb);
}

static void cp_daemon_wallpaper(int x, int y, int w, int h)
{
    copland_wallpaper_rect(x, y, w, h);
}

static const struct cp_backend cp_daemon_backend = {
    cp_daemon_blit, cp_daemon_fill, cp_daemon_wallpaper
};

#ifdef CP_PAINT_DEBUG_BRIDGE
/* bridge: called from compositor.c under CP_PAINT_DEBUG */
void cp_debug_blit(uint32_t surf, uint32_t client, uint32_t buf,
                   uint32_t pool, uint32_t paddr, uint32_t poff,
                   uint32_t src)
{
    ser_puts("[PAINT] surf=");  print_u32(surf);
    ser_puts(" cl=");          print_u32(client);
    ser_puts(" buf=");         print_u32(buf);
    ser_puts(" pool=");        print_u32(pool);
    ser_puts(" addr=0x");      print_u32(paddr);
    ser_puts(" off=");         print_u32(poff);
    ser_puts(" => src=0x");    print_u32(src);
    ser_puts("\n");
}
#endif

/* One round of new-protocol service: adopt new connections,
 * dispatch one request per client, and run one frame cycle. */
static void copland_protocol_tick(void)
{
    struct cp_conn *adopted[CP_CONN_SLOTS];
    int n, i;

    n = cp_conn_table_poll(adopted, CP_CONN_SLOTS);
    for (i = 0; i < n; i++) {
        int ci = cp_compositor_attach_client(&cp_comp, adopted[i]);
        ser_puts("[COPLAND] new protocol client #");
        print_u32(adopted[i]->client_id);
        ser_puts(ci >= 0 ? " attached\n" : " REJECTED\n");
    }
    for (i = 0; i < CP_MAX_CLIENTS; i++) {
        if (cp_comp.clients[i].active)
            cp_compositor_dispatch(&cp_comp, &cp_comp.clients[i]);
    }
    if (cp_comp.frame_dirty) {
        int fx = cp_comp.frame_dmg_x, fy = cp_comp.frame_dmg_y;
        int fw = cp_comp.frame_dmg_w, fh = cp_comp.frame_dmg_h;
        cp_compositor_frame(&cp_comp);
        /* present the damaged region (frame() blits into the
         * back buffer; the legacy path flips full-screen) */
        m4k_flip_rect(fx, fy, fw, fh);
#ifdef COPLAND_FRAME_DEBUG
        {
            static int ndbg;
            if (ndbg++ < 8) {
                ser_puts("[COPLAND] frame dmg=");
                print_u32((uint32_t)fx); ser_puts(",");
                print_u32((uint32_t)fy); ser_puts(" ");
                print_u32((uint32_t)fw); ser_puts("x");
                print_u32((uint32_t)fh); ser_puts("\n");
            }
        }
#endif
    }
}

/* ── Sprach watchdog (phase 3) ── */

#define COPLAND_SIGKILL 2  /* M4K_SIGKILL (signal.h); kernel-internal value */

/* Fork a Copland-protocol smoke client (phase 2 verification).
 * Returns the child pid, or -1.  The child execs /bin/cptest which
 * runs the full protocol handshake and then parks with its surface
 * composited on screen (no argv ABI: behaviour fixed by the binary). */
static int copland_spawn_cptest(void)
{
    /* Parent-side beacon: fork+spawn ATTEMPT issued (column x=640,
     * y=410).  If this pixel is missing the spawn path never ran. */
    {
        struct m4k_framebuffer_info fb;
        if (m4k_get_framebuffer_info(&fb) == 0) {
            uint32_t *px = (uint32_t *)(uintptr_t)fb.phys_addr;
            px[410 * fb.width + 640] = 0xFF0000FFu;
        }
    }
    int pid = musr_sc_fork();
    if (pid == 0) {
        int r = m4k_spawn("/bin/cptest", 0);
        /* Serial is dead by this point in the boot chain; paint the
         * failure code on the framebuffer so headless smoke tests
         * can see it.  Column x=640, y=400..: one red px per
         * failure kind (r==-1 open/elf: 1px; else 2px). */
        {
            struct m4k_framebuffer_info fb;
            if (m4k_get_framebuffer_info(&fb) == 0) {
                uint32_t *px =
                    (uint32_t *)(uintptr_t)fb.phys_addr;
                px[400 * fb.width + 640] = 0xFFFF0000u;
                if (r != (int)0xFFFF0000u && r != -1)
                    px[401 * fb.width + 640] = 0xFF00FF00u;
            }
        }
        ser_puts("[COPLAND] cptest spawn failed (ret=");
        print_u32((uint32_t)r);
        ser_puts(")\n");
        m4k_exit(1);
    }
    if (pid < 0)
        return -1;
    ser_puts("[COPLAND] cptest client started (pid=");
    print_u32((uint32_t)pid);
    ser_puts(")\n");
    return (int)pid;
}

/* Fork a Sprach window-manager child.  Returns the child pid on
 * success, -1 on failure.  The child execs /bin/sprach (which never
 * returns on success). */
static int copland_spawn_wm(struct copland_shm *shm)
{
    int pid = musr_sc_fork();
    if (pid == 0) {
        /* Child: become the window manager */
        int r = m4k_spawn("/bin/sprach", 0);
        ser_puts("[COPLAND] WM spawn failed (ret=");
        print_u32((uint32_t)r);
        ser_puts(")\n");
        m4k_exit(1);
    }
    if (pid < 0) {
        ser_puts("[COPLAND] fork failed\n");
        return -1;
    }
    ser_puts("[COPLAND] Sprach WM started (pid=");
    print_u32((uint32_t)pid);
    ser_puts(")\n");
    shm->heartbeat = 0;
    return (int)pid;
}

void _start(void)
{
    ser_puts("[COPLAND] ================================\n");
    ser_puts("[COPLAND] Display server starting...\n");
    ser_puts("[COPLAND] ================================\n");

    struct copland_shm *shm = copland_shm_get();

    copland_init_shm(shm);
    copland_composite(shm);

    /* New protocol stack (phase 1-2): connection table + compositor */
    cp_conn_table_init();
    cp_compositor_init(&cp_comp);
    cp_compositor_set_backend(&cp_comp, &cp_daemon_backend);
    ser_puts("[COPLAND] protocol compositor up (conn table 0x720000)\n");

    if (copland_status_mode()) {
        /* Verification mode: report readiness and stay up */
        ser_puts("[COPLAND] status: Copland ready\n");
        for (;;) {
            copland_handle_commands(shm);
            for (volatile int i = 0; i < 150000; i++);
        }
    }
    ser_puts("[COPLAND] Copland ready\n");

    /* Bring up the Sprach window manager */
    uint32_t last_beat = 0;
    int wm_pid = copland_spawn_wm(shm);
    /* Pin the layout privilege to the WM process BEFORE any client
     * (including our own smoke client below) can claim it by racing
     * the bind. */
    cp_compositor_set_wm_pid(&cp_comp, (uint32_t)wm_pid);

    /* Phase 2 verification: also start the protocol smoke client.
     * It connects through the new connection table, runs the full
     * handshake, and parks with its surface on screen. */
    copland_spawn_cptest();

    /* Server loop: consume IPC commands and recompose the screen
     * only when the dirty flag is set.  Cursor updates are handled
     * separately by the kernel (m4k_update_cursor) and do not need
     * a full composite+flip. */
    for (;;) {
        /* Session shutdown request from the WM (menu Lock/Shut Down):
         * tear everything down and exit — MDM is waiting on our pid
         * and will redraw the login screen. */
        if (shm->shutdown) {
            ser_puts("[COPLAND] shutdown requested, exiting...\n");
            if (wm_pid > 0)
                m4k_kill(wm_pid, COPLAND_SIGKILL);
            /* "Shut Down" menu action: the WM set shm->reboot —
             * issue the magic-gated reboot syscall (int 0x80 nr 0x6D,
             * magic1=0x01234567 magic2=0x89ABCDEF) before exiting.
             * QEMU std i386 warm-resets the machine. */
            if (shm->reboot) {
                uint32_t ret;
                ser_puts("[COPLAND] rebooting machine...\n");
                __asm__ volatile(
                    "int $0x80"
                    : "=a"(ret)
                    : "a"(0x6Du), "b"(0x01234567u),
                      "c"(0x89ABCDEFu), "d"(1u)
                    : "memory");
                (void)ret;   /* never returns on success */
            }
            m4k_exit(0);
        }

        int had_commands = 0;
        {
            uint32_t saved_r = shm->cmd_read_idx;
            copland_handle_commands(shm);
            had_commands = (shm->cmd_read_idx != saved_r);
        }
        copland_protocol_tick();

        /* Proto-frame telemetry: one line per 30 s wall-clock.  If the
         * proto frame path stalls (frame callbacks never fire, chrome
         * never repaints), this pinpoints WHEN it died. */
        {
            static uint32_t last_tel;
            uint32_t now = musr_sc_uptime();
            if (now - last_tel >= 30000) {
                int ncl = 0, nmap = 0;
                for (int i = 0; i < CP_MAX_CLIENTS; i++)
                    if (cp_comp.clients[i].active)
                        ncl++;
                for (int i = 0; i < CP_MAX_SURFACES; i++)
                    if (cp_comp.surfaces[i].mapped)
                        nmap++;
                last_tel = now;
                ser_puts("[COPLAND] tel serial=");
                print_u32(cp_comp.serial);
                ser_puts(" clients=");
                print_u32((uint32_t)ncl);
                ser_puts(" mapped=");
                print_u32((uint32_t)nmap);
                ser_puts(" fdirty=");
                ser_puts(cp_comp.frame_dirty ? "1" : "0");
                ser_puts("\n");
            }
        }

        if (shm->dirty || had_commands) {
            copland_composite(shm);
            shm->dirty = 0;
            /* full-frame composite consumed all pending damage */
            for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
                shm->surfaces[i].dmg_w = 0;
        } else {
            /* Incremental path: union every pending per-surface damage
             * rect (union-bounds semantics) and
             * re-composite only that region, then flip it. */
            struct mkrn_rect total;
            int incr = 0;
            total.left = 1; total.top = 1;
            total.right = 0; total.bottom = 0;   /* empty */
            for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
                struct copland_surface *s = &shm->surfaces[i];
                if (s->dmg_w > 0 && s->dmg_h > 0) {
                    struct mkrn_rect r;
                    r.left = s->dmg_x; r.top = s->dmg_y;
                    r.right = s->dmg_x + s->dmg_w;
                    r.bottom = s->dmg_y + s->dmg_h;
                    mkrn_rect_union(&total, &r);
                    incr = 1;
                }
                s->dmg_w = 0;
            }
            if (incr) {
                struct mkrn_rect clip;
                clip.left = 0; clip.top = 0;
                clip.right = 800; clip.bottom = 600;
                mkrn_rect_intersect(&total, &clip);
                if (!mkrn_rect_is_empty(&total))
                    copland_composite_region(shm, &total);
            } else {
                /* The clean path performs no syscalls; without an
                 * explicit yield, Sprach never gets CPU to re-set the
                 * dirty flag, the heartbeat freezes, and the watchdog
                 * kills a healthy WM.  Sleep (hlt) rather than bare
                 * yield: an interactive storm keeps the command path
                 * hot, and the watchdog below is time-based, so the
                 * server loop itself must never out-run the WM's
                 * 20 ms frame cadence. */
                m4k_sleep(5);
            }
        }

        /* WM watchdog (time-based): the WM beats once per frame
         * (~50 FPS).  Under an interactive storm the server loop
         * iterates orders of magnitude faster than the WM, and an
         * iteration-count watchdog kills a perfectly healthy WM
         * (hb still climbing at the kill - observed hb=223).
         * Judge on wall-clock time instead: no beat change for
         * 3 s = a genuinely hung WM. */
        {
            static uint32_t last_beat_time;
            static int have_beat_time;
            if (shm->heartbeat != last_beat) {
                last_beat = shm->heartbeat;
                last_beat_time = musr_sc_uptime();
                have_beat_time = 1;
            } else if (have_beat_time &&
                       musr_sc_uptime() - last_beat_time > 3000) {
                ser_puts("[COPLAND] WM heartbeat stalled (hb=");
                print_u32(shm->heartbeat);
                ser_puts("), restarting Sprach...\n");
                if (wm_pid > 0)
                    m4k_kill(wm_pid, COPLAND_SIGKILL);
                for (int i = 0; i < COPLAND_MAX_SURFACES; i++) {
                    if (shm->surfaces[i].in_use)
                        shm->surfaces[i].in_use = 0;
                }
                shm->surface_count = 0;
                shm->dirty = 1;
                wm_pid = copland_spawn_wm(shm);
                last_beat = shm->heartbeat;
                last_beat_time = musr_sc_uptime();
            }
        }

        /* Coarse frame pacing (no sleep syscall in 4P1) */
        for (volatile int i = 0; i < 5000; i++);
    }
}
