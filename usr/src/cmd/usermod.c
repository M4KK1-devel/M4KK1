/*
 * M4KK1 4P1 - usermod.c
 * Description: Modify user account (§6.8.1)
 *
 * Syntax: usermod [options] <username>
 *   -a/--add    Add user to supplementary group
 *   -d/--del    Remove user from group
 *   -s/--shell  Change login shell
 *   -g/--group  Set primary group
 *   -L/--lock   Lock account
 *   -U/--unlock Unlock account
 *   -h/--help   Show help
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

void musr_cmd_usermod(int ac, char **av)
{
    if (ac < 2) {
        ser_puts("usage: usermod [options] <username>\n");
        return;
    }

    if (!is_prime()) {
        c_red();
        out_puts("usermod: permission denied (requires prime group or root)\n");
        c_rst();
        return;
    }

    const char *add_grp = NULL;
    const char *del_grp = NULL;
    const char *new_shell = NULL;
    const char *new_group = NULL;
    int lock = 0, unlock = 0;
    const char *username = NULL;

    for (int i = 1; i < ac; i++) {
        if (musr_strcmp(av[i], "-h") == 0 || musr_strcmp(av[i], "--help") == 0) {
            ser_puts("usermod: modify user account\n");
            ser_puts("  -a/--add <group>    Add user to group\n");
            ser_puts("  -d/--del <group>    Remove user from group\n");
            ser_puts("  -s/--shell <shell>  Change login shell\n");
            ser_puts("  -g/--group <gid>    Set primary group\n");
            ser_puts("  -L/--lock           Lock account\n");
            ser_puts("  -U/--unlock         Unlock account\n");
            return;
        } else if (musr_strcmp(av[i], "-a") == 0 || musr_strcmp(av[i], "--add") == 0) {
            if (i + 1 < ac) add_grp = av[++i];
        } else if (musr_strcmp(av[i], "-d") == 0 || musr_strcmp(av[i], "--del") == 0) {
            if (i + 1 < ac) del_grp = av[++i];
        } else if (musr_strcmp(av[i], "-s") == 0 || musr_strcmp(av[i], "--shell") == 0) {
            if (i + 1 < ac) new_shell = av[++i];
        } else if (musr_strcmp(av[i], "-g") == 0 || musr_strcmp(av[i], "--group") == 0) {
            if (i + 1 < ac) new_group = av[++i];
        } else if (musr_strcmp(av[i], "-L") == 0 || musr_strcmp(av[i], "--lock") == 0) {
            lock = 1;
        } else if (musr_strcmp(av[i], "-U") == 0 || musr_strcmp(av[i], "--unlock") == 0) {
            unlock = 1;
        } else {
            username = av[i];
        }
    }

    if (!username) {
        c_red();
        out_puts("usermod: missing username\n");
        c_rst();
        return;
    }

    passwd_entry_t entries[32];
    int count = musr_read_passwd_db(entries, 32);
    int found = -1;
    for (int i = 0; i < count; i++) {
        if (musr_strcmp(entries[i].username, username) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        c_red();
        out_puts("usermod: user not found\n");
        c_rst();
        return;
    }

    passwd_entry_t *e = &entries[found];

    if (new_shell) {
        musr_strncpy(e->shell, new_shell, sizeof(e->shell)-1);
    }

    if (new_group) {
        uint32_t gid = 0;
        const char *gp = new_group;
        while (*gp >= '0' && *gp <= '9')
            gid = gid * 10 + (*gp++ - '0');
        if (gid > 0) e->gid = gid;
    }

    if (lock) {
        /* Lock: prefix hash with ! */
        if (e->password_hash[0] != '!') {
            char tmp[256];
            musr_strncpy(tmp, "!", sizeof(tmp)-1);
            musr_strncat(tmp, e->password_hash, sizeof(tmp)-musr_strlen(tmp)-1);
            musr_strncpy(e->password_hash, tmp, sizeof(e->password_hash)-1);
        }
    }

    if (unlock) {
        /* Unlock: remove leading ! */
        if (e->password_hash[0] == '!') {
            char *src = e->password_hash + 1;
            char *dst = e->password_hash;
            while (*src) *dst++ = *src++;
            *dst = '\0';
        }
    }

    if (musr_update_passwd_db(entries, count) == 0) {
        c_grn();
        out_puts("usermod: updated\n");
        c_rst();
    } else {
        c_red();
        out_puts("usermod: failed to update passwd.db\n");
        c_rst();
        return;
    }

    /* Handle group add/del */
    if (add_grp || del_grp) {
        group_entry_t ventries[32];
        int gcount = musr_read_groups_db(ventries, 32);

        if (add_grp) {
            int gfound = -1;
            for (int i = 0; i < gcount; i++) {
                if (musr_strcmp(ventries[i].groupname, add_grp) == 0) {
                    gfound = i;
                    break;
                }
            }
            if (gfound >= 0) {
                char *members = ventries[gfound].members;
                int already = 0;
                const char *p = members;
                while (*p) {
                    const char *start = p;
                    while (*p && *p != ',') p++;
                    int len = (int)(p - start);
                    if (musr_strlen(username) == len && musr_strncmp(username, start, len) == 0) {
                        already = 1;
                        break;
                    }
                    if (*p == ',') p++;
                }
                if (!already) {
                    int mlen = musr_strlen(members);
                    if (mlen > 0) { members[mlen++] = ','; }
                    musr_strncpy(members + mlen, username, 256 - mlen);
                }
            }
        }

        if (del_grp) {
            for (int i = 0; i < gcount; i++) {
                if (musr_strcmp(ventries[i].groupname, del_grp) == 0) {
                    char *members = ventries[i].members;
                    char newm[256];
                    int ni = 0;
                    const char *p = members;
                    while (*p) {
                        const char *start = p;
                        while (*p && *p != ',') p++;
                        int len = (int)(p - start);
                        int match = (musr_strlen(username) == len && musr_strncmp(username, start, len) == 0);
                        if (!match) {
                            if (ni > 0) newm[ni++] = ',';
                            for (int j = 0; j < len; j++)
                                newm[ni++] = start[j];
                        }
                        if (*p == ',') p++;
                    }
                    newm[ni] = '\0';
                    musr_strncpy(members, newm, sizeof(ventries->members)-1);
                    break;
                }
            }
        }

        musr_update_groups_db(ventries, gcount);
    }
}
