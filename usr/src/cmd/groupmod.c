/*
 * M4KK1 4P1 - groupmod.c
 * Description: Modify group (§6.8.2)
 *
 * Syntax: groupmod [options] <groupname>
 *   -a/--add <user>  Add user to group
 *   -d/--del <user>  Remove user from group
 *   -n/--new <name>  Rename group
 *   -h/--help        Show help
 */

#include "m4sh.h"

static int is_prime(void)
{
    if (musr_sc_getuid() == 0) return 1;
    uint32_t groups[16];
    int n = musr_sc_getgroups(16, groups);
    for (int i = 0; i < n; i++)
        if (groups[i] == 1001) return 1;
    return 0;
}

void musr_cmd_groupmod(int ac, char **av)
{
    if (ac < 2) {
        ser_puts("usage: groupmod [options] <groupname>\n");
        return;
    }

    if (!is_prime()) {
        c_red();
        out_puts("groupmod: permission denied (requires prime group or root)\n");
        c_rst();
        return;
    }

    const char *add_user = NULL;
    const char *del_user = NULL;
    const char *new_name = NULL;
    const char *groupname = NULL;

    for (int i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "-h") == 0 || musr_strcmp(av[i], "--help") == 0) {
            ser_puts("groupmod: modify group\n");
            ser_puts("  -a/--add <user>   Add user to group\n");
            ser_puts("  -d/--del <user>   Remove user from group\n");
            ser_puts("  -n/--new <name>   Rename group\n");
            return;
        } else if (musr_strcmp(av[i], "-a") == 0 || musr_strcmp(av[i], "--add") == 0) {
            if (i + 1 < ac) add_user = av[++i];
        } else if (musr_strcmp(av[i], "-d") == 0 || musr_strcmp(av[i], "--del") == 0) {
            if (i + 1 < ac) del_user = av[++i];
        } else if (musr_strcmp(av[i], "-n") == 0 || musr_strcmp(av[i], "--new") == 0) {
            if (i + 1 < ac) new_name = av[++i];
        } else {
            groupname = av[i];
        }
    }

    if (!groupname) {
        c_red();
        out_puts("groupmod: missing group name\n");
        c_rst();
        return;
    }

    group_entry_t entries[32];
    int count = musr_read_groups_db(entries, 32);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (musr_strcmp(entries[i].groupname, groupname) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        c_red();
        out_puts("groupmod: group not found\n");
        c_rst();
        return;
    }

    group_entry_t *e = &entries[found];

    if (new_name) {
        musr_strcpy(e->groupname, new_name);
    }

    if (add_user) {
        char *members = e->members;
        int already = 0;
        const char *p = members;
        while (*p) {
            const char *start = p;
            while (*p && *p != ',') p++;
            int len = (int)(p - start);
            if (musr_strlen(add_user) == len && musr_strncmp(add_user, start, len) == 0) {
                already = 1;
                break;
            }
            if (*p == ',') p++;
        }
        if (!already) {
            int mlen = musr_strlen(members);
            if (mlen > 0) { members[mlen++] = ','; }
            musr_strcpy(members + mlen, add_user);
        }
    }

    if (del_user) {
        char newm[256];
        int ni = 0;
        const char *p = e->members;
        while (*p) {
            const char *start = p;
            while (*p && *p != ',') p++;
            int len = (int)(p - start);
            int match = (musr_strlen(del_user) == len && musr_strncmp(del_user, start, len) == 0);
            if (!match) {
                if (ni > 0) newm[ni++] = ',';
                for (int j = 0; j < len; j++)
                    newm[ni++] = start[j];
            }
            if (*p == ',') p++;
        }
        newm[ni] = '\0';
        musr_strcpy(e->members, newm);
    }

    if (musr_update_groups_db(entries, count) == 0) {
        c_grn();
        out_puts("groupmod: updated\n");
        c_rst();
    } else {
        c_red();
        out_puts("groupmod: failed to update groups.db\n");
        c_rst();
    }
}
