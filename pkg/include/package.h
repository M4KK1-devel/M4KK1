/*
 * M4KK1 4P1 - package.h
 * Description: Package management system header for M4KK1.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <time.h>

/* Constants */
#define MAX_PACKAGES 4096
#define MAX_PACKAGE_NAME 128
#define MAX_PACKAGE_VERSION 64
#define MAX_PACKAGE_DESCRIPTION 512
#define MAX_DEPENDENCIES 64
#define MAX_FILES 1024

/* Package state */
#define PKG_STATE_INSTALLED 1
#define PKG_STATE_REMOVED   0
#define PKG_STATE_BROKEN    -1

/* Dependency types */
#define DEP_TYPE_REQUIRED   1
#define DEP_TYPE_OPTIONAL   2
#define DEP_TYPE_CONFLICTS  3

/* Dependency structure */
typedef struct {
    char name[MAX_PACKAGE_NAME];
    char version_constraint[64];
    uint32_t type;
} package_dependency_t;

/* Package info structure */
typedef struct {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_PACKAGE_VERSION];
    char description[MAX_PACKAGE_DESCRIPTION];
    char maintainer[128];
    char architecture[32];
    uint32_t state;
    uint64_t size;
    time_t install_time;
    char checksum[64];
    uint32_t dep_count;
    package_dependency_t dependencies[MAX_DEPENDENCIES];
} package_info_t;

/* File info structure */
typedef struct {
    char path[256];
    uint32_t type;
    uint32_t mode;
    uint64_t size;
    char checksum[64];
} package_file_t;

/* Package database structure */
typedef struct {
    uint32_t count;
    package_info_t packages[MAX_PACKAGES];
} package_db_t;

/* API function declarations */
int musr_pkg_init(void);
int musr_pkg_install(const char *package_path, int force);
int musr_pkg_remove(const char *package_name, int force);
int musr_pkg_update(const char *package_name);
int musr_pkg_info(const char *package_name);
int musr_pkg_list(void);
int musr_pkg_search(const char *pattern);
int musr_pkg_cleanup(void);
void musr_pkg_print_stats(void);

/* Package query functions */
package_info_t *musr_pkg_find(const char *name);
int musr_pkg_is_installed(const char *name);
int musr_pkg_get_size(const char *name);

/* Dependency management */
int musr_pkg_resolve_dependencies(char **packages,
                                  int package_count);
int musr_pkg_check_conflicts(package_info_t *pkg);

/* Package verification */
int musr_pkg_verify_checksum(const char *package_path);
int musr_pkg_verify_signature(const char *package_path);

/* Package creation tools */
int musr_pkg_create(const char *package_name,
                    const char *version,
                    const char *description,
                    char **files, int file_count);
int musr_pkg_add_dependency(const char *package_path,
                            const char *dep_name,
                            const char *version_constraint,
                            int type);
int musr_pkg_add_file(const char *package_path,
                      const char *file_path,
                      const char *package_file_path);

/* Package repository management */
int musr_pkg_repo_add(const char *name, const char *url);
int musr_pkg_repo_remove(const char *name);
int musr_pkg_repo_update(const char *name);
int musr_pkg_repo_list(void);

/* Advanced functions */
int musr_pkg_downgrade(const char *package_name,
                       const char *version);
int musr_pkg_verify_system(void);
int musr_pkg_autoremove(void);
int musr_pkg_autoclean(void);
