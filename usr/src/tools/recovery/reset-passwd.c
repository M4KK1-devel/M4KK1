/*
 * M4KK1 4P1 - reset-passwd.c
 * Description: Recovery-mode password reset tool.
 *              Prompts for a username and a new password, writes a
 *              fresh salted SHA-256 hash into /export/cfg/passwd.db.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

static int read_line(char *buf, int max, int echo)
{
    int i = 0;
    while (i < max - 1) {
        int ch = ser_getc();
        if (ch < 0) continue;
        if (ch == '\n' || ch == '\r') {
            ser_putc('\n');
            break;
        }
        if (ch == '\b' || ch == 0x7F) {
            if (i > 0) {
                i--;
                ser_puts("\b \b");
            }
            continue;
        }
        buf[i++] = (char)ch;
        ser_putc(echo ? (char)ch : ' ');
    }
    buf[i] = '\0';
    return i;
}

void _start(void)
{
    char user[64];
    char pass1[64];
    char pass2[64];
    char hash[256];

    ser_puts("reset-passwd: M4KK1 recovery password reset\n");
    ser_puts("--------------------------------------------\n");

    ser_puts("Username: ");
    if (read_line(user, sizeof(user), 1) <= 0) {
        ser_puts("reset-passwd: no username given, aborting\n");
        m4k_exit(1);
    }

    passwd_entry_t entries[32];
    int nentries = musr_read_passwd_db(entries, 32);
    if (nentries <= 0) {
        ser_puts("reset-passwd: cannot read passwd.db\n");
        m4k_exit(1);
    }

    int idx = -1;
    for (int i = 0; i < nentries; i++) {
        if (musr_strcmp(entries[i].username, user) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        ser_puts("reset-passwd: unknown user: ");
        ser_puts(user);
        ser_puts("\n");
        m4k_exit(1);
    }

    ser_puts("New password: ");
    read_line(pass1, sizeof(pass1), 0);
    ser_puts("Retype new password: ");
    read_line(pass2, sizeof(pass2), 0);

    if (musr_strcmp(pass1, pass2) != 0) {
        ser_puts("reset-passwd: passwords do not match, aborting\n");
        m4k_exit(1);
    }
    if (musr_strlen(pass1) < 6) {
        ser_puts("reset-passwd: password too short (< 6 chars), aborting\n");
        m4k_exit(1);
    }

    /* Salted SHA-256 hash, same scheme as login (m4sh/lib/pwd.c) */
    musr_make_password_hash(pass1, hash);
    musr_strncpy(entries[idx].password_hash, hash,
        sizeof(entries[idx].password_hash) - 1);

    if (musr_update_passwd_db(entries, nentries) != 0) {
        ser_puts("reset-passwd: failed to write passwd.db\n");
        m4k_exit(1);
    }

    ser_puts("reset-passwd: password for user '");
    ser_puts(entries[idx].username);
    ser_puts("' updated\n");
    m4k_exit(0);
}
