/*
 * M4KK1 4P1 - userlog.c
 * Description: Show login history (§6.8.6)
 *
 * Reads /var/log/userlog (if exists) and displays entries.
 * Syntax: userlog [username]
 */

#include "m4sh.h"

void musr_cmd_userlog(int ac, char **av)
{
    const char *filter = NULL;
    if (ac >= 2) filter = av[1];

    int fd = musr_sc_open("/var/log/userlog", O_RDONLY);
    if (fd < 0) {
        c_ylw();
        out_puts("userlog: no login history\n");
        c_rst();
        return;
    }

    char buf[2048];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0) {
        out_puts("userlog: empty log\n");
        return;
    }
    buf[n] = '\0';

    c_cyn();
    out_puts("DATE                USER     ACTION   STATUS  TTY\n");
    c_wht();

    char *line = buf;
    int count = 0;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') end++;
        char save = *end;
        *end = '\0';

        if (*line && *line != '#') {
            if (filter) {
                /* Check if username field matches */
                const char *p = line;
                while (*p && *p != ' ') p++;
                if (*p) p++;
                /* p now points to username */
                if (musr_strpref(p, filter)) {
                    out_puts(line);
                    out_putc('\n');
                    count++;
                }
            } else {
                out_puts(line);
                out_putc('\n');
                count++;
            }
        }

        *end = save;
        line = end + 1;
    }

    if (count == 0) {
        out_puts("(no entries)\n");
    }
}
