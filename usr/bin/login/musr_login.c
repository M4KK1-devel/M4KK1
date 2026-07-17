#include <stdint.h>
#include <stdbool.h>
#include <string.h>

extern int musr_printf(const char *fmt, ...);
extern int musr_gets(char *buf, int size);
extern void musr_getpass(char *buf, int size);
extern int m4k_setuid(uint32_t uid);
extern int m4k_chdir(const char *path);
extern int m4k_mount(const char *source, const char *target, uint32_t flags);
extern int m4k_spawn(const char *path, char *const argv[], char *const envp[], uint32_t flags);

#define M4K_MNT_BIND 0

static void strip_crlf(char *s)
{
    int i, j;
    for (i = 0, j = 0; s[i]; i++) {
        if (s[i] != '\r' && s[i] != '\n')
            s[j++] = s[i];
    }
    s[j] = '\0';
}

int main(void)
{
    char szUser[64], szPass[64];

    while (1) {
        musr_printf("M4KK1 login: ");
        musr_gets(szUser, sizeof(szUser));
        strip_crlf(szUser);

        musr_printf("Password: ");
        musr_getpass(szPass, sizeof(szPass));
        strip_crlf(szPass);

        if (strcmp(szUser, "root") == 0 && strcmp(szPass, "m4k123") == 0) {
            musr_printf("Login successful.\n");
            m4k_mount("/export/root", "/root", M4K_MNT_BIND);
            m4k_chdir("/root");
            m4k_setuid(0);
            char *argv[] = {"/bin/m4sh", NULL};
            m4k_spawn("/bin/m4sh", argv, NULL, 0);
            return 0;
        } else {
            musr_printf("Login incorrect.\n");
        }
    }
}
