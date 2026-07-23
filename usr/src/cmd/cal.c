/*
 * M4KK1 4P1 - cal.c
 * Description: cal command - display calendar
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static int is_leap(int year)
{
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

static int month_days(int month, int year)
{
    static const int md[12] = {31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
    if (month == 1 && is_leap(year))
        return 29;
    return md[month];
}

static int day_of_week(int d, int m, int y)
{
    if (m < 2) {
        m += 12;
        y--;
    }
    int k = y % 100, j = y / 100;
    return (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
}

/**
 * musr_cmd_cal - Display a calendar
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_cal(int ac, char **av)
{
    (void)ac;
    (void)av;
    int m = 7, y = 2026;
    if (ac >= 2) {
        m = 0;
        const char *p = av[1];
        while (*p >= '0' && *p <= '9') {
            m = m * 10 + (*p - '0');
            p++;
        }
        if (m < 1 || m > 12) {
            m = 7;
        }
    }
    if (ac >= 3) {
        y = 0;
        const char *p = av[2];
        while (*p >= '0' && *p <= '9') {
            y = y * 10 + (*p - '0');
            p++;
        }
        if (y < 1)
            y = 2026;
    }
    m--;
    int days = month_days(m, y);
    int start = day_of_week(1, m, y);
    start = (start + 6) % 7;
    static const char *mon_names[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    out_puts("      ");
    out_puts(mon_names[m]);
    out_putc(' ');
    print_u32(y);
    out_puts("\n");
    out_puts("Su Mo Tu We Th Fr Sa\n");
    for (int i = 0; i < start; i++)
        out_puts("   ");
    for (int d = 1; d <= days; d++) {
        if (d < 10)
            out_putc(' ');
        print_u32(d);
        out_putc(' ');
        if ((start + d) % 7 == 0 && d < days)
            out_puts("\n");
    }
    out_puts("\n");
}
