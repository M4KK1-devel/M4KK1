#include "../m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

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

void _start(void)
{
    char user[64];
    char pass[64];
    int attempts;

    for (;;) {
        attempts = 0;
        while (attempts < 3) {
            ser_puts("M4KK1 login: ");
            if (read_line(user, sizeof(user)) <= 0)
                continue;
            ser_puts("Password: ");
            read_pass(pass, sizeof(pass));

            /* 认证完全依赖 /export/cfg/passwd.db，无硬编码凭据 */
            passwd_entry_t entry;
            int auth_ok = 0;
            if (musr_getpwnam(user, &entry) == 0 &&
                musr_verify_password(pass, entry.password_hash)) {
                ser_puts("Login successful\n");
                m4k_setuid(entry.uid);
                m4k_setgid(entry.gid);
                m4k_chdir(entry.home);
                m4k_register_session("ttyS0", m4k_getpid(), entry.username);

                /* 记录登录日志 */
                musr_sc_mkdir("/var/log");
                {
                    int lfd = musr_sc_open("/var/log/userlog",
                        O_WRONLY | O_CREAT | 0x00002000);
                    if (lfd >= 0) {
                        char lbuf[256];
                        uint32_t now = (uint32_t)musr_sc_time();
                        char tbuf[16];
                        int ti = 0;
                        if (now == 0) { tbuf[ti++] = '0'; }
                        else { char rev[16]; int ri = 0;
                            while (now) { rev[ri++] = '0' + (now % 10);
                                          now /= 10; }
                            while (ri > 0) { tbuf[ti++] = rev[--ri]; } }
                        tbuf[ti] = '\0';
                        musr_strncpy(lbuf, tbuf, sizeof(lbuf)-1);
                        musr_strncat(lbuf, "  ", sizeof(lbuf)-musr_strlen(lbuf)-1);
                        musr_strncat(lbuf, entry.username, sizeof(lbuf)-musr_strlen(lbuf)-1);
                        musr_strncat(lbuf, "  login  success  ttyS0\n", sizeof(lbuf)-musr_strlen(lbuf)-1);
                        musr_sc_write(lfd, lbuf, musr_strlen(lbuf));
                        musr_sc_close(lfd);
                    }
                }

                m4k_spawn(entry.shell, 0);
                ser_puts("login: spawn failed\n");
                break;
            }
            ser_puts("Login incorrect\n");
            attempts++;
        }
        if (attempts >= 3)
            ser_puts("Too many failed attempts\n");
    }
}
