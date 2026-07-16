/*
 * M4KK1 4P1 - main.c
 * Description: Package manager main program for M4KK1.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "package.h"

/* Program version */
#define PKGMGR_VERSION "1.0.0"

/* Command line options */
static struct option long_options[] = {
    {"install", required_argument, 0, 'i'},
    {"remove", required_argument, 0, 'r'},
    {"update", required_argument, 0, 'u'},
    {"info", required_argument, 0, 'I'},
    {"list", no_argument, 0, 'l'},
    {"search", required_argument, 0, 's'},
    {"force", no_argument, 0, 'f'},
    {"yes", no_argument, 0, 'y'},
    {"version", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

/* Show help */
void show_help(const char *program_name)
{
    printf("M4KK1 Package Manager v%s\n", PKGMGR_VERSION);
    printf("Usage: %s [options] [package/file]\n",
           program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -i, --install <file>    Install package\n");
    printf("  -r, --remove <package>  Remove package\n");
    printf("  -u, --update <package>  Update package\n");
    printf("  -I, --info <package>    Show package info\n");
    printf("  -l, --list              List packages\n");
    printf("  -s, --search <pattern>  Search packages\n");
    printf("  -f, --force             Force operation\n");
    printf("  -y, --yes               Auto-confirm\n");
    printf("  -v, --version           Show version\n");
    printf("  -h, --help              Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --install package.m4pkg\n", program_name);
    printf("  %s --remove vim\n", program_name);
    printf("  %s --list\n", program_name);
    printf("  %s --search editor\n", program_name);
}

/* Show version */
void show_version(void)
{
    printf("M4KK1 Package Manager v%s\n", PKGMGR_VERSION);
    printf("Copyright (C) 2025 M4KK1 Development Team\n");
    printf("License: GPL-3.0\n");
}

/* Main function */
int main(int argc, char *argv[])
{
    int opt;
    int option_index = 0;
    int force = 0;
    int yes = 0;
    const char *package_name = NULL;
    const char *package_file = NULL;
    int action = 0;

    /* Parse command line arguments */
    while ((opt = getopt_long(argc, argv, "i:r:u:I:ls:fyvh",
                              long_options,
                              &option_index)) != -1) {
        switch (opt) {
        case 'i':
            action = 'i';
            package_file = optarg;
            break;
        case 'r':
            action = 'r';
            package_name = optarg;
            break;
        case 'u':
            action = 'u';
            package_name = optarg;
            break;
        case 'I':
            action = 'I';
            package_name = optarg;
            break;
        case 'l':
            action = 'l';
            break;
        case 's':
            action = 's';
            package_name = optarg;
            break;
        case 'f':
            force = 1;
            break;
        case 'y':
            yes = 1;
            break;
        case 'v':
            show_version();
            return 0;
        case 'h':
            show_help(argv[0]);
            return 0;
        default:
            show_help(argv[0]);
            return 1;
        }
    }

    /* Check arguments */
    if (action == 0) {
        printf("Error: Must specify an operation\n");
        show_help(argv[0]);
        return 1;
    }

    /* Initialize package system */
    if (musr_pkg_init() != 0) {
        printf("Error: Cannot initialize package system\n");
        return 1;
    }

    /* Execute operation */
    switch (action) {
    case 'i':
        if (!package_file) {
            printf("Error: Must specify package file\n");
            return 1;
        }
        return musr_pkg_install(package_file, force);

    case 'r':
        if (!package_name) {
            printf("Error: Must specify package name\n");
            return 1;
        }
        return musr_pkg_remove(package_name, force);

    case 'u':
        if (!package_name) {
            printf("Error: Must specify package name\n");
            return 1;
        }
        return musr_pkg_update(package_name);

    case 'I':
        if (!package_name) {
            printf("Error: Must specify package name\n");
            return 1;
        }
        return musr_pkg_info(package_name);

    case 'l':
        return musr_pkg_list();

    case 's':
        if (!package_name) {
            printf("Error: Must specify search pattern\n");
            return 1;
        }
        return musr_pkg_search(package_name);

    default:
        printf("Error: Unknown operation\n");
        return 1;
    }

    return 0;
}
