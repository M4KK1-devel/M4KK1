/*
 * M4KK1 4P1 - exec.c
 * Description: Process execution handler.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/console.h"

int
mkrn_execve(const char *pFilename, char *const argv[],
            char *const envp[])
{
    (void)argv;
    (void)envp;
    mkrn_console_write("execve: not implemented yet\n");
    return -1;
}
