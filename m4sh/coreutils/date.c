/*
 * M4KK1 4P1 - date.c
 * Description: Display, parse, and manage system date and time
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../m4sh.h"

#define YEAR0       1970
#define SECS_PER_MIN 60
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY  86400

static const char *szMonths[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *szMonthsAbbr[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *szWeekdays[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char *szWeekdaysAbbr[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static int is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int month_days(int y, int m)
{
    static const int dim[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (m < 1 || m > 12)
        return 0;
    int d = dim[m - 1];
    if (m == 2 && is_leap(y))
        d++;
    return d;
}

/*
 * Convert Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 * to broken-down date/time.
 */
static void epoch_to_ymd(uint32_t t, int *y, int *m, int *d,
                         int *h, int *mi, int *s)
{
    *h = (t / SECS_PER_HOUR) % 24;
    *mi = (t / SECS_PER_MIN) % 60;
    *s = t % SECS_PER_MIN;

    int days = t / SECS_PER_DAY;
    int yr = YEAR0;
    while (1) {
        int ld = is_leap(yr) ? 366 : 365;
        if (days < ld)
            break;
        days -= ld;
        yr++;
    }
    *y = yr;

    int mo = 1;
    while (mo <= 12) {
        int dim = month_days(yr, mo);
        if (days < dim)
            break;
        days -= dim;
        mo++;
    }
    *m = mo;
    *d = days + 1;
}

/*
 * Compute day-of-week (0=Sunday, 6=Saturday) using Tomohiko
 * Sakamoto's algorithm.
 */
static int day_of_week(int y, int m, int d)
{
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

/*
 * Convert broken-down date/time back to Unix timestamp.
 * Assumes input is in UTC. Uses 400-year cycle for efficiency.
 */
static uint32_t ymd_to_epoch(int y, int m, int d,
                             int h, int mi, int s)
{
    int yr = YEAR0;
    uint32_t days = 0;
    while (yr < y) {
        days += is_leap(yr) ? 366 : 365;
        yr++;
    }
    for (int mo = 1; mo < m; mo++)
        days += month_days(y, mo);
    days += d - 1;
    return days * SECS_PER_DAY + h * SECS_PER_HOUR + mi * SECS_PER_MIN + s;
}

/* Parse a 2-digit number from a string, return value or -1 */
static int parse2(const char *s)
{
    if (s[0] < '0' || s[0] > '9')
        return -1;
    if (s[1] < '0' || s[1] > '9')
        return -1;
    return (s[0] - '0') * 10 + (s[1] - '0');
}

/* Parse a 4-digit number from a string, return value or -1 */
static int parse4(const char *s)
{
    int v = 0;
    for (int i = 0; i < 4; i++) {
        if (s[i] < '0' || s[i] > '9')
            return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

/*
 * Parse an ISO-format timestamp: "YYYY-MM-DD" or
 * "YYYY-MM-DD HH:MM:SS".  Returns 0 on success, -1 on error.
 */
static int parse_iso(const char *s, uint32_t *out)
{
    int y, m, d, h = 0, mi = 0, se = 0;
    y = parse4(s);
    if (y < YEAR0 || s[4] != '-')
        return -1;
    m = parse2(s + 5);
    if (m < 1 || m > 12 || s[7] != '-')
        return -1;
    d = parse2(s + 8);
    if (d < 1 || d > month_days(y, m))
        return -1;
    if (s[10] == ' ' || s[10] == 'T') {
        h = parse2(s + 11);
        if (h < 0 || h > 23 || s[13] != ':')
            return -1;
        mi = parse2(s + 14);
        if (mi < 0 || mi > 59)
            return -1;
        if (s[16] == ':') {
            se = parse2(s + 17);
            if (se < 0 || se > 59)
                return -1;
        }
    } else if (s[10] != '\0') {
        return -1;
    }
    *out = ymd_to_epoch(y, m, d, h, mi, se);
    return 0;
}

/* Compare two strings case-insensitively for prefix matching */
static int strpref_ci(const char *s, const char *pref)
{
    while (*pref) {
        char cs = *s;
        char cp = *pref;
        if (cs >= 'A' && cs <= 'Z')
            cs += 32;
        if (cp >= 'A' && cp <= 'Z')
            cp += 32;
        if (cs != cp)
            return 0;
        s++;
        pref++;
    }
    return 1;
}

/* Parse a natural-language unit suffix, return multiplier */
static int parse_unit(const char *s, int *unit_secs)
{
    if (strpref_ci(s, "second") || strpref_ci(s, "sec")) {
        *unit_secs = 1;
        return 1;
    }
    if (strpref_ci(s, "minute") || strpref_ci(s, "min")) {
        *unit_secs = SECS_PER_MIN;
        return 1;
    }
    if (strpref_ci(s, "hour")) {
        *unit_secs = SECS_PER_HOUR;
        return 1;
    }
    if (strpref_ci(s, "day")) {
        *unit_secs = SECS_PER_DAY;
        return 1;
    }
    if (strpref_ci(s, "week")) {
        *unit_secs = 7 * SECS_PER_DAY;
        return 1;
    }
    if (strpref_ci(s, "month")) {
        *unit_secs = 30 * SECS_PER_DAY;
        return 1;
    }
    if (strpref_ci(s, "year")) {
        *unit_secs = 365 * SECS_PER_DAY;
        return 1;
    }
    return 0;
}

/* Parse a number from a string */
static int parse_int(const char *s, int *val)
{
    int sign = 1;
    int i = 0;
    if (s[0] == '+') {
        i = 1;
    } else if (s[0] == '-') {
        sign = -1;
        i = 1;
    }
    int v = 0;
    int found = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
        found = 1;
    }
    if (!found)
        return -1;
    *val = v * sign;
    return i;
}

/*
 * Parse a natural-language date string.
 * Supported formats:
 *   "now"            -> current time
 *   "today"          -> today at 00:00:00
 *   "tomorrow"       -> tomorrow at 00:00:00
 *   "yesterday"      -> yesterday at 00:00:00
 *   "+N unit"        -> N units from now
 *   "-N unit"        -> N units before now
 *   "YYYY-MM-DD"     -> absolute date at 00:00:00
 *   "YYYY-MM-DD HH:MM:SS" -> absolute date/time
 *   "next monday"    -> next occurrence of weekday
 * Returns 0 on success, -1 on error.
 */
static int parse_date(const char *s, uint32_t *out)
{
    uint32_t now = (uint32_t)musr_sc_time();
    int y, m, d, h, mi, se, wd;

    /* Skip leading whitespace */
    while (*s == ' ' || *s == '\t')
        s++;

    /* now */
    if (musr_strcmp(s, "now") == 0) {
        *out = now;
        return 0;
    }

    /* today */
    if (musr_strcmp(s, "today") == 0) {
        *out = (now / SECS_PER_DAY) * SECS_PER_DAY;
        return 0;
    }

    /* tomorrow */
    if (musr_strcmp(s, "tomorrow") == 0) {
        *out = (now / SECS_PER_DAY + 1) * SECS_PER_DAY;
        return 0;
    }

    /* yesterday */
    if (musr_strcmp(s, "yesterday") == 0) {
        *out = (now / SECS_PER_DAY - 1) * SECS_PER_DAY;
        return 0;
    }

    /* Relative offsets: +N unit or -N unit */
    if (s[0] == '+' || s[0] == '-') {
        int val;
        int n = parse_int(s, &val);
        if (n > 0) {
            while (s[n] == ' ' || s[n] == '\t')
                n++;
            int unit_secs;
            if (parse_unit(s + n, &unit_secs)) {
                *out = now + val * unit_secs;
                return 0;
            }
        }
    }

    /* "next weekday" */
    if (strpref_ci(s, "next ")) {
        const char *wds = s + 5;
        int target = -1;
        for (int i = 0; i < 7; i++) {
            if (strpref_ci(wds, szWeekdays[i]) ||
                strpref_ci(wds, szWeekdaysAbbr[i])) {
                target = i;
                break;
            }
        }
        if (target >= 0) {
            epoch_to_ymd(now, &y, &m, &d, &h, &mi, &se);
            uint32_t today_start = (now / SECS_PER_DAY) * SECS_PER_DAY;
            wd = day_of_week(y, m, d);
            int diff = (target - wd + 7) % 7;
            if (diff == 0)
                diff = 7;
            *out = today_start + diff * SECS_PER_DAY;
            return 0;
        }
    }

    /* "last weekday" */
    if (strpref_ci(s, "last ")) {
        const char *wds = s + 5;
        int target = -1;
        for (int i = 0; i < 7; i++) {
            if (strpref_ci(wds, szWeekdays[i]) ||
                strpref_ci(wds, szWeekdaysAbbr[i])) {
                target = i;
                break;
            }
        }
        if (target >= 0) {
            epoch_to_ymd(now, &y, &m, &d, &h, &mi, &se);
            uint32_t today_start = (now / SECS_PER_DAY) * SECS_PER_DAY;
            wd = day_of_week(y, m, d);
            int diff = (wd - target + 7) % 7;
            if (diff == 0)
                diff = 7;
            *out = today_start - diff * SECS_PER_DAY;
            return 0;
        }
    }

    /* Try ISO format */
    if (parse_iso(s, out) == 0)
        return 0;

    return -1;
}

/*
 * Format a date/time into a buffer using strftime-style specifiers.
 * Supported: %Y %m %d %H %M %S %s %a %A %b %B %n %%
 */
static void fmt_strftime(char *buf, int bufsize,
                         const char *fmt,
                         int y, int m, int d,
                         int h, int mi, int s,
                         int wday, int bUTC)
{
    int pos = 0;
    while (*fmt && pos < bufsize - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
            case 'Y': {
                int tmp = y;
                char ys[8];
                int p = 0;
                if (tmp < 1000) {
                    ys[p++] = '0';
                    if (tmp < 100) {
                        ys[p++] = '0';
                        if (tmp < 10)
                            ys[p++] = '0';
                    }
                }
                /* Print year digits */
                char yd[8];
                int q = 0;
                do {
                    yd[q++] = '0' + (tmp % 10);
                    tmp /= 10;
                } while (tmp > 0);
                while (q > 0)
                    ys[p++] = yd[--q];
                ys[p] = '\0';
                for (int i = 0; ys[i] && pos < bufsize - 1; i++)
                    buf[pos++] = ys[i];
                break;
            }
            case 'm':
                buf[pos++] = '0' + (m / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (m % 10);
                break;
            case 'd':
                buf[pos++] = '0' + (d / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (d % 10);
                break;
            case 'H':
                buf[pos++] = '0' + (h / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (h % 10);
                break;
            case 'M':
                buf[pos++] = '0' + (mi / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (mi % 10);
                break;
            case 'S':
                buf[pos++] = '0' + (s / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (s % 10);
                break;
            case 's': {
                uint32_t ts = ymd_to_epoch(y, m, d, h, mi, s);
                char tsb[16];
                int tp = 0;
                do {
                    tsb[tp++] = '0' + (ts % 10);
                    ts /= 10;
                } while (ts > 0);
                while (tp > 0)
                    buf[pos++] = tsb[--tp];
                break;
            }
            case 'a': {
                const char *wdn = szWeekdaysAbbr[wday];
                while (*wdn && pos < bufsize - 1)
                    buf[pos++] = *wdn++;
                break;
            }
            case 'A': {
                const char *wdn = szWeekdays[wday];
                while (*wdn && pos < bufsize - 1)
                    buf[pos++] = *wdn++;
                break;
            }
            case 'b': {
                const char *mn = szMonthsAbbr[m - 1];
                while (*mn && pos < bufsize - 1)
                    buf[pos++] = *mn++;
                break;
            }
            case 'B': {
                const char *mn = szMonths[m - 1];
                while (*mn && pos < bufsize - 1)
                    buf[pos++] = *mn++;
                break;
            }
            case 'z':
                if (bUTC) {
                    buf[pos++] = '+';
                    buf[pos++] = '0';
                    buf[pos++] = '0';
                    buf[pos++] = '0';
                    buf[pos++] = '0';
                } else {
                    buf[pos++] = 'L';
                    buf[pos++] = 'O';
                    buf[pos++] = 'C';
                    buf[pos++] = 'A';
                    buf[pos++] = 'L';
                }
                break;
            case 'Z':
                if (bUTC) {
                    buf[pos++] = 'U';
                    buf[pos++] = 'T';
                    buf[pos++] = 'C';
                } else {
                    buf[pos++] = 'L';
                    buf[pos++] = 'O';
                    buf[pos++] = 'C';
                    buf[pos++] = 'A';
                    buf[pos++] = 'L';
                }
                break;
            case 'n':
                buf[pos++] = '\n';
                break;
            case '%':
                buf[pos++] = '%';
                break;
            case '\0':
                goto done;
            default:
                buf[pos++] = '%';
                if (pos < bufsize - 1 && *fmt)
                    buf[pos++] = *fmt;
                break;
            }
            if (*fmt)
                fmt++;
        } else {
            buf[pos++] = *fmt++;
        }
    }
done:
    buf[pos] = '\0';
}

/* Print a formatted date string directly using out_puts */
static void print_formatted(const char *fmt, int y, int m, int d,
                            int h, int mi, int s, int wday, int bUTC)
{
    char buf[256];
    fmt_strftime(buf, sizeof(buf), fmt, y, m, d, h, mi, s, wday, bUTC);
    out_puts(buf);
}

/* Print default date format: "YYYY-MM-DD HH:MM:SS" */
static void print_default(int y, int m, int d,
                          int h, int mi, int s)
{
    print_u32(y);
    out_putc('-');
    if (m < 10) out_putc('0');
    print_u32(m);
    out_putc('-');
    if (d < 10) out_putc('0');
    print_u32(d);
    out_puts("  ");
    if (h < 10) out_putc('0');
    print_u32(h);
    out_putc(':');
    if (mi < 10) out_putc('0');
    print_u32(mi);
    out_putc(':');
    if (s < 10) out_putc('0');
    print_u32(s);
    out_putc('\n');
}

/* Compute absolute difference between two timestamps */
static void print_diff(uint32_t t1, uint32_t t2)
{
    uint32_t diff;
    if (t1 > t2)
        diff = t1 - t2;
    else
        diff = t2 - t1;

    int days = diff / SECS_PER_DAY;
    int rem = diff % SECS_PER_DAY;
    int hours = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    int mins = rem / SECS_PER_MIN;
    int secs = rem % SECS_PER_MIN;

    if (days > 0) {
        print_u32(days);
        out_puts(" days, ");
    }
    print_u32(hours);
    out_puts(" hours, ");
    print_u32(mins);
    out_puts(" minutes, ");
    print_u32(secs);
    out_puts(" seconds\n");
}

/* Format uptime: "X days, HH:MM:SS" */
static void print_uptime(void)
{
    uint32_t now = (uint32_t)musr_sc_time();
    /* Assume boot timestamp is stored; we approximate by using
       a file-based approach: read /proc/uptime if available.
       Otherwise, just show the current uptime from sysinfo.
       Here we use a simpler approach: we just show the timestamp
       as "System time" since proper uptime needs boot-time tracking. */
    int y, m, d, h, mi, s;
    epoch_to_ymd(now, &y, &m, &d, &h, &mi, &s);
    out_puts("System time: ");
    print_default(y, m, d, h, mi, s);
}

/**
 * musr_cmd_date - Display, parse, and manage date and time
 * @ac: argument count
 * @av: argument vector
 *
 * Usage: date [OPTIONS]
 *   -f FMT     Custom output format
 *   -u         Show UTC time
 *   -d STR     Parse natural-language date
 *   -c DATETIME  Countdown to specified time
 *   --diff D1 D2  Difference between two dates
 *   --uptime   Show system uptime
 *   -s DATETIME  Set system time (privileged)
 */
void musr_cmd_date(int ac, char **av)
{
    const char *szFormat = "%Y-%m-%d %H:%M:%S";
    const char *szDateStr = NULL;
    const char *szDiff1 = NULL;
    const char *szDiff2 = NULL;
    const char *szSetTime = NULL;
    const char *szCountdown = NULL;
    int bUTC = 0;
    int bUptime = 0;
    int bHwclock = 0;
    int bTimers = 0;
    int bHw2Sys = 0;
    int bSys2Hw = 0;
    int i;

    for (i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "-f") == 0 && i + 1 < ac) {
            szFormat = av[++i];
        } else if (musr_strcmp(av[i], "-u") == 0) {
            bUTC = 1;
        } else if (musr_strcmp(av[i], "-d") == 0 && i + 1 < ac) {
            szDateStr = av[++i];
        } else if (musr_strcmp(av[i], "-c") == 0 && i + 1 < ac) {
            szCountdown = av[++i];
        } else if (musr_strcmp(av[i], "-s") == 0 && i + 1 < ac) {
            szSetTime = av[++i];
        } else if (musr_strcmp(av[i], "--diff") == 0) {
            if (i + 2 < ac) {
                szDiff1 = av[++i];
                szDiff2 = av[++i];
            } else {
                out_puts("date: --diff requires two date arguments\n");
                return;
            }
        } else if (musr_strcmp(av[i], "--uptime") == 0) {
            bUptime = 1;
        } else if (musr_strcmp(av[i], "--hwclock") == 0) {
            bHwclock = 1;
        } else if (musr_strcmp(av[i], "--timers") == 0) {
            bTimers = 1;
        } else if (musr_strcmp(av[i], "--hw2sys") == 0) {
            bHw2Sys = 1;
        } else if (musr_strcmp(av[i], "--sys2hw") == 0) {
            bSys2Hw = 1;
        } else if (av[i][0] == '-') {
            out_puts("date: unknown option: ");
            out_puts(av[i]);
            out_puts("\n");
            return;
        } else {
            /* Treat as date string if -d not already given */
            if (szDateStr == NULL)
                szDateStr = av[i];
        }
    }

    /* --uptime: show system time info */
    if (bUptime) {
        print_uptime();
        return;
    }

    /* --hwclock: show hardware clock (not available, report) */
    if (bHwclock) {
        out_puts("date: hardware clock not available on this system\n");
        return;
    }

    /* --timers: list kernel timers (not available) */
    if (bTimers) {
        out_puts("date: kernel timer listing not available\n");
        return;
    }

    /* --hw2sys / --sys2hw: RTC sync (not available) */
    if (bHw2Sys || bSys2Hw) {
        out_puts("date: RTC management not available\n");
        return;
    }

    /* --diff: compute difference between two dates */
    if (szDiff1 && szDiff2) {
        uint32_t t1, t2;
        if (parse_date(szDiff1, &t1) != 0) {
            out_puts("date: invalid date: ");
            out_puts(szDiff1);
            out_puts("\n");
            return;
        }
        if (parse_date(szDiff2, &t2) != 0) {
            out_puts("date: invalid date: ");
            out_puts(szDiff2);
            out_puts("\n");
            return;
        }
        print_diff(t1, t2);
        return;
    }

    /* -s: set system time (stub - set not available) */
    if (szSetTime) {
        uint32_t ts;
        if (parse_date(szSetTime, &ts) != 0) {
            out_puts("date: invalid date format: ");
            out_puts(szSetTime);
            out_puts("\n");
            return;
        }
        out_puts("date: setting system time not supported\n");
        return;
    }

    /* -c: countdown */
    if (szCountdown) {
        uint32_t target;
        if (parse_date(szCountdown, &target) != 0) {
            out_puts("date: invalid countdown target: ");
            out_puts(szCountdown);
            out_puts("\n");
            return;
        }
        while (1) {
            uint32_t now = (uint32_t)musr_sc_time();
            if (now >= target) {
                out_puts("Countdown finished!\n");
                return;
            }
            uint32_t remaining = target - now;
            int hours = remaining / SECS_PER_HOUR;
            int mins = (remaining % SECS_PER_HOUR) / SECS_PER_MIN;
            int secs = remaining % SECS_PER_MIN;

            out_puts("\rCountdown: ");
            if (hours < 10) out_putc('0');
            print_u32(hours);
            out_putc(':');
            if (mins < 10) out_putc('0');
            print_u32(mins);
            out_putc(':');
            if (secs < 10) out_putc('0');
            print_u32(secs);

            /* Busy-wait approx 1 second */
            uint32_t tick_start = (uint32_t)musr_sc_time();
            while ((uint32_t)musr_sc_time() == tick_start)
                ;
        }
    }

    /* -d: parse and display a date */
    if (szDateStr) {
        uint32_t ts;
        if (parse_date(szDateStr, &ts) != 0) {
            out_puts("date: invalid date: ");
            out_puts(szDateStr);
            out_puts("\n");
            return;
        }
        int y, m, d, h, mi, s;
        epoch_to_ymd(ts, &y, &m, &d, &h, &mi, &s);
        int wd = day_of_week(y, m, d);
        print_formatted(szFormat, y, m, d, h, mi, s, wd, bUTC);
        out_putc('\n');
        return;
    }

    /* Default: display current time */
    uint32_t now = (uint32_t)musr_sc_time();
    int y, m, d, h, mi, s;
    epoch_to_ymd(now, &y, &m, &d, &h, &mi, &s);
    int wd = day_of_week(y, m, d);
    print_formatted(szFormat, y, m, d, h, mi, s, wd, bUTC);
    out_putc('\n');
}
