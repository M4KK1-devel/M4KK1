/*
 * M4KK1 4P1 - cu.c
 * Description: Change user (replace su, §6.8.4)
 *
 * Syntax: cu <username>
 * Prompts for password, on success sets EUID to target user.
 * With no arguments, prompts for root password.
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

void musr_cmd_cu(int ac, char **av)
{
    char target[64];

    if (ac >= 2) {
        musr_strncpy(target, av[1], sizeof(target)-1);
    } else {
        musr_strncpy(target, "root", sizeof(target)-1);
    }

    /* Look up target user */
    passwd_entry_t entry;
    if (musr_getpwnam(target, &entry) != 0) {
        c_red();
        out_puts("cu: unknown user: ");
        out_puts(target);
        out_putc('\n');
        c_rst();
        return;
    }

    ser_puts("Password: ");
    char pass[64];
    read_pass_noecho(pass, sizeof(pass));

    if (!musr_verify_password(pass, entry.password_hash)) {
        c_red();
        out_puts("cu: incorrect password\n");
        c_rst();
        return;
    }

    if (musr_sc_setuid(entry.uid) != 0) {
        c_red();
        out_puts("cu: permission denied\n");
        c_rst();
        return;
    }

    /* Chdir to target user's home */
    musr_sc_chdir(entry.home);

    c_grn();
    out_puts("cu: switched to ");
    out_puts(target);
    out_putc('\n');
    c_rst();
}
