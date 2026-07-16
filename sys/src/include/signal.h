#pragma once

#include <stdint.h>

#define M4K_SIGABRT     1
#define M4K_SIGKILL     2
#define M4K_SIGTERM     3
#define M4K_SIGSTOP     4
#define M4K_SIGCONT     5
#define M4K_SIGTRAP     6
#define M4K_SIGCHLD     7
#define M4K_SIGPIPE     8
#define M4K_SIGUSR1     9
#define M4K_SIGUSR2     10

#define M4K_NSIG        11

#define M4K_SIG_ERR     (-1)
#define M4K_SIG_DFL     0
#define M4K_SIG_IGN     1

#define M4K_WNOHANG     1
#define M4K_WUNTRACED   2
#define M4K_WCONTINUED  4

#define M4K_WEXITSTATUS(s)  (((s) >> 8) & 0xFF)
#define M4K_WTERMSIG(s)     ((s) & 0x7F)
#define M4K_WCOREDUMP(s)    ((s) & 0x80)
#define M4K_WIFEXITED(s)    (M4K_WTERMSIG(s) == 0)
#define M4K_WIFSIGNALED(s)  (M4K_WTERMSIG(s) > 0)
#define M4K_WIFSTOPPED(s)   ((s) & 0x10000)
#define M4K_WSTOPSIG(s)     (((s) >> 16) & 0xFF)

struct mkrn_process;
void mkrn_signal_deliver(struct mkrn_process *proc, int sig);
void mkrn_signal_init(void);