/*
 * M4KK1 4P1 - export.c
 * Description: export command - set environment variables
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_export - Set or display environment variables
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_export(int ac, char **av)
{
    if (ac < 2) {
        for (int i = 0; i < envar_cnt; i++) {
            out_puts(envars[i]);
            out_puts("\n");
        }
        return;
    }
    if (envar_cnt < 16)
        musr_strcpy(envars[envar_cnt++], av[1]);
}
