/*
 * M4KK1 4P1 - package.c
 * Description: Package management core implementation for M4KK1.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "package.h"

/* Package database file */
#define PACKAGE_DB_PATH "/var/lib/pkgmgr/packages.db"
#define PACKAGE_DB_BACKUP "/var/lib/pkgmgr/packages.db.bak"

/* Package install root */
#define PACKAGE_ROOT "/usr/local"

/* Package state */
#define PKG_STATE_INSTALLED 1
#define PKG_STATE_REMOVED   0
#define PKG_STATE_BROKEN    -1

/* Dependency types */
#define DEP_TYPE_REQUIRED   1
#define DEP_TYPE_OPTIONAL   2
#define DEP_TYPE_CONFLICTS  3

/* Global package database */
static package_db_t package_db;

/* String trim */
static char *str_trim(char *str)
{
    char *end;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';

    return str;
}

/* String split */
static int str_split(char *str, char delim,
                     char **tokens, int max_tokens)
{
    int count = 0;
    char *token = strtok(str, &delim);

    while (token && count < max_tokens) {
        tokens[count++] = str_trim(token);
        token = strtok(NULL, &delim);
    }

    return count;
}

/* File exists check */
static int file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

/* Directory exists check */
static int dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* mkdir -p implementation */
static int mkdir_p(const char *path)
{
    char *path_copy = strdup(path);
    char *p = path_copy;
    int result = 0;

    if (!path_copy) return -1;

    if (*p == '/') p++;

    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (!dir_exists(path_copy) &&
                mkdir(path_copy, 0755) != 0) {
                result = -1;
                break;
            }
            *p = '/';
        }
        p++;
    }

    if (result == 0 && !dir_exists(path)) {
        result = mkdir(path, 0755);
    }

    free(path_copy);
    return result;
}

/* Package database load */
static int package_db_load(void)
{
    FILE *fp = fopen(PACKAGE_DB_PATH, "r");
    if (!fp) {
        memset(&package_db, 0, sizeof(package_db));
        return 0;
    }

    if (fread(&package_db.count,
              sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    for (uint32_t i = 0;
         i < package_db.count && i < MAX_PACKAGES; i++) {
        package_info_t *pkg = &package_db.packages[i];

        if (fread(pkg, sizeof(package_info_t), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/* Package database save */
static int package_db_save(void)
{
    FILE *fp;
    char dir_path[256];

    snprintf(dir_path, sizeof(dir_path), "%s",
             PACKAGE_DB_PATH);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir_path);
        *last_slash = '/';
    }

    if (file_exists(PACKAGE_DB_PATH)) {
        rename(PACKAGE_DB_PATH, PACKAGE_DB_BACKUP);
    }

    fp = fopen(PACKAGE_DB_PATH, "w");
    if (!fp) {
        if (file_exists(PACKAGE_DB_BACKUP)) {
            rename(PACKAGE_DB_BACKUP, PACKAGE_DB_PATH);
        }
        return -1;
    }

    if (fwrite(&package_db.count,
               sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    for (uint32_t i = 0; i < package_db.count; i++) {
        package_info_t *pkg = &package_db.packages[i];

        if (fwrite(pkg, sizeof(package_info_t), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/* Package database find */
static package_info_t *package_db_find(const char *name)
{
    for (uint32_t i = 0; i < package_db.count; i++) {
        if (strcmp(package_db.packages[i].name, name) == 0) {
            return &package_db.packages[i];
        }
    }
    return NULL;
}

/* Package database add */
static int package_db_add(package_info_t *pkg)
{
    if (package_db.count >= MAX_PACKAGES) {
        return -1;
    }

    if (package_db_find(pkg->name)) {
        return -1;
    }

    memcpy(&package_db.packages[package_db.count],
           pkg, sizeof(package_info_t));
    package_db.count++;

    return 0;
}

/* Package database remove */
static int package_db_remove(const char *name)
{
    for (uint32_t i = 0; i < package_db.count; i++) {
        if (strcmp(package_db.packages[i].name, name) == 0) {
            memmove(&package_db.packages[i],
                    &package_db.packages[i + 1],
                    (package_db.count - i - 1) *
                    sizeof(package_info_t));
            package_db.count--;
            return 0;
        }
    }
    return -1;
}

/* Package file extraction */
static int package_extract_files(
    const char *package_path, const char *install_prefix)
{
    char command[512];
    snprintf(command, sizeof(command),
             "tar -tzf \"%s\" | head -20", package_path);

    FILE *fp = popen(command, "r");
    if (!fp) {
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        str_trim(line);
        if (line[0] == '/') {
            char file_path[512];
            snprintf(file_path, sizeof(file_path),
                     "%s%s", install_prefix, line);

            char *last_slash = strrchr(file_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                mkdir_p(file_path);
                *last_slash = '/';
            }
        }
    }

    pclose(fp);
    return 0;
}

/* Package file removal */
static int package_remove_files(const char *package_name)
{
    package_info_t *pkg = package_db_find(package_name);
    if (!pkg) {
        return -1;
    }

    pkg->state = PKG_STATE_REMOVED;
    return 0;
}

/* Dependency check */
static int package_check_dependencies(package_info_t *pkg)
{
    for (int i = 0;
         i < pkg->dep_count && i < MAX_DEPENDENCIES; i++) {
        package_dependency_t *dep = &pkg->dependencies[i];

        if (dep->type == DEP_TYPE_REQUIRED) {
            package_info_t *dep_pkg =
                package_db_find(dep->name);
            if (!dep_pkg ||
                dep_pkg->state != PKG_STATE_INSTALLED) {
                printf("Error: Missing dependency: %s\n",
                       dep->name);
                return -1;
            }
        }
    }

    return 0;
}

/* Package management API */

int musr_pkg_init(void)
{
    mkdir_p("/var/lib/pkgmgr");

    if (package_db_load() != 0) {
        printf("Warning: Failed to load package db\n");
        memset(&package_db, 0, sizeof(package_db));
    }

    return 0;
}

int musr_pkg_install(const char *package_path, int force)
{
    package_info_t pkg_info;
    char metadata_path[512];

    printf("Installing package: %s\n", package_path);

    snprintf(metadata_path, sizeof(metadata_path),
             "%s/METADATA", package_path);
    if (!file_exists(metadata_path)) {
        printf("Error: Invalid package format\n");
        return -1;
    }

    memset(&pkg_info, 0, sizeof(pkg_info));
    strcpy(pkg_info.name, "unknown");
    pkg_info.state = PKG_STATE_INSTALLED;

    if (!force &&
        package_check_dependencies(&pkg_info) != 0) {
        printf("Error: Dependency check failed\n");
        return -1;
    }

    if (package_extract_files(package_path,
                              PACKAGE_ROOT) != 0) {
        printf("Error: Failed to extract files\n");
        return -1;
    }

    if (package_db_add(&pkg_info) != 0) {
        printf("Error: Failed to add to database\n");
        return -1;
    }

    if (package_db_save() != 0) {
        printf("Warning: Failed to save database\n");
    }

    printf("Package installed successfully\n");
    return 0;
}

int musr_pkg_remove(const char *package_name, int force)
{
    package_info_t *pkg = package_db_find(package_name);

    if (!pkg) {
        printf("Error: Package not found: %s\n",
               package_name);
        return -1;
    }

    if (pkg->state != PKG_STATE_INSTALLED) {
        printf("Error: Package not installed: %s\n",
               package_name);
        return -1;
    }

    printf("Removing package: %s\n", package_name);

    if (!force) {
        for (uint32_t i = 0; i < package_db.count; i++) {
            package_info_t *other_pkg =
                &package_db.packages[i];
            if (other_pkg == pkg) continue;

            for (int j = 0;
                 j < other_pkg->dep_count; j++) {
                if (strcmp(
                        other_pkg->dependencies[j].name,
                        package_name) == 0) {
                    printf("Error: Required by: %s\n",
                           other_pkg->name);
                    return -1;
                }
            }
        }
    }

    if (package_remove_files(package_name) != 0) {
        printf("Error: Failed to remove files\n");
        return -1;
    }

    if (package_db_remove(package_name) != 0) {
        printf("Error: Failed to remove from database\n");
        return -1;
    }

    if (package_db_save() != 0) {
        printf("Warning: Failed to save database\n");
    }

    printf("Package removed successfully\n");
    return 0;
}

int musr_pkg_info(const char *package_name)
{
    package_info_t *pkg = package_db_find(package_name);

    if (!pkg) {
        printf("Package not found: %s\n", package_name);
        return -1;
    }

    printf("Package: %s\n", pkg->name);
    printf("Version: %s\n", pkg->version);
    printf("Description: %s\n", pkg->description);
    printf("State: %s\n",
           pkg->state == PKG_STATE_INSTALLED
           ? "installed" : "removed");
    printf("Install Date: %s", ctime(&pkg->install_time));

    if (pkg->dep_count > 0) {
        printf("Dependencies:\n");
        for (int i = 0; i < pkg->dep_count; i++) {
            printf("  %s (%s)\n",
                   pkg->dependencies[i].name,
                   pkg->dependencies[i].type ==
                   DEP_TYPE_REQUIRED
                   ? "required" : "optional");
        }
    }

    return 0;
}

int musr_pkg_list(void)
{
    printf("Installed packages:\n");
    printf("%-20s %-10s %-15s %s\n",
           "Name", "Version", "State", "Description");
    printf("%s\n",
           "-------------------------------"
           "-----------------------------");

    for (uint32_t i = 0; i < package_db.count; i++) {
        package_info_t *pkg = &package_db.packages[i];

        printf("%-20s %-10s %-15s %s\n",
               pkg->name,
               pkg->version,
               pkg->state == PKG_STATE_INSTALLED
               ? "installed" : "removed",
               pkg->description);
    }

    return 0;
}

int musr_pkg_search(const char *pattern)
{
    printf("Searching for: %s\n", pattern);

    int found = 0;
    for (uint32_t i = 0; i < package_db.count; i++) {
        package_info_t *pkg = &package_db.packages[i];

        if (strstr(pkg->name, pattern) ||
            strstr(pkg->description, pattern)) {
            printf("  %s - %s\n",
                   pkg->name, pkg->description);
            found++;
        }
    }

    if (found == 0) {
        printf("No packages found matching: %s\n",
               pattern);
    }

    return 0;
}

int musr_pkg_update(const char *package_name)
{
    package_info_t *pkg = package_db_find(package_name);

    if (!pkg) {
        printf("Error: Package not found: %s\n",
               package_name);
        return -1;
    }

    printf("Updating package: %s\n", package_name);
    printf("Package update not yet implemented\n");

    return 0;
}

int musr_pkg_cleanup(void)
{
    printf("Cleaning up package cache...\n");

    system("rm -rf /var/cache/pkgmgr/*");

    printf("Cleanup completed\n");
    return 0;
}

void musr_pkg_print_stats(void)
{
    uint32_t installed_count = 0;
    uint32_t total_size = 0;

    for (uint32_t i = 0; i < package_db.count; i++) {
        if (package_db.packages[i].state ==
            PKG_STATE_INSTALLED) {
            installed_count++;
            total_size += package_db.packages[i].size;
        }
    }

    printf("Package Statistics:\n");
    printf("  Total packages: %u\n", package_db.count);
    printf("  Installed: %u\n", installed_count);
    printf("  Total size: %u KB\n", total_size / 1024);
    printf("  Database: %s\n", PACKAGE_DB_PATH);
}
