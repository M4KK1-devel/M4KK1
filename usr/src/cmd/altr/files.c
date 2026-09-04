/*
 * ==== ALTR2 files.c — explorer tree, load/save, path completion ====
 * M4KK1 4P1 - usr/src/cmd/altr/files.c
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "altr.h"

struct al_tree_ent tree_ents[AL_MAX_FILES];
int tree_count = 0;
char tree_cwd[AL_MAX_PATH] = "/export/root";
int tree_sel = 0;

static void path_join(char *dst, int dstsz, const char *dir,
                      const char *name)
{
    int dl = al_strlen(dir);
    int i = 0;
    if (dl >= dstsz - 2) return;
    for (; i < dl; i++) dst[i] = dir[i];
    if (dl && dst[dl - 1] != '/' && dl < dstsz - 2)
        dst[dl++] = '/';
    for (int j = 0; name[j] && dl < dstsz - 1; j++)
        dst[dl++] = name[j];
    dst[dl] = 0;
}

void tree_reload(void)
{
    tree_count = 0;
    int fd = musr_sc_open(tree_cwd, O_RDONLY);
    if (fd < 0) return;
    static struct dirent ents[16];
    for (;;) {
        int n = musr_sc_getdents(fd, ents, 16);
        if (n <= 0) break;
        for (int i = 0; i < n && tree_count < AL_MAX_FILES; i++) {
            char *nm = ents[i].name;
            int dot = (nm[0] == '.' &&
                       (!nm[1] || (nm[1] == '.' && !nm[2])));
            if (dot) continue;
            musr_strncpy(tree_ents[tree_count].name, nm,
                         DIRENT_NAME_MAX - 1);
            tree_ents[tree_count].is_dir = (ents[i].type == 2);
            tree_count++;
        }
        if (tree_count >= AL_MAX_FILES) break;
    }
    musr_sc_close(fd);
}

void tree_open_sel(void)
{
    if (tree_sel < 0 || tree_sel >= tree_count) return;
    char full[192];
    path_join(full, sizeof(full), tree_cwd, tree_ents[tree_sel].name);
    if (tree_ents[tree_sel].is_dir) {
        musr_strncpy(tree_cwd, full, sizeof(tree_cwd) - 1);
        tree_sel = 0;
        tree_reload();
        return;
    }
    S.focus = FOC_EDITOR;
    altr_load(full);
}

int al_save(struct al_doc *d, const char *path)
{
    int fd = musr_sc_open(path, O_WRONLY | O_CREAT);
    if (fd < 0) return -1;
    for (int i = 0; i < d->nlines; i++) {
        int l = al_line_len(d, i);
        if (l > 0)
            musr_sc_write(fd, d->lines[i], l);
        if (musr_sc_write(fd, "\n", 1) != 1) {
            musr_sc_close(fd);
            return -1;
        }
    }
    musr_sc_close(fd);
    d->dirty = 0;
    return 0;
}

/* complete the last path component of buf (used by palette open) */
void al_tab_complete_path(char *buf, int bufsz)
{
    int len = al_strlen(buf);
    int slash = -1;
    for (int i = len - 1; i >= 0; i--)
        if (buf[i] == '/') { slash = i; break; }

    char dir[160], pfx[64];
    if (slash >= 0) {
        if (slash == 0) { dir[0] = '/'; dir[1] = 0; }
        else {
            for (int i = 0; i < slash; i++) dir[i] = buf[i];
            dir[slash] = 0;
        }
        musr_strncpy(pfx, buf + slash + 1, sizeof(pfx) - 1);
    } else {
        musr_strncpy(dir, tree_cwd, sizeof(dir) - 1);
        musr_strncpy(pfx, buf, sizeof(pfx) - 1);
    }

    static struct dirent ents[16];
    char cand[8][DIRENT_NAME_MAX];
    int cand_dir[8];
    int nc = 0;
    int pfxlen = al_strlen(pfx);
    int fd = musr_sc_open(dir, O_RDONLY);
    if (fd < 0) return;
    for (;;) {
        int n = musr_sc_getdents(fd, ents, 16);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (nc >= 8) break;
            char *nm = ents[i].name;
            if (nm[0] == '.') continue;
            int match = 1;
            for (int k = 0; k < pfxlen; k++)
                if (nm[k] != pfx[k]) { match = 0; break; }
            if (!match) continue;
            musr_strncpy(cand[nc], nm, DIRENT_NAME_MAX - 1);
            cand_dir[nc] = (ents[i].type == 2);
            nc++;
        }
        if (nc >= 8) break;
    }
    musr_sc_close(fd);
    if (nc == 0) return;

    int lcp = al_strlen(cand[0]);
    for (int i = 1; i < nc; i++) {
        int l = 0;
        while (cand[i][l] && cand[0][l] && cand[i][l] == cand[0][l])
            l++;
        lcp = l < lcp ? l : lcp;
    }

    int o = 0;
    if (slash > 0) {
        for (int i = 0; i <= slash && o < bufsz - 1; i++)
            buf[o++] = buf[i];
    } else if (slash == 0) {
        buf[o++] = '/';
    } else {
        for (int i = 0; dir[i] && o < bufsz - 1; i++)
            buf[o++] = dir[i];
        if (o < bufsz - 1 && buf[o - 1] != '/')
            buf[o++] = '/';
    }
    for (int i = 0; i < lcp && o < bufsz - 1; i++)
        buf[o++] = cand[0][i];
    if (nc == 1 && cand_dir[0] && o < bufsz - 1)
        buf[o++] = '/';
    buf[o] = 0;
}
