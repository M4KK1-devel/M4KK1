/*
 * ==== ALTR2 main.c — surface claim + event loop ====
 * M4KK1 4P1 - usr/src/cmd/altr/main.c
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

static volatile struct al_mailbox *al_mb =
    (volatile struct al_mailbox *)AL_MB_ADDR;

void altr_load(const char *path)
{
    struct al_doc *d = al_doc_open(path);
    if (d)
        S.cur_doc = (int)(d - S.docs);
}

void altr_save_cur(void)
{
    struct al_doc *d = al_cur();
    if (!d) return;
    if (!d->path[0]) {
        musr_strncpy(S.status, "no file name (Ctrl+O: open <path>)",
                     sizeof(S.status) - 1);
        S.status_col = C_MSG_ERR;
        return;
    }
    if (al_save(d, d->path) == 0) {
        musr_strncpy(S.status, "saved", sizeof(S.status) - 1);
        S.status_col = C_MSG_OK;
        ser_puts("[ALTR2] saved ");
        ser_puts(d->path);
        ser_puts("\n");
    } else {
        musr_strncpy(S.status, "save failed", sizeof(S.status) - 1);
        S.status_col = C_MSG_ERR;
    }
}

void altr_quit(void)
{
    ser_puts("[ALTR2] exit\n");
    m4k_exit(0);
}

void _start(void)
{
    ser_puts("[ALTR2] editor starting\n");

    struct copland_shm *shm = copland_shm_get();
    if (!shm || shm->magic != COPLAND_SHM_MAGIC) {
        ser_puts("[ALTR2] copland not ready\n");
        m4k_exit(1);
    }

    /* mailbox (protocol-stable: 0x620000 / "ALT1") */
    al_mb->magic = AL_MB_MAGIC;
    al_mb->write_idx = 0;
    al_mb->read_idx = 0;

    /* initial state */
    S.sidebar_open = 1;
    S.minimap_on = 1;
    S.focus = FOC_EDITOR;
    S.mode = MODE_NORMAL;
    S.cur_doc = -1;
    al_doc_new();                     /* scratch document */
    tree_reload();

    /* surface (width 640 = sprach dispatch key) */
    if (copland_cmd_push(shm, COPLAND_CMD_CREATE_SURFACE,
                         80, 40, AL2_W, AL2_H, (int32_t)C_TITLE_BG,
                         COPLAND_SURF_VISIBLE) != 0) {
        ser_puts("[ALTR2] cmd ring full\n");
        m4k_exit(1);
    }

    int guard = 0;
    int slot = -1;
    while (guard++ < 200000) {
        m4k_yield();
        for (int i = 0; i < COPLAND_MAX_SURFACES; i++)
            if (shm->surfaces[i].in_use &&
                !shm->surfaces[i].buffer_ptr &&
                shm->surfaces[i].w == AL2_W) {
                slot = i;
                break;
            }
        if (slot >= 0) break;
    }
    if (slot < 0) {
        ser_puts("[ALTR2] surface allocation timeout\n");
        m4k_exit(1);
    }
    shm->surfaces[slot].buffer_ptr = (uint32_t)(uintptr_t)al_buf;
    ser_puts("[ALTR2] surface ready\n");

    int need_render = 1;
    for (;;) {
        if (!(shm->surfaces[slot].flags & COPLAND_SURF_VISIBLE))
            m4k_exit(0);

        while (al_mb->read_idx != al_mb->write_idx) {
            unsigned char ch = al_mb->buf[al_mb->read_idx];
            al_mb->read_idx = (al_mb->read_idx + 1) % AL_MB_SIZE;
            al_key(ch);
            need_render = 1;
        }

        if (need_render) {
            draw_all();
            shm->surfaces[slot].dmg_x = shm->surfaces[slot].x;
            shm->surfaces[slot].dmg_y = shm->surfaces[slot].y;
            shm->surfaces[slot].dmg_w = shm->surfaces[slot].w;
            shm->surfaces[slot].dmg_h = shm->surfaces[slot].h;
            shm->dirty = 1;
            need_render = 0;
        }
        m4k_yield();
    }
}
