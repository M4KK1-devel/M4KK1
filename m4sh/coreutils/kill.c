/*
 * M4KK1 4P1 - kill.c
 * Description: kill command - send signal to process
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_kill - Send a signal to a process (not implemented)
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_kill(int ac, char **av)
{
    (void)ac;
    (void)av;
    out_puts("kill: not implemented (no signal support)\n");
}
