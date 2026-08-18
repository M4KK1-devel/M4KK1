/*
 * M4KK1 4P1 - fsck.c
 * Description: Recovery-mode filesystem checker (userspace).
 *              Validates the YAFS-mounted root tree: essential files,
 *              passwd.db / groups.db integrity, /bin contents.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

static void put_dec(int v)
{
    char tmp[16];
    int i = 0;
    if (v == 0) { tmp[i++] = '0'; }
    while (v > 0 && i < 15) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0) {
        i--;
        ser_putc(tmp[i]);
    }
}

static int check_file(const char *path)
{
    int fd = musr_sc_open(path, 0); /* O_RDONLY */
    if (fd < 0) {
        ser_puts("fsck: ERROR  missing: ");
        ser_puts(path);
        ser_puts("\n");
        return -1;
    }
    char buf[16];
    int n = musr_sc_read(fd, buf, sizeof(buf));
    musr_sc_close(fd);
    if (n < 0) {
        ser_puts("fsck: ERROR  unreadable: ");
        ser_puts(path);
        ser_puts("\n");
        return -1;
    }
    ser_puts("fsck: OK     ");
    ser_puts(path);
    ser_puts(" (");
    put_dec(n);
    ser_puts(" bytes)\n");
    return 0;
}

static int check_dir(const char *path)
{
    int fd = musr_sc_open(path, 0);
    if (fd < 0) {
        ser_puts("fsck: ERROR  missing dir: ");
        ser_puts(path);
        ser_puts("\n");
        return -1;
    }
    struct dirent buf[32];
    int n = musr_sc_getdents(fd, buf, 32);
    musr_sc_close(fd);
    if (n <= 0) {
        ser_puts("fsck: ERROR  unreadable dir: ");
        ser_puts(path);
        ser_puts("\n");
        return -1;
    }
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i].name[0])
            count++;
    }
    ser_puts("fsck: OK     ");
    ser_puts(path);
    ser_puts(" (");
    put_dec(count);
    ser_puts(" entries)\n");
    return 0;
}

void _start(void)
{
    int errors = 0;

    ser_puts("fsck: M4KK1 recovery filesystem check\n");
    ser_puts("----------------------------------------\n");

    if (check_dir("/") < 0) errors++;
    if (check_dir("/bin") < 0) errors++;
    if (check_dir("/export/cfg") < 0) errors++;

    if (check_file("/export/cfg/passwd.db") < 0) errors++;
    if (check_file("/export/cfg/groups.db") < 0) errors++;
    if (check_file("/export/cfg/timezone") < 0) errors++;

    {
        passwd_entry_t entries[32];
        int n = musr_read_passwd_db(entries, 32);
        if (n < 0) {
            ser_puts("fsck: ERROR  passwd.db unparsable\n");
            errors++;
        } else {
            ser_puts("fsck: OK     passwd.db format (");
            put_dec(n);
            ser_puts(" users)\n");
        }
    }

    if (musr_sc_open("/bin/login", 0) >= 0) {
        ser_puts("fsck: OK     /bin/login present\n");
    } else {
        ser_puts("fsck: WARN   /bin/login missing\n");
        errors++;
    }
    if (musr_sc_open("/bin/m4sh", 0) >= 0) {
        ser_puts("fsck: OK     /bin/m4sh present\n");
    } else {
        ser_puts("fsck: WARN   /bin/m4sh missing\n");
        errors++;
    }

    ser_puts("----------------------------------------\n");
    if (errors == 0) {
        ser_puts("fsck: NO ERRORS FOUND\n");
    } else {
        ser_puts("fsck: ERRORS FOUND (");
        put_dec(errors);
        ser_puts(")\n");
    }

    /* Recovery flow: fsck runs in place of init (spawn, no fork).
     * Hand over to the serial login so the system stays usable. */
    ser_puts("fsck: starting login...\n");
    int r = m4k_spawn("/bin/login", 0);
    ser_puts("fsck: login failed to start (ret=");
    ser_puts((r < 0) ? "err" : "0");
    ser_puts(")\n");
    for (;;) {
        /* no login available; spin (recovery dead end) */
    }
}
