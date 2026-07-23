#include "m4sh.h"

#define PRIME_GID 1001
#define MAX_GROUPS 16

void musr_cmd_quell(int ac, char **av)
{
    if (ac < 2) {
        c_red();
        out_puts("Usage: quell <command> [args...]\n");
        c_rst();
        return;
    }

    int allowed = (musr_sc_getuid() == 0);

    if (!allowed) {
        uint32_t groups[MAX_GROUPS];
        int ng = musr_sc_getgroups(MAX_GROUPS, groups);
        for (int i = 0; i < ng; i++) {
            if (groups[i] == PRIME_GID) {
                allowed = 1;
                break;
            }
        }
    }

    if (!allowed) {
        c_red();
        out_puts("quell: permission denied (not in prime group or root)\n");
        c_rst();
        return;
    }

    uint32_t old_euid = musr_sc_geteuid();
    musr_sc_setuid(0);

    /* Find and run the requested command */
    const char *cmd = av[1];
    char **cmd_av = av + 1;
    int cmd_ac = ac - 1;
    int found = 0;
    for (int i = 0; musr_cmd_table[i].name; i++) {
        if (musr_strcmp(cmd, musr_cmd_table[i].name) == 0) {
            musr_cmd_table[i].func(cmd_ac, cmd_av);
            found = 1;
            break;
        }
    }

    if (!found) {
        c_red();
        out_puts("quell: command not found: ");
        out_puts(cmd);
        out_puts("\n");
        c_rst();
    }

    /* Restore EUID regardless of outcome */
    musr_sc_setuid(old_euid);
}
