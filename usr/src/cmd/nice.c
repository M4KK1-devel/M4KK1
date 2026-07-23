/*
 * M4KK1 4P1 - nice.c
 * Description: nice command - set process priority
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

/**
 * musr_cmd_nice - Set process priority (not implemented)
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_nice(int ac, char **av)
{
    (void)ac;
    (void)av;
    out_puts("nice: not implemented\n");
}
