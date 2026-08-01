#include "m4sh.h"

#define YEAR0       1970
#define SECS_PER_MIN 60
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY  86400
#define MS_PER_SEC   1000
#define MS_PER_MIN   (60 * MS_PER_SEC)
#define MS_PER_HOUR  (3600 * MS_PER_SEC)
#define MS_PER_DAY   (86400 * MS_PER_SEC)
#define TZ_PATH      "/export/cfg/timezone"

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

static int day_of_week(int y, int m, int d)
{
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

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

static int parse2(const char *s)
{
    if (s[0] < '0' || s[0] > '9')
        return -1;
    if (s[1] < '0' || s[1] > '9')
        return -1;
    return (s[0] - '0') * 10 + (s[1] - '0');
}

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

static int parse_date(const char *s, uint32_t *out)
{
    uint32_t now = (uint32_t)musr_sc_time();
    int y, m, d, h, mi, se, wd;

    while (*s == ' ' || *s == '\t')
        s++;

    if (musr_strcmp(s, "now") == 0) {
        *out = now;
        return 0;
    }

    if (musr_strcmp(s, "today") == 0) {
        *out = (now / SECS_PER_DAY) * SECS_PER_DAY;
        return 0;
    }

    if (musr_strcmp(s, "tomorrow") == 0) {
        *out = (now / SECS_PER_DAY + 1) * SECS_PER_DAY;
        return 0;
    }

    if (musr_strcmp(s, "yesterday") == 0) {
        *out = (now / SECS_PER_DAY - 1) * SECS_PER_DAY;
        return 0;
    }

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

    if (parse_iso(s, out) == 0)
        return 0;

    return -1;
}

static int read_tz_offset(void)
{
    int fd = musr_sc_open(TZ_PATH, O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[16];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p != '+' && *p != '-')
        return 0;
    int sign = (*p == '+') ? 1 : -1;
    p++;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
        return 0;
    int h = (p[0] - '0') * 10 + (p[1] - '0');
    if (p[2] < '0' || p[2] > '9' || p[3] < '0' || p[3] > '9')
        return 0;
    int m = (p[2] - '0') * 10 + (p[3] - '0');
    return sign * (h * 3600 + m * 60);
}

static void read_tz_string(char *buf, int bufsize)
{
    buf[0] = '\0';
    int fd = musr_sc_open(TZ_PATH, O_RDONLY);
    if (fd < 0)
        return;
    int n = musr_sc_read(fd, buf, bufsize - 1);
    musr_sc_close(fd);
    if (n <= 0) {
        buf[0] = '\0';
        return;
    }
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
}

static int write_tz_string(const char *s)
{
    int fd = musr_sc_open(TZ_PATH, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        fd = musr_sc_open(TZ_PATH, O_CREAT | O_WRONLY);
        if (fd < 0)
            return -1;
    }
    int len = 0;
    while (s[len])
        len++;
    int r = musr_sc_write(fd, s, len);
    musr_sc_write(fd, "\n", 1);
    musr_sc_close(fd);
    return (r == len) ? 0 : -1;
}

static void fmt_strftime(char *buf, int bufsize,
                         const char *fmt,
                         int y, int m, int d,
                         int h, int mi, int s,
                         int wday, int bUTC,
                         const char *tz_string)
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
            case 'y': {
                int yr2 = y % 100;
                buf[pos++] = '0' + (yr2 / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (yr2 % 10);
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
            case 'I': {
                int h12 = h % 12;
                if (h12 == 0)
                    h12 = 12;
                buf[pos++] = '0' + (h12 / 10);
                if (pos < bufsize - 1)
                    buf[pos++] = '0' + (h12 % 10);
                break;
            }
            case 'p':
                buf[pos++] = (h < 12) ? 'A' : 'P';
                buf[pos++] = 'M';
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
            case 'b':
            case 'h': {
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
            case 'Z': {
                const char *tz = bUTC ? "UTC" : tz_string;
                if (!tz || !*tz)
                    tz = bUTC ? "UTC" : "LOCAL";
                while (*tz && pos < bufsize - 1)
                    buf[pos++] = *tz++;
                break;
            }
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

static void print_formatted(const char *fmt, int y, int m, int d,
                            int h, int mi, int s, int wday,
                            int bUTC, const char *tz_string)
{
    char buf[256];
    fmt_strftime(buf, sizeof(buf), fmt, y, m, d, h, mi, s, wday,
                 bUTC, tz_string);
    out_puts(buf);
}

static void print_default(int y, int m, int d,
                          int h, int mi, int s, int wday,
                          int bUTC, const char *tz_string)
{
    print_formatted("%Y-%m-%d %H:%M:%S %Z", y, m, d, h, mi, s, wday,
                    bUTC, tz_string);
    out_putc('\n');
}

static void print_diff(uint32_t t1, uint32_t t2)
{
    uint32_t diff;
    if (t1 > t2)
        diff = t1 - t2;
    else
        diff = t2 - t1;

    if (diff < 60) {
        print_u32(diff);
        out_puts(" seconds\n");
        return;
    }

    int days = diff / SECS_PER_DAY;
    int rem = diff % SECS_PER_DAY;
    int hours = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    int mins = rem / SECS_PER_MIN;
    int secs = rem % SECS_PER_MIN;

    if (days > 365) {
        int years = days / 365;
        out_puts("over ");
        print_u32(years);
        out_puts(years == 1 ? " year\n" : " years\n");
        return;
    }

    if (days > 0) {
        print_u32(days);
        out_puts(days == 1 ? " day, " : " days, ");
    }
    print_u32(hours);
    out_puts(hours == 1 ? " hour, " : " hours, ");
    print_u32(mins);
    out_puts(mins == 1 ? " minute, " : " minutes, ");
    print_u32(secs);
    out_puts(secs == 1 ? " second\n" : " seconds\n");
}

static void print_usage(void)
{
    out_puts("Usage: date [OPTIONS]\n");
    out_puts("  -f, --format FMT    Output format (strftime-style)\n");
    out_puts("  -s, --set DATETIME  Set system time (requires privilege)\n");
    out_puts("  -u, --utc           Display UTC time\n");
    out_puts("  -d, --date DATETIME Parse and display a date string\n");
    out_puts("  -c, --countdown DT  Countdown to specified date/time\n");
    out_puts("  --diff D1 D2        Difference between two dates\n");
    out_puts("  --uptime            Show system uptime\n");
    out_puts("  --hwclock           Read and display RTC time\n");
    out_puts("  --hw2sys            Set system time from RTC\n");
    out_puts("  --sys2hw            Set RTC from system time\n");
    out_puts("  --timers              List active kernel timers\n");
    out_puts("  --timezone OFFSET     Set timezone offset (e.g., +0800)\n");
    out_puts("  -h, --help            Show this help\n");
}

void musr_cmd_date(int ac, char **av)
{
    const char *szFormat = NULL;
    const char *szDateStr = NULL;
    const char *szDiff1 = NULL;
    const char *szDiff2 = NULL;
    const char *szSetTime = NULL;
    const char *szCountdown = NULL;
    const char *szTimezone = NULL;
    int bUTC = 0;
    int bUptime = 0;
    int bHwclock = 0;
    int bTimers = 0;
    int bHw2Sys = 0;
    int bSys2Hw = 0;
    int bHelp = 0;
    int i;

    for (i = 1; i < ac; i++) {
        if ((musr_strcmp(av[i], "-f") == 0 || musr_strcmp(av[i], "--format") == 0) && i + 1 < ac) {
            szFormat = av[++i];
        } else if (musr_strcmp(av[i], "-u") == 0 || musr_strcmp(av[i], "--utc") == 0) {
            bUTC = 1;
        } else if ((musr_strcmp(av[i], "-d") == 0 || musr_strcmp(av[i], "--date") == 0) && i + 1 < ac) {
            szDateStr = av[++i];
        } else if ((musr_strcmp(av[i], "-c") == 0 || musr_strcmp(av[i], "--countdown") == 0) && i + 1 < ac) {
            szCountdown = av[++i];
        } else if ((musr_strcmp(av[i], "-s") == 0 || musr_strcmp(av[i], "--set") == 0) && i + 1 < ac) {
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
        } else if (musr_strcmp(av[i], "--timezone") == 0 && i + 1 < ac) {
            szTimezone = av[++i];
        } else if (musr_strcmp(av[i], "-h") == 0 || musr_strcmp(av[i], "--help") == 0) {
            bHelp = 1;
        } else if (av[i][0] == '-') {
            out_puts("date: unknown option: ");
            out_puts(av[i]);
            out_puts("\n");
            return;
        } else {
            if (szDateStr == NULL)
                szDateStr = av[i];
        }
    }

    if (bHelp) {
        print_usage();
        return;
    }

    if (szTimezone) {
        int ok = 0;
        if (szTimezone[0] == '+' || szTimezone[0] == '-') {
            int len = 0;
            while (szTimezone[len])
                len++;
            if (len >= 5) {
                int h = 0, m = 0;
                if (szTimezone[1] >= '0' && szTimezone[1] <= '9')
                    h = (szTimezone[1] - '0') * 10;
                if (szTimezone[2] >= '0' && szTimezone[2] <= '9')
                    h += (szTimezone[2] - '0');
                if (szTimezone[3] >= '0' && szTimezone[3] <= '9')
                    m = (szTimezone[3] - '0') * 10;
                if (szTimezone[4] >= '0' && szTimezone[4] <= '9')
                    m += (szTimezone[4] - '0');
                if (h >= 0 && h <= 23 && m >= 0 && m <= 59)
                    ok = 1;
            }
        }
        if (!ok) {
            out_puts("date: invalid timezone format (use +HHMM or -HHMM)\n");
            return;
        }
        if (write_tz_string(szTimezone) != 0) {
            out_puts("date: failed to write timezone (need root)\n");
            return;
        }
        out_puts("date: timezone set to ");
        out_puts(szTimezone);
        out_puts("\n");
        return;
    }

    char tz_buf[16];
    read_tz_string(tz_buf, sizeof(tz_buf));
    if (!tz_buf[0])
        musr_strncpy(tz_buf, bUTC ? "UTC" : "LOCAL", sizeof(tz_buf)-1);
    const char *tz_string = tz_buf;
    int tz_offset = 0;
    if (!bUTC)
        tz_offset = read_tz_offset();

    if (bUptime) {
        uint32_t ms = musr_sc_uptime();
        int days = ms / MS_PER_DAY;
        ms %= MS_PER_DAY;
        int hours = ms / MS_PER_HOUR;
        ms %= MS_PER_HOUR;
        int mins = ms / MS_PER_MIN;
        int secs = (ms % MS_PER_MIN) / MS_PER_SEC;
        if (days > 0) {
            print_u32(days);
            out_puts(days == 1 ? " day, " : " days, ");
        }
        if (hours < 10) out_putc('0');
        print_u32(hours);
        out_putc(':');
        if (mins < 10) out_putc('0');
        print_u32(mins);
        out_putc(':');
        if (secs < 10) out_putc('0');
        print_u32(secs);
        out_putc('\n');
        return;
    }

    if (bHwclock) {
        uint32_t rtc[6];
        if (musr_sc_rtcread(rtc) != 0) {
            out_puts("date: failed to read RTC\n");
            return;
        }
        int y = (int)rtc[0], m = (int)rtc[1], d = (int)rtc[2];
        int h = (int)rtc[3], mi = (int)rtc[4], s = (int)rtc[5];
        int wd = day_of_week(y, m, d);
        if (szFormat) {
            print_formatted(szFormat, y, m, d, h, mi, s, wd, bUTC, tz_string);
        } else {
            print_default(y, m, d, h, mi, s, wd, bUTC, tz_string);
        }
        return;
    }

    if (bHw2Sys) {
        uint32_t rtc[6];
        if (musr_sc_rtcread(rtc) != 0) {
            out_puts("date: failed to read RTC\n");
            return;
        }
        uint32_t epoch = ymd_to_epoch((int)rtc[0], (int)rtc[1], (int)rtc[2],
                                      (int)rtc[3], (int)rtc[4], (int)rtc[5]);
        if (musr_sc_settime(epoch) != 0) {
            out_puts("date: failed to set system time (need root)\n");
            return;
        }
        out_puts("date: system time synchronized from RTC\n");
        return;
    }

    if (bSys2Hw) {
        uint32_t now = (uint32_t)musr_sc_time();
        int y, m, d, h, mi, s;
        epoch_to_ymd(now, &y, &m, &d, &h, &mi, &s);
        uint32_t rtc[6];
        rtc[0] = (uint32_t)y;
        rtc[1] = (uint32_t)m;
        rtc[2] = (uint32_t)d;
        rtc[3] = (uint32_t)h;
        rtc[4] = (uint32_t)mi;
        rtc[5] = (uint32_t)s;
        if (musr_sc_rtcwrite(rtc) != 0) {
            out_puts("date: failed to write RTC (need root)\n");
            return;
        }
        out_puts("date: RTC synchronized from system time\n");
        return;
    }

    if (bTimers) {
        uint32_t count = musr_sc_timerlist();
        print_u32(count);
        out_puts(count == 1 ? " active kernel timer\n" : " active kernel timers\n");
        return;
    }

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

    if (szSetTime) {
        uint32_t ts;
        if (parse_date(szSetTime, &ts) != 0) {
            out_puts("date: invalid date format: ");
            out_puts(szSetTime);
            out_puts("\n");
            return;
        }
        if (musr_sc_settime(ts) != 0) {
            out_puts("date: failed to set time (need root)\n");
            return;
        }
        out_puts("date: time set\n");
        return;
    }

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
                out_puts("\rCountdown finished!\n");
                return;
            }
            uint32_t remaining = target - now;
            int hours = remaining / SECS_PER_HOUR;
            int mins = (remaining % SECS_PER_HOUR) / SECS_PER_MIN;
            int secs = remaining % SECS_PER_MIN;
            int bell = (remaining <= 10) ? 1 : 0;

            out_puts("\rCountdown: ");
            if (hours < 10) out_putc('0');
            print_u32(hours);
            out_putc(':');
            if (mins < 10) out_putc('0');
            print_u32(mins);
            out_putc(':');
            if (secs < 10) out_putc('0');
            print_u32(secs);

            if (bell)
                out_putc('\a');

            uint32_t tick_start = (uint32_t)musr_sc_time();
            while ((uint32_t)musr_sc_time() == tick_start)
                ;
        }
    }

    if (szDateStr) {
        uint32_t ts;
        if (parse_date(szDateStr, &ts) != 0) {
            out_puts("date: invalid date: ");
            out_puts(szDateStr);
            out_puts("\n");
            return;
        }
        ts += tz_offset;
        int y, m, d, h, mi, s;
        epoch_to_ymd(ts, &y, &m, &d, &h, &mi, &s);
        int wd = day_of_week(y, m, d);
        if (szFormat) {
            print_formatted(szFormat, y, m, d, h, mi, s, wd, bUTC, tz_string);
        } else {
            print_default(y, m, d, h, mi, s, wd, bUTC, tz_string);
        }
        return;
    }

    uint32_t now = (uint32_t)musr_sc_time();
    now += tz_offset;
    int y, m, d, h, mi, s;
    epoch_to_ymd(now, &y, &m, &d, &h, &mi, &s);
    int wd = day_of_week(y, m, d);
    if (szFormat) {
        print_formatted(szFormat, y, m, d, h, mi, s, wd, bUTC, tz_string);
        out_putc('\n');
    } else {
        print_default(y, m, d, h, mi, s, wd, bUTC, tz_string);
    }
}
