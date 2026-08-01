/*
 * M4KK1 4P1 - unistd.h
 * Description: Minimal unistd.h implementation for M4KK1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4K_UNISTD_H
#define _M4K_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

/* File operations */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int open(const char *pathname, int flags, ...);
off_t lseek(int fd, off_t offset, int whence);

/* Process control */
pid_t fork(void);
int execve(const char *filename, char *const argv[], char *const envp[]);
int execv(const char *filename, char *const argv[]);
int execvp(const char *file, char *const argv[]);
void _exit(int status);

/* Process information */
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);

/* Working directory */
char *getcwd(char *buf, size_t size);
int chdir(const char *path);

/* File system */
int unlink(const char *pathname);
int rmdir(const char *pathname);
int link(const char *oldpath, const char *newpath);
int symlink(const char *oldpath, const char *newpath);
int readlink(const char *path, char *buf, size_t bufsiz);

/* Pipes and duplication */
int pipe(int pipefd[2]);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

/* Sleep */
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

/* Constants */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#endif /* _M4K_UNISTD_H */
