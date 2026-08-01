#include "../m4sh.h"

static void ensure_dir(const char *path)
{
    musr_sc_mkdir(path);
}

static int build_passwd_entry(char *buf, int max,
    const char *user, const char *uid, const char *gid,
    const char *home, const char *shell, const char *gecos,
    const char *hash)
{
    int pos = 0;
    const char *fields[] = {user, uid, gid, home, shell, gecos, hash};
    for (int i = 0; i < 7; i++) {
        const char *f = fields[i];
        while (*f && pos < max - 2) buf[pos++] = *f++;
        buf[pos++] = (i < 6) ? ':' : '\n';
    }
    buf[pos] = '\0';
    return pos;
}

void musr_boot_setup(void)
{
    int fd = musr_sc_open("/export/cfg/passwd.db", O_RDONLY);
    if (fd >= 0) {
        musr_sc_close(fd);
        return;
    }

    c_ylw();
    out_puts("First boot: creating initial user database...\n");
    c_rst();

    ensure_dir("/export/cfg");
    ensure_dir("/export/cfg/testuser");
    ensure_dir("/export/PATH");
    ensure_dir("/export/home/testuser");

    char root_hash[256];
    musr_make_password_hash("123456", root_hash);
    char testuser_hash[256];
    musr_make_password_hash("yakumakki", testuser_hash);

    fd = musr_sc_open("/export/cfg/passwd.db", O_CREAT | O_WRONLY);
    if (fd < 0) {
        c_red();
        out_puts("boot: failed to create passwd.db\n");
        c_rst();
        return;
    }

    char line[512];
    int n;

    n = build_passwd_entry(line, sizeof(line),
        "root", "0", "0", "/export/root", "/bin/m4sh",
        "System Administrator", root_hash);
    musr_sc_write(fd, line, n);

    n = build_passwd_entry(line, sizeof(line),
        "testuser", "1001", "1001", "/home/testuser", "/bin/m4sh",
        "Test User", testuser_hash);
    musr_sc_write(fd, line, n);

    n = build_passwd_entry(line, sizeof(line),
        "nobody", "65534", "65534", "/export/srv/nobody", "/sbin/nologin",
        "Unprivileged", "");
    musr_sc_write(fd, line, n);

    musr_sc_close(fd);

    fd = musr_sc_open("/export/cfg/groups.db", O_CREAT | O_WRONLY);
    if (fd >= 0) {
        musr_sc_write(fd, "prime:1001:testuser,root\n", 24);
        musr_sc_write(fd, "staff:1002:\n", 12);
        musr_sc_close(fd);
    }

    fd = musr_sc_open("/export/PATH/testuser", O_CREAT | O_WRONLY);
    if (fd >= 0) {
        musr_sc_write(fd, "/home/testuser/bin:/usr/local/bin:/bin", 38);
        musr_sc_close(fd);
    }

    fd = musr_sc_open("/export/PATH/root", O_CREAT | O_WRONLY);
    if (fd >= 0) {
        musr_sc_write(fd, "/bin:/sbin:/usr/bin:/usr/sbin", 28);
        musr_sc_close(fd);
    }

    fd = musr_sc_open("/export/cfg/testuser/welcome.txt", O_CREAT | O_WRONLY);
    if (fd >= 0) {
        musr_sc_write(fd, "Welcome, testuser! This is your config directory.\n", 50);
        musr_sc_close(fd);
    }

    c_grn();
    out_puts("boot: user database created (root + testuser)\n");
    c_rst();
}

static void set_env(const char *name, const char *value)
{
    for (int i = 0; i < envar_cnt; i++) {
        int k = 0;
        while (envars[i][k] && envars[i][k] != '=' && name[k] && envars[i][k] == name[k])
            k++;
        if (envars[i][k] == '=' && name[k] == '\0') {
            int vi = k + 1;
            int si = 0;
            while (value[si] && vi < 127)
                envars[i][vi++] = value[si++];
            envars[i][vi] = '\0';
            return;
        }
    }
    if (envar_cnt >= 16) return;
    int pos = 0;
    while (*name && pos < 127)
        envars[envar_cnt][pos++] = *name++;
    envars[envar_cnt][pos++] = '=';
    while (*value && pos < 127)
        envars[envar_cnt][pos++] = *value++;
    envars[envar_cnt][pos] = '\0';
    envar_cnt++;
}

static int read_file(const char *path, char *buf, int max)
{
    int fd = musr_sc_open(path, 0);
    if (fd < 0) return -1;
    int n = musr_sc_read(fd, buf, (uint32_t)(max - 1));
    musr_sc_close(fd);
    if (n > 0) buf[n] = '\0';
    return n;
}

void musr_setup_env(void)
{
    passwd_entry_t pw;
    const char *home_dir = "/";
    const char *user_name = "unknown";
    uint32_t uid = musr_sc_getuid();
    if (musr_getpwuid(uid, &pw) == 0) {
        home_dir = pw.home;
        user_name = pw.username;
    }
    set_env("HOME", home_dir);
    set_env("USER", user_name);

    char path_buf[256];
    char path_file[128];
    musr_strncpy(path_file, "/export/PATH/", sizeof(path_file)-1);
    musr_strncat(path_file, user_name, sizeof(path_file)-musr_strlen(path_file)-1);

    int n = read_file(path_file, path_buf, sizeof(path_buf));
    if (n <= 0) {
        n = read_file("/export/PATH/default", path_buf, sizeof(path_buf));
    }
    if (n > 0) {
        while (n > 0 && (path_buf[n-1] == '\n' || path_buf[n-1] == '\r'))
            path_buf[--n] = '\0';
        set_env("PATH", path_buf);
    } else {
        set_env("PATH", "/bin");
    }
}
