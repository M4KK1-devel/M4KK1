/*
 * M4KK1 4P1 - namespace.h
 * Description: Per-process namespace definitions
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_NS_ENTRIES      16
#define M4K_NS_PATH_MAX     256

#define M4K_MNT_RDONLY      0x0001
#define M4K_MNT_NOSUID      0x0002
#define M4K_MNT_PRIVATE     0x0004
#define M4K_MNT_BIND        0x0008

typedef struct {
    char mnt_path[M4K_NS_PATH_MAX];
    char target_path[M4K_NS_PATH_MAX];
    uint32_t mnt_flags;
} m4k_ns_entry_t;

typedef struct {
    m4k_ns_entry_t entries[M4K_NS_ENTRIES];
    uint32_t entry_count;
} m4k_namespace_t;

void mkrn_ns_init(m4k_namespace_t *ns);
int mkrn_ns_set(m4k_namespace_t *ns, const char *path, const char *target, uint32_t flags);
int mkrn_ns_resolve(m4k_namespace_t *ns, const char *path, char *out, uint32_t out_sz);
void mkrn_ns_copy(m4k_namespace_t *dst, const m4k_namespace_t *src);
