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

            uint32_t login_uid = 1001;
            uint32_t login_gid = 1001;
            const char *login_home = "/home/testuser";
            const char *login_shell = "/bin/m4sh";
            const char *login_user = "testuser";
            int auth_ok = 0;

            if (musr_strcmp(user, "root") == 0 &&
                musr_strcmp(pass, "123456") == 0) {
                login_uid = 0;
                login_gid = 0;
                login_home = "/export/root";
                login_user = "root";
                auth_ok = 1;
            }
            if (musr_strcmp(user, "testuser") == 0 &&
                musr_strcmp(pass, "yakumakki") == 0) {
                login_uid = 1001;
                login_gid = 1001;
                login_home = "/home/testuser";
                login_user = "testuser";
                auth_ok = 1;
            }

            if (auth_ok) {
                ser_puts("Login successful\n");
                m4k_setuid(login_uid);
                m4k_setgid(login_gid);
                m4k_chdir(login_home);
                m4k_register_session("ttyS0", m4k_getpid(), login_user);

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
                        musr_strncat(lbuf, login_user, sizeof(lbuf)-musr_strlen(lbuf)-1);
                        musr_strncat(lbuf, "  login  success  ttyS0\n", sizeof(lbuf)-musr_strlen(lbuf)-1);
                        musr_sc_write(lfd, lbuf, musr_strlen(lbuf));
                        musr_sc_close(lfd);
                    }
                }

                m4k_spawn(login_shell, 0);
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
