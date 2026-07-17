#include "../m4sh.h"

static int read_line(char *buf, int max)
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
        ser_putc((char)ch);
    }
    buf[i] = '\0';
    return i;
}

static void read_pass(char *buf, int max)
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

void musr_cmd_login(int ac, char **av)
{
    (void)ac;
    (void)av;

    uint32_t cur_uid = musr_sc_getuid();
    if (cur_uid != 0) {
        c_red();
        out_puts("login: already logged in (use su to switch)\n");
        c_rst();
        return;
    }

    char user[64];
    char pass[64];
    int attempts = 0;

    while (attempts < 3) {
        c_grn();
        out_puts("M4KK1 login: ");
        c_wht();
        if (read_line(user, sizeof(user)) <= 0)
            continue;

        ser_puts("Password: ");
        read_pass(pass, sizeof(pass));

        if (musr_strcmp(user, "root") == 0 && musr_strcmp(pass, "123456") == 0) {
            c_grn();
            out_puts("Login successful\n");
            c_rst();
            return;
        }

        c_red();
        out_puts("Login incorrect\n");
        c_rst();
        attempts++;
    }

    out_puts("Too many failed attempts\n");
}
