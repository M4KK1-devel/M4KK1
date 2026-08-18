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
    if (s->w <= 0 || s->h <= 0)
        return;

    /* Client-rendered window content (Sprach model): blit the
     * surface's pixel buffer.  Otherwise fall back to a flat fill. */
    if (s->buffer_ptr)
        m4k_gfx_blit(s->x, s->y, s->w, s->h,
                     (const void *)(uintptr_t)s->buffer_ptr);
    else
        gui_draw_rect(s->x, s->y, s->w, s->h, s->color);

    /* 3D-ish border: light top/left, dark bottom/right */
    gui_draw_rect(s->x, s->y, s->w, 1, COPLAND_COLOR_WHITE);
    gui_draw_rect(s->x, s->y, 1, s->h, COPLAND_COLOR_WHITE);
    gui_draw_rect(s->x, s->y + s->h - 1, s->w, 1, COPLAND_COLOR_DARK);
    gui_draw_rect(s->x + s->w - 1, s->y, 1, s->h, COPLAND_COLOR_DARK);
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

    m4k_flip_rect(rx, ry, rw, rh);
}

static void copland_composite(struct copland_shm *shm)
{
    int i;

    /* Desktop background: default blue gradient wallpaper
     * (0x000044 → 0x0066FF), same palette as the MDM login screen. */
    gui_draw_gradient(0x00000044, 0x000066FF);

    /* All live surfaces, back to front */
    for (i = 0; i < COPLAND_MAX_SURFACES; i++) {
        if (shm->surfaces[i].in_use)
            copland_render_surface(&shm->surfaces[i]);
    }

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
                shm->surfaces[c.args[0]].in_use = 0;
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

/* ── Sprach watchdog (phase 3) ── */

#define COPLAND_SIGKILL 2  /* M4K_SIGKILL (signal.h); kernel-internal value */

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
    int stale_ticks = 0;
    int wm_pid = copland_spawn_wm(shm);

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
            m4k_exit(0);
        }

        int had_commands = 0;
        {
            uint32_t saved_r = shm->cmd_read_idx;
            copland_handle_commands(shm);
            had_commands = (shm->cmd_read_idx != saved_r);
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
                 * kills a healthy WM.  Yield here so Sprach can
                 * repaint and beat its heart. */
                m4k_yield();
            }
        }

        /* WM watchdog: if the window manager stops beating its heart,
         * tear it down and relaunch it.  The WM owns every client
         * surface, so clear the whole table before relaunching;
         * otherwise a crashed WM leaks surfaces and the next instance
         * can never allocate a slot (COPLAND_MAX_SURFACES=16). */
        if (shm->heartbeat != last_beat) {
            last_beat = shm->heartbeat;
            stale_ticks = 0;
        } else if (++stale_ticks > 60) {
            ser_puts("[COPLAND] WM heartbeat stalled, restarting Sprach...\n");
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
            stale_ticks = 0;
        }

        /* Coarse frame pacing (no sleep syscall in 4P1) */
        for (volatile int i = 0; i < 5000; i++);
    }
}
