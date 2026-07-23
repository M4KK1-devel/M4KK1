#pragma once

#include <stdint.h>
#include <stdbool.h>

struct mkrn_vfs_dirent;

#define M4K_SESSION_MAX 16
#define M4K_SESSION_TTY_LEN 32
#define M4K_SESSION_USERNAME_LEN 64

struct m4k_session {
    char tty[32];
    uint32_t uid;
    uint32_t pid;
    uint32_t login_time;
    char username[64];
    int active;
};

void mkrn_sessions_init(void);
int mkrn_session_create(const char *tty, uint32_t uid, const char *username);
int mkrn_session_destroy(const char *tty);
int mkrn_sessions_open(const char *path, int flags, int *out_fd);
int mkrn_sessions_close(int fd);
int mkrn_sessions_read(int fd, void *buf, uint32_t count);
int mkrn_sessions_write(int fd, const void *buf, uint32_t count);
int mkrn_sessions_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max);
int mkrn_sessions_is_sessions_fd(int fd);

int mkrn_sessions_get_all(struct m4k_session *buf, int max);
