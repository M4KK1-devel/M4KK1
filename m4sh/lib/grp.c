/*
 * M4KK1 4P1 - grp.c
 * Description: Groups database parser (§6.6)
 *
 * Format: groupname:gid:user1,user2,user3
 * File: /export/cfg/groups.db
 */

#include "../m4sh.h"

#define GRP_DB_PATH "/export/cfg/groups.db"
#define MAX_GROUPS 32
#define MAX_LINE 512

static int parse_group_line(const char *line, group_entry_t *entry)
{
    const char *p = line;
    char *out = entry->groupname;
    int field = 0, oi = 0;

    memset(entry, 0, sizeof(group_entry_t));

    while (*p && *p != '#') {
        if (*p == ':') {
            field++;
            oi = 0;
            p++;
            continue;
        }
        switch (field) {
            case 0:
                if (oi < 63) entry->groupname[oi++] = *p;
                break;
            case 1:
                if (*p >= '0' && *p <= '9')
                    entry->gid = entry->gid * 10 + (*p - '0');
                break;
            case 2:
                if (oi < 255) entry->members[oi++] = *p;
                break;
        }
        p++;
    }
    return 1;
}

int musr_read_groups_db(group_entry_t *entries, int max)
{
    memset(entries, 0, sizeof(group_entry_t) * (size_t)max);

    int fd = musr_sc_open(GRP_DB_PATH, O_RDONLY);
    if (fd < 0) {
        /* Return default prime group */
        musr_strncpy(entries[0].groupname, "prime", sizeof(entries[0].groupname)-1);
        entries[0].gid = 1001;
        musr_strncpy(entries[0].members, "root", sizeof(entries[0].members)-1);
        return 1;
    }

    char buf[2048];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    int count = 0;
    char *line = buf;
    while (*line && count < max) {
        char *end = line;
        while (*end && *end != '\n') end++;
        char save = *end;
        *end = '\0';

        if (*line && *line != '#')
            parse_group_line(line, &entries[count++]);

        *end = save;
        line = end + 1;
    }
    return count;
}

int musr_getgrnam(const char *name, group_entry_t *out)
{
    group_entry_t entries[MAX_GROUPS];
    int count = musr_read_groups_db(entries, MAX_GROUPS);
    for (int i = 0; i < count; i++) {
        if (musr_strcmp(entries[i].groupname, name) == 0) {
            *out = entries[i];
            return 0;
        }
    }
    return -1;
}

int musr_getgrgid(uint32_t gid, group_entry_t *out)
{
    group_entry_t entries[MAX_GROUPS];
    int count = musr_read_groups_db(entries, MAX_GROUPS);
    for (int i = 0; i < count; i++) {
        if (entries[i].gid == gid) {
            *out = entries[i];
            return 0;
        }
    }
    return -1;
}

int musr_in_group(const char *username, const char *groupname)
{
    group_entry_t entry;
    if (musr_getgrnam(groupname, &entry) != 0)
        return 0;

    const char *p = entry.members;
    while (*p) {
        const char *start = p;
        while (*p && *p != ',') p++;
        int len = (int)(p - start);
        if (musr_strlen(username) == len &&
            musr_strncmp(username, start, len) == 0)
            return 1;
        if (*p == ',') p++;
    }
    return 0;
}

int musr_update_groups_db(const group_entry_t *entries, int count)
{
    /* Write to temp file, then rename atomically */
    const char *tmp = "/export/cfg/.groups.db.tmp";
    int fd = musr_sc_open(tmp, O_CREAT | O_WRONLY);
    if (fd < 0) return -1;

    for (int i = 0; i < count; i++) {
        char line[MAX_LINE];
        int pos = 0;
        const char *p;

        p = entries[i].groupname;
        while (*p && pos < MAX_LINE - 1) line[pos++] = *p++;
        line[pos++] = ':';

        char gbuf[16];
        int gi = 0;
        uint32_t g = entries[i].gid;
        if (g == 0) { gbuf[gi++] = '0'; }
        else { char rev[16]; int ri = 0; while (g) { rev[ri++] = '0' + (g % 10); g /= 10; } while (ri > 0) gbuf[gi++] = rev[--ri]; }
        for (int j = 0; j < gi && pos < MAX_LINE - 1; j++)
            line[pos++] = gbuf[j];
        line[pos++] = ':';

        p = entries[i].members;
        while (*p && pos < MAX_LINE - 2) line[pos++] = *p++;
        line[pos++] = '\n';
        line[pos] = '\0';

        musr_sc_write(fd, line, pos);
    }

    musr_sc_close(fd);
    musr_sc_rename(tmp, GRP_DB_PATH);
    return 0;
}
