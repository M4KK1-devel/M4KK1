/*
 * M4KK1 4P1 - clear.c
 * Description: clear command - clear terminal screen
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

/**
 * musr_cmd_clear - Clear the terminal screen
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_clear(int ac, char **av)
{
    (void)ac;
    (void)av;
    ser_puts("\x1B[2J\x1B[H");
}
