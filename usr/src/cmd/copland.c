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

static void copland_composite(struct copland_shm *shm)
{
    int i;

    /* Desktop background */
    gui_draw_gradient(0x002040A0, 0x00102050);

    /* Status bar */
    gui_draw_rect(0, 0, 800, 22, 0x00101030);
    gui_draw_text(10, 4, "Copland ready", 0x00FFFFFF, 0x00101030);

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

    /* Server loop: consume IPC commands and recompose the screen */
    for (;;) {
        copland_handle_commands(shm);
        copland_composite(shm);

        /* WM watchdog: if the window manager stops beating its heart,
         * tear it down and relaunch it. */
        if (shm->heartbeat != last_beat) {
            last_beat = shm->heartbeat;
            stale_ticks = 0;
        } else if (++stale_ticks > 60) {
            ser_puts("[COPLAND] WM heartbeat stalled, restarting Sprach...\n");
            if (wm_pid > 0)
                m4k_kill(wm_pid, COPLAND_SIGKILL);
            wm_pid = copland_spawn_wm(shm);
            last_beat = shm->heartbeat;
            stale_ticks = 0;
        }

        /* Coarse frame pacing (no sleep syscall in 4P1) */
        for (volatile int i = 0; i < 150000; i++);
    }
}
