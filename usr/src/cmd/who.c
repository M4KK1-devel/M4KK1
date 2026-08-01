#include "m4sh.h"

void musr_cmd_who(int ac, char **av)
{
    (void)ac;
    (void)av;

    struct m4k_session_info sessions[M4K_SESSION_MAX];
    int count = m4k_get_session_list(sessions, M4K_SESSION_MAX);
    
    if (count <= 0) {
        c_ylw();
        out_puts("who: no active sessions\n");
        c_rst();
        return;
    }

    c_grn();
    out_puts("USER       TTY        LOGIN TIME  FROM\n");
    c_wht();

    for (int i = 0; i < count; i++) {
        if (!sessions[i].active) continue;

        /* Try to get username from passwd.db */
        passwd_entry_t entry;
        char username[64];
        if (musr_getpwuid(sessions[i].uid, &entry) == 0) {
            musr_strncpy(username, entry.username, sizeof(username)-1);
        } else {
            /* Fallback: show UID as number */
            username[0] = 'u';
            username[1] = 'i';
            username[2] = 'd';
            username[3] = '=';
            int pos = 4;
            uint32_t uid = sessions[i].uid;
            if (uid == 0) {
                username[pos++] = '0';
            } else {
                char rev[16];
                int ri = 0;
                while (uid) {
                    rev[ri++] = '0' + (uid % 10);
                    uid /= 10;
                }
                while (ri > 0) {
                    username[pos++] = rev[--ri];
                }
            }
            username[pos] = '\0';
        }

        int ulen = musr_strlen(username);
        out_puts(username);
        for (int p = ulen; p < 10; p++) ser_putc(' ');

        int tlen = musr_strlen(sessions[i].tty);
        out_puts(sessions[i].tty);
        for (int p = tlen; p < 10; p++) ser_putc(' ');

        char tbuf[16];
        int ti = 0;
        uint32_t t = sessions[i].login_time;
        if (t == 0) {
            tbuf[ti++] = '0';
        } else {
            char rev[16];
            int ri = 0;
            while (t) {
                rev[ri++] = '0' + (t % 10);
                t /= 10;
            }
            while (ri > 0) {
                tbuf[ti++] = rev[--ri];
            }
        }
        tbuf[ti] = '\0';
        out_puts(tbuf);
        for (int p = ti; p < 10; p++) ser_putc(' ');

        out_puts("local\n");
    }
    c_rst();
}
