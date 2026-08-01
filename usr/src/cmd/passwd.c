/*
 * M4KK1 4P1 - passwd.c
 * Description: Change password (§6.8.3)
 *
 * Prompts for new password twice, updates hash in /export/cfg/passwd.db.
 * With argument (prime group): force-change another user's password.
 */

#include "m4sh.h"

static void read_pass_noecho(char *buf, int max)
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
        ser_putc(' ');
    }
    buf[i] = '\0';
}

void musr_cmd_passwd(int ac, char **av)
{
    uint32_t cur_uid = musr_sc_getuid();
    uint32_t cur_euid = musr_sc_geteuid();

    char target_user[64];
    int is_self = 1;

    if (ac >= 2) {
        musr_strncpy(target_user, av[1], sizeof(target_user)-1);
        is_self = 0;
    }

    passwd_entry_t entries[32];
    int count = musr_read_passwd_db(entries, 32);
    if (count <= 0) {
        c_red();
        out_puts("passwd: cannot read passwd.db\n");
        c_rst();
        return;
    }

    int target_idx = -1;
    for (int i = 0; i < count; i++) {
        if (is_self) {
            if (entries[i].uid == cur_uid) {
                target_idx = i;
                musr_strncpy(target_user, entries[i].username, sizeof(target_user)-1);
                break;
            }
        } else {
            if (musr_strcmp(entries[i].username, target_user) == 0) {
                target_idx = i;
                break;
            }
        }
    }

    if (target_idx < 0) {
        c_red();
        out_puts("passwd: user not found\n");
        c_rst();
        return;
    }

    if (!is_self && cur_euid != 0) {
        c_red();
        out_puts("passwd: permission denied (requires prime group)\n");
        c_rst();
        return;
    }

    if (is_self && cur_uid != entries[target_idx].uid) {
        c_red();
        out_puts("passwd: cannot change another user's password\n");
        c_rst();
        return;
    }

    char old_pass[64], new_pass[64], confirm[64];

    if (is_self) {
        ser_puts("Current password: ");
        read_pass_noecho(old_pass, sizeof(old_pass));
        if (!musr_verify_password(old_pass, entries[target_idx].password_hash)) {
            c_red();
            out_puts("passwd: incorrect password\n");
            c_rst();
            return;
        }
    }

    ser_puts("New password: ");
    read_pass_noecho(new_pass, sizeof(new_pass));
    ser_puts("Retype new password: ");
    read_pass_noecho(confirm, sizeof(confirm));

    if (musr_strcmp(new_pass, confirm) != 0) {
        c_red();
        out_puts("passwd: passwords do not match\n");
        c_rst();
        return;
    }

    if (musr_strlen(new_pass) < 4) {
        c_red();
        out_puts("passwd: password too short (min 4 chars)\n");
        c_rst();
        return;
    }

    musr_make_password_hash(new_pass, entries[target_idx].password_hash);

    if (musr_update_passwd_db(entries, count) == 0) {
        c_grn();
        out_puts("passwd: password changed successfully\n");
        c_rst();
    } else {
        c_red();
        out_puts("passwd: failed to update passwd.db\n");
        c_rst();
    }
}
