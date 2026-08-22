/*
 * M4KK1 4P1 - sprach.h
 * Description: Sprach window manager - shared core declarations
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * Sprach is the desktop shell / window manager.  It runs as a child of
 * the Copland display server (fork + exec) at 0x900000 and talks to
 * Copland through the shared-memory protocol in libcopland.h:
 *
 *   - windows are Copland surfaces whose pixel buffers live in Sprach's
 *     own BSS (clients render, the server composites via gfx blit);
 *   - every frame Sprach paints its windows, pushes MOVE commands for
 *     the mode layout, bumps shm->heartbeat for the Copland watchdog
 *     and polls the keyboard (a syscall, which also cooperatively
 *     yields the CPU back to Copland).
 *
 * Desktop layout (Mac OS styled, 2026-08-12 refactor):
 *   - top menubar   : MENUBAR_H px, system name + HH:MM:SS clock;
 *   - bottom dock   : TASKBAR_H px, window buttons + terminal launch;
 *   - work area     : between menubar and dock (WORK_AREA_Y..SCREEN_H).
 *
 * Three layout modes are compiled from one core (sprach.c) plus a mode
 * file: sprach_mode_stack.c (overlapping, active-on-top), sprach_mode_
 * tiling.c (non-overlapping grid) and sprach_mode_scroll.c (a virtual
 * canvas scrolled under a fixed viewport).
 */

#ifndef SPRACH_H
#define SPRACH_H

#include <stdint.h>
#include "../lib/libcopland.h"

/* ── Screen geometry ── */

#define SPRACH_WINDOW_COUNT 3
#define SPRACH_WIN_W        256
#define SPRACH_WIN_H        192
#define SPRACH_TITLE_H      18
#define SCREEN_W            800
#define SCREEN_H            600
#define MENUBAR_H           24
#define TASKBAR_H           48
#define TASKBAR_BTN_W       96
#define DOCK_ICON_SIZE      32      /* 32x32 dock icons (no text buttons) */
#define DOCK_ICON_PITCH     (DOCK_ICON_SIZE + 12)   /* icon + gap */
#define DOCK_PAD            8       /* left edge of the first icon */

/* Terminal (client process) window geometry — must match terminal.c */
#define TERM_WIN_W          680
#define TERM_WIN_H          456
#define TERM_TITLE_H        SPRACH_TITLE_H

/* Low-frequency animation refresh: composite at most every N ticks.
 * Mouse cursor movement never touches this (kernel hardware cursor). */
#define SPRACH_ANIM_TICKS   30

/* Mouse: raw PS/2 deltas are scaled by this factor (screen: +y down) */
#define SPRACH_MOUSE_SPEED  1

/* Work area: the region between the top menubar and bottom taskbar */
#define WORK_AREA_Y         MENUBAR_H
#define WORK_AREA_H         (SCREEN_H - MENUBAR_H - TASKBAR_H)

/* ── Chrome colors (BGRA) ── */

#define SPRACH_COL_BODY        0x00D0D0D0
#define SPRACH_COL_TITLE_1     0x00803030
#define SPRACH_COL_TITLE_2     0x00308030
#define SPRACH_COL_TITLE_3     0x00303080
#define SPRACH_COL_CLOSE       0x00C03030
#define SPRACH_COL_TEXTBAR     0x008080C0
#define SPRACH_COL_ACCENT      0x00FFD060
#define SPRACH_COL_DOT         0x00206020
#define SPRACH_COL_BORDER      0x00505050

/* ── Taskbar (bottom Dock, Mac OS style) colors ── */

#define SPRACH_COL_TASKBAR_BG  0x00333333
#define SPRACH_COL_TASKBAR_BTN 0x00505050
#define SPRACH_COL_TASKBAR_TOP 0x00C0C0C0
#define SPRACH_COL_TASKBAR_TXT 0x00E8E8E8
#define SPRACH_COL_TASKBAR_ACT 0x00F5F5F5   /* active button: near-white bg */
#define SPRACH_COL_TASKBAR_ACTTXT 0x00101010

/* Menubar (Mac OS style) */
#define SPRACH_COL_MENUBAR_BG  0x00CCCCCC
#define SPRACH_COL_MENUBAR_FG  0x00000000
#define SPRACH_COL_MENUBAR_TXT 0x00303030
#define SPRACH_COL_MENUBAR_DIM 0x00909090

/* ── Mac OS 9 Platinum window controls ── */

#define SPRACH_COL_BTN_CLOSE    0x00E05050
#define SPRACH_COL_BTN_MIN      0x00E0C040
#define SPRACH_COL_BTN_MAX      0x0040C040
#define SPRACH_COL_BTN_CLOSE_D  0x00802020
#define SPRACH_COL_BTN_MIN_D    0x00806020
#define SPRACH_COL_BTN_MAX_D    0x00206020
#define SPRACH_COL_BTN_SHADOW   0x00800000
#define CTRL_SIZE               10
#define CTRL_Y                   4
#define CTRL_CLOSE_X             8
#define CTRL_MIN_X              22
#define CTRL_MAX_X              36

/* ── A window owned by Sprach ── */

struct sprach_window {
    int slot;            /* Copland surface slot */
    int x, y;            /* screen position (mode-driven) */
    int w, h;            /* fixed size */
    int normal_x, normal_y, normal_w, normal_h;  /* pre-maximize geometry */
    uint32_t title;      /* title-bar color */
    uint32_t body;       /* body color */
    uint32_t *buf;       /* pixel buffer (w * h) */
    const char *title_str; /* short title shown in the taskbar button */
    int hidden;          /* 1 = minimized */
    int maximized;       /* 1 = full work area */
    int btn_clicked;     /* 1=close, 2=min, 3=max, 0=none */
    uint32_t click_tick; /* frame when click happened (for feedback) */
    int desktop;         /* virtual desktop this window lives on */
};

/* ── Terminal (client process) window state ──
 * The /bin/terminal process owns its pixel buffer and paints both its
 * title bar and its 80×25 character grid; Sprach only tracks the
 * surface, routes clicks/chords, raises it in z-order and forwards
 * keyboard input through the term_mailbox ring. */

struct sprach_ctx {
    struct copland_shm *shm;
    struct sprach_window wins[SPRACH_WINDOW_COUNT];
    uint32_t tick;
    int active;          /* active window index (stacking: raised) */
    int dir;             /* scroll direction */
    int offs;            /* scroll viewport offset */
    int taskbar_slot;    /* Copland surface slot for the taskbar (-1 = uninit) */
    int menubar_slot;    /* Copland surface slot for the top menubar (-1 = uninit) */
    int mouse_x, mouse_y;  /* accumulated absolute mouse position */
    int btn_was_down;      /* click edge-trigger latch (1 = button held) */

    /* Terminal client window */
    int term_slot;       /* Copland surface slot owned by /bin/terminal (-1 = none) */
    int term_pid;        /* terminal process pid (-1 = none) */
    int term_hidden;     /* 1 = minimized (surface not visible) */
    int term_maximized;  /* 1 = full work area */
    int term_normal_x, term_normal_y, term_normal_w, term_normal_h;
    uint32_t term_spawn_tick;  /* tick of the last fork; stale-pid timeout */
    int term_desktop;    /* virtual desktop the terminal lives on */

    /* Taskbar redraw caching: only repaint when state actually changes */
    int last_tbar_active;    /* active window index (-1 = terminal focus) */
    uint32_t last_tbar_win;  /* per-window slot/hidden bitmask */
    int last_tbar_minute;    /* last rendered epoch-minute (HH:MM clock) */

    /* Menubar clock cache: last rendered epoch-second (HH:MM:SS clock) */
    int menu_last_second;

    /* Clock popup window (menubar time click): 1 = open.  Drawn on its
     * own surface (clock_slot) below the menubar. */
    int clock_open;
    int clock_slot;      /* Copland surface slot (-1 = uninit) */
    int clock_x, clock_y;   /* popup top-left (screen space) */
    int clock_last_sec;  /* second of the last popup repaint */
    int clock_about;     /* 1 = popup shows "About This PC" info */
    int clock_settings;  /* 1 = About panel is the settings placeholder */

    /* Desktop index shown in the menubar center ("Desktop N") */
    int desktop_idx;

    /* Apple-menu dropdown (click the "M4KK1" brand): 1 = open.
     * Drawn on its own surface (menu_slot) below the brand text. */
    int menu_open;
    int menu_slot;       /* Copland surface slot (-1 = uninit) */

    /* Launchpad overlay (Dock grid icon click): 1 = open.  Drawn on
     * its own surface (lp_slot) covering the work area. */
    int lp_open;
    int lp_slot;         /* Copland surface slot (-1 = uninit) */

    /* Per-window virtual desktop assignment (wins[].desktop uses this
     * range too).  Desktop 0 is the initial one. */
#define SPRACH_DESKTOPS 4

    /* Launchpad app list (scanned from /bin at open time) */
    int lp_count;
};

/* Core services (sprach.c) */
int sprach_create_window(struct sprach_ctx *ctx, int idx, int x, int y,
                         uint32_t title, uint32_t body);
void sprach_paint_window(struct sprach_ctx *ctx, struct sprach_window *w);
void sprach_commit_layout(struct sprach_ctx *ctx);
int sprach_create_taskbar(struct sprach_ctx *ctx);
void sprach_draw_taskbar(struct sprach_ctx *ctx);
int sprach_create_menubar(struct sprach_ctx *ctx);
void sprach_draw_menubar(struct sprach_ctx *ctx);
int sprach_create_clock_popup(struct sprach_ctx *ctx);
void sprach_draw_clock_popup(struct sprach_ctx *ctx);
int sprach_create_app_menu(struct sprach_ctx *ctx);
void sprach_draw_app_menu(struct sprach_ctx *ctx);
void sprach_app_menu_toggle(struct sprach_ctx *ctx, int open);
int sprach_create_launchpad(struct sprach_ctx *ctx);
void sprach_draw_launchpad(struct sprach_ctx *ctx);
void sprach_launchpad_toggle(struct sprach_ctx *ctx, int open);
void sprach_dock_activate(struct sprach_ctx *ctx, int idx);
void sprach_switch_desktop(struct sprach_ctx *ctx, int desk);
void sprach_spawn_terminal(struct sprach_ctx *ctx);
void sprach_handle_mouse(struct sprach_ctx *ctx);
void sprach_raise_window(struct sprach_ctx *ctx, int idx);
void sprach_raise_surface(struct sprach_ctx *ctx, int slot);
void sprach_spawn_terminal(struct sprach_ctx *ctx);
void sprach_poll_terminal(struct sprach_ctx *ctx);
void sprach_handle_terminal_click(struct sprach_ctx *ctx, int sx, int sy,
                                  int sw, int sh, int lx, int ly);
int sprach_taskbar_dirty(struct sprach_ctx *ctx);

/* Mode interface (one mode file per binary) */
void sprach_mode_init(struct sprach_ctx *ctx);
void sprach_mode_tick(struct sprach_ctx *ctx);
void sprach_mode_key(struct sprach_ctx *ctx, unsigned char key);
const char *sprach_mode_name(void);

#endif /* SPRACH_H */
