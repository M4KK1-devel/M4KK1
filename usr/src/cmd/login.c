#include "m4sh.h"

/* 临时 fallback: 用明文 strcmp 验证密码。
   当 libmusr 的 musr_verify_password 不可用或不匹配密码格式时，
   取消下一行的注释以启用此回退（注意需要同时把调用处改为
   musr_verify_password_fallback）。 */
/*
static int musr_verify_password_fallback(const char *input, const char *hash) {
    return musr_strcmp(input, hash) == 0;
}
*/

static int read_line(char *buf, int max) {
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

static void read_pass(char *buf, int max) {
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

void musr_cmd_login(int ac, char **av) {
    (void)ac;
    (void)av;

    if (musr_sc_getuid() != 0) {
        c_red();
        out_puts("login: already logged in\n");
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
        if (read_line(user, sizeof(user)) <= 0) continue;

        ser_puts("Password: ");
        read_pass(pass, sizeof(pass));

        // ---- 1. 尝试从 passwd.db 获取用户 ----
        passwd_entry_t entry;
        int found = musr_getpwnam(user, &entry);

#if defined(DEBUG_LOGIN)
        c_wht();
        out_puts("[DEBUG] musr_getpwnam returned ");
        print_u32(found);
        out_puts("\n");
        if (found == 0) {
            out_puts("[DEBUG] username: ");
            out_puts(entry.username);
            out_puts(", uid: ");
            print_u32(entry.uid);
            out_puts(", hash: ");
            out_puts(entry.password_hash);
            out_puts("\n");
        }
        c_rst();
#endif

        int verified = 0;
        // ---- 2. 如果数据库查询成功，使用 musr_verify_password ----
        if (found == 0) {
            verified = musr_verify_password(pass, entry.password_hash);
        }

        // ---- 3. 回退：硬编码 root:123456（用于紧急访问） ----
        if (!verified && musr_strcmp(user, "root") == 0 && musr_strcmp(pass, "123456") == 0) {
            // 构造一个临时 entry
            musr_strcpy(entry.username, "root");
            entry.uid = 0;
            entry.gid = 0;
            musr_strcpy(entry.home, "/root");
            musr_strcpy(entry.shell, "/bin/m4sh");
            musr_strcpy(entry.gecos, "Superuser");
            musr_strcpy(entry.password_hash, "123456");
            verified = 1;
            c_wht();
            out_puts("WARNING: Using hardcoded root password (123456). Fix your passwd.db\n");
            c_rst();
        }

        // ---- 4. 验证成功 ----
        if (verified) {
            c_grn();
            out_puts("Login successful\n");
            c_rst();

            // 切换身份
            musr_sc_setuid(entry.uid);
            musr_sc_setgid(entry.gid);
            musr_sc_chdir(entry.home);

            // 注册会话（tty 名称固定为 ttyS0，可改进为动态获取）
            m4k_register_session("ttyS0", musr_sc_getpid(), entry.username);

            // 记录登录日志
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
                    musr_strcpy(lbuf, tbuf);
                    musr_strcat(lbuf, "  ");
                    musr_strcat(lbuf, entry.username);
                    musr_strcat(lbuf, "  login  success  ttyS0\n");
                    musr_sc_write(lfd, lbuf, musr_strlen(lbuf));
                    musr_sc_close(lfd);
                }
            }

            musr_login_ok = 1;
            return;
        }

        // ---- 5. 失败 ----
        c_red();
        out_puts("Login incorrect\n");
        c_rst();
        attempts++;
    }

    out_puts("Too many failed attempts\n");
}
