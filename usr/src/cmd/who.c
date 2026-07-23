#include "m4sh.h"

static void print_hex8(unsigned char c)
{
    const char *hex = "0123456789ABCDEF";
    ser_putc(hex[(c >> 4) & 0xF]);
    ser_putc(hex[c & 0xF]);
}

static void dump_hex(const char *label, const char *data, int len)
{
    c_red();
    out_puts(label);
    out_puts(": ");
    c_wht();
    for (int j = 0; j < len; j++) {
        print_hex8((unsigned char)data[j]);
        ser_putc(' ');
    }
    ser_putc('\n');
}

void musr_cmd_who(int ac, char **av)
{
    (void)ac;
    (void)av;

    struct m4k_session_info sessions[M4K_SESSION_MAX];
    int n = m4k_get_session_list(sessions, M4K_SESSION_MAX);
    if (n <= 0) {
        c_red();
        out_puts("who: no active sessions\n");
        c_rst();
        return;
    }

    c_grn();
    out_puts("USER       TTY        LOGIN TIME  FROM\n");
    c_wht();

    for (int i = 0; i < n; i++) {
        dump_hex("username", sessions[i].username, 16);
        dump_hex("tty", sessions[i].tty, 16);

        int ulen = musr_strlen(sessions[i].username);
        out_puts(sessions[i].username);
        for (int p = ulen; p < 10; p++) ser_putc(' ');

        int tlen = musr_strlen(sessions[i].tty);
        out_puts(sessions[i].tty);
        for (int p = tlen; p < 10; p++) ser_putc(' ');

        char tbuf[16];
        int ti = 0;
        uint32_t t = sessions[i].login_time;
        if (t == 0) { tbuf[ti++] = '0'; }
        else { char rev[16]; int ri = 0; while (t) { rev[ri++] = '0' + (t % 10); t /= 10; } while (ri > 0) { tbuf[ti++] = rev[--ri]; } }
        tbuf[ti] = '\0';
        out_puts(tbuf);
        for (int p = ti; p < 10; p++) ser_putc(' ');

        out_puts("local\n");
    }
    c_rst();
}
