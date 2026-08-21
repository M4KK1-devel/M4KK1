#include <sessions.h>
/*
 * M4KK1 4P1 - mkrn_sessions.c
 * Description: Session tracking for /sys/sessions (§6.11.2)
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <vfs.h>
#include <process.h>
#include <kernel.h>

#define MAX_SESSIONS 16
#define SESSIONS_MAX_NAME 64

typedef struct {
    char tty[32];
    uint32_t uid;
    uint32_t pid;
    uint32_t login_time;
    char username[64];
    bool in_use;
} session_entry_t;

static session_entry_t sessions[MAX_SESSIONS];
static int sessions_next_fd = 2000;

#define SESSIONS_FD_BASE  2000
#define SESSIONS_FD_LIMIT 3000   /* device_tree range starts here */

/* ── Session file tracking (for open files) ── */

typedef struct {
    char path[SESSIONS_MAX_NAME];
    int session_idx;
    int fd;                     /* fd handed out at open; close/find
                                 * must match on it, not blindly grab
                                 * the first in_use slot */
    bool in_use;
} session_file_t;

static session_file_t session_files[32];

void mkrn_sessions_init(void)
{
    memset(sessions, 0, sizeof(sessions));
    memset(session_files, 0, sizeof(session_files));
    M4K_LOG_INFO("Sessions subsystem initialized");
}

int mkrn_session_create(const char *tty, uint32_t uid, const char *username)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].in_use) {
            sessions[i].in_use = true;
            strncpy(sessions[i].tty, tty, 31);
            sessions[i].tty[31] = '\0';
            sessions[i].uid = uid;
            sessions[i].pid = mkrn_process_get_pid();
            sessions[i].login_time = 1721124309; /* FIXME: real time */
            strncpy(sessions[i].username, username, 63);
            sessions[i].username[63] = '\0';
            M4K_LOG_INFO("Session created on ");
            M4K_LOG_INFO(tty);
            return i;
        }
    }
    return -1;
}

int mkrn_session_destroy(const char *tty)
{
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].tty, tty) == 0) {
            sessions[i].in_use = false;
            M4K_LOG_INFO("Session destroyed on ");
            M4K_LOG_INFO(tty);
            return 0;
        }
    }
    return -1;
}

static int get_session_idx_from_path(const char *path)
{
    const char *p = path;
    while (*p == '/') p++;
    if (strncmp(p, "sys/sessions/", 13) != 0)
        return -1;
    p += 13;

    /* Check for listing request (just /sys/sessions/) */
    if (*p == '\0')
        return -2;

    /* Extract TTY name */
    char tty[32];
    int ti = 0;
    while (*p && *p != '/' && ti < 31)
        tty[ti++] = *p++;
    tty[ti] = '\0';

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].tty, tty) == 0) {
            /* Check for sub-file */
            if (*p == '/') {
                p++;
                const char *sub = p;
                if (strcmp(sub, "uid") == 0) return 1000 + i;
                if (strcmp(sub, "pid") == 0) return 2000 + i;
                if (strcmp(sub, "login_time") == 0) return 3000 + i;
                if (strcmp(sub, "username") == 0) return 4000 + i;
            }
            return -1;
        }
    }
    return -1;
}

int mkrn_sessions_open(const char *path, int flags, int *out_fd)
{
    (void)flags;
    int idx = get_session_idx_from_path(path);
    if (idx < 0) {
        /* Allow listing /sys/sessions/ itself */
        if (idx == -2) {
            for (int i = 0; i < 32; i++) {
                if (!session_files[i].in_use) {
                    session_files[i].in_use = true;
                    strncpy(session_files[i].path, path, SESSIONS_MAX_NAME - 1);
                    session_files[i].path[SESSIONS_MAX_NAME - 1] = '\0';
                    session_files[i].session_idx = -2;
                    session_files[i].fd = sessions_next_fd++;
                    if (sessions_next_fd >= SESSIONS_FD_LIMIT)
                        sessions_next_fd = SESSIONS_FD_BASE;
                    *out_fd = session_files[i].fd;
                    return 0;
                }
            }
        }
        return -1;
    }

    for (int i = 0; i < 32; i++) {
        if (!session_files[i].in_use) {
            session_files[i].in_use = true;
            strncpy(session_files[i].path, path, SESSIONS_MAX_NAME - 1);
            session_files[i].path[SESSIONS_MAX_NAME - 1] = '\0';
            session_files[i].session_idx = idx;
            session_files[i].fd = sessions_next_fd++;
            if (sessions_next_fd >= SESSIONS_FD_LIMIT)
                sessions_next_fd = SESSIONS_FD_BASE;
            *out_fd = session_files[i].fd;
            return 0;
        }
    }
    return -1;
}

int mkrn_sessions_close(int fd)
{
    for (int i = 0; i < 32; i++) {
        if (session_files[i].in_use && session_files[i].fd == fd) {
            session_files[i].in_use = false;
            return 0;
        }
    }
    return -1;
}

static session_file_t *sessions_find_by_fd(int fd)
{
    for (int i = 0; i < 32; i++) {
        if (session_files[i].in_use && session_files[i].fd == fd)
            return &session_files[i];
    }
    return NULL;
}

int mkrn_sessions_read(int fd, void *buf, uint32_t count)
{
    session_file_t *sf = sessions_find_by_fd(fd);
    if (!sf) return -1;

    char tmp[128];
    int len = 0;

    /* Listing /sys/sessions/ directory */
    if (sf->session_idx == -2) {
        /* Bound by BOTH the tmp buffer and the caller's count — the
         * loop guard used to be count alone (user-controlled via the
         * read syscall), letting 16 sessions x 32-char ttys overrun
         * tmp[128]. */
        int cap = (int)count;
        if (cap > (int)sizeof(tmp)) cap = (int)sizeof(tmp);
        for (int i = 0; i < MAX_SESSIONS && len < cap - 2; i++) {
            if (sessions[i].in_use) {
                for (int j = 0; sessions[i].tty[j] && len < cap - 2; j++)
                    tmp[len++] = sessions[i].tty[j];
                tmp[len++] = '\n';
            }
        }
        if (len == 0) {
            tmp[len++] = '\n';
        }
        if (len > (int)count) len = (int)count;
        memcpy(buf, tmp, (uint32_t)len);
        return len;
    }

    int session_idx;
    int sub_type;
    if (sf->session_idx >= 1000 && sf->session_idx < 2000) {
        session_idx = sf->session_idx - 1000;
        sub_type = 0;
    } else if (sf->session_idx >= 2000 && sf->session_idx < 3000) {
        session_idx = sf->session_idx - 2000;
        sub_type = 1;
    } else if (sf->session_idx >= 3000 && sf->session_idx < 4000) {
        session_idx = sf->session_idx - 3000;
        sub_type = 2;
    } else if (sf->session_idx >= 4000 && sf->session_idx < 5000) {
        session_idx = sf->session_idx - 4000;
        sub_type = 3;
    } else {
        return -1;
    }

    if (session_idx < 0 || session_idx >= MAX_SESSIONS || !sessions[session_idx].in_use)
        return -1;

    session_entry_t *se = &sessions[session_idx];

    switch (sub_type) {
        case 0: { /* uid */
            char nbuf[16];
            int ni = 0;
            uint32_t n = se->uid;
            if (n == 0) { nbuf[ni++] = '0'; }
            else { char rev[16]; int ri = 0; while (n) { rev[ri++] = '0' + (n % 10); n /= 10; } while (ri > 0) { nbuf[ni++] = rev[--ri]; } }
            nbuf[ni++] = '\n';
            memcpy(tmp, nbuf, ni);
            len = ni;
            break;
        }
        case 1: { /* pid */
            char nbuf[16];
            int ni = 0;
            uint32_t n = se->pid;
            if (n == 0) { nbuf[ni++] = '0'; }
            else { char rev[16]; int ri = 0; while (n) { rev[ri++] = '0' + (n % 10); n /= 10; } while (ri > 0) { nbuf[ni++] = rev[--ri]; } }
            nbuf[ni++] = '\n';
            memcpy(tmp, nbuf, ni);
            len = ni;
            break;
        }
        case 2: { /* login_time */
            char nbuf[16];
            int ni = 0;
            uint32_t n = se->login_time;
            if (n == 0) { nbuf[ni++] = '0'; }
            else { char rev[16]; int ri = 0; while (n) { rev[ri++] = '0' + (n % 10); n /= 10; } while (ri > 0) { nbuf[ni++] = rev[--ri]; } }
            nbuf[ni++] = '\n';
            memcpy(tmp, nbuf, ni);
            len = ni;
            break;
        }
        case 3: { /* username */
            int i;
            for (i = 0; se->username[i] && i < 62; i++)
                tmp[i] = se->username[i];
            tmp[i++] = '\n';
            len = i;
            break;
        }
    }

    if (len > (int)count) len = (int)count;
    memcpy(buf, tmp, (uint32_t)len);
    return len;
}

int mkrn_sessions_write(int fd, const void *buf, uint32_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

int mkrn_sessions_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    (void)fd;
    (void)buf;
    (void)max;
    return 0;
}

int mkrn_sessions_is_sessions_fd(int fd)
{
    session_file_t *sf = sessions_find_by_fd(fd);
    return sf ? 1 : 0;
}

int mkrn_sessions_get_all(struct m4k_session *buf, int max)
{
    int written = 0;
    for (int i = 0; i < MAX_SESSIONS && written < max; i++) {
        if (sessions[i].in_use) {
            memcpy(buf[written].tty, sessions[i].tty, 32);
            buf[written].uid = sessions[i].uid;
            buf[written].pid = sessions[i].pid;
            buf[written].login_time = sessions[i].login_time;
            memcpy(buf[written].username, sessions[i].username, 64);
            buf[written].active = true;
            written++;
        }
    }
    return written;
}
