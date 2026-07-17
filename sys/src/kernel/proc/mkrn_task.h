#pragma once

#include <stdint.h>
#include <stddef.h>
#include <m4k/types.h>
#include <m4k/state.h>
#include <namespace.h>
#include <signal.h>

#define M4K_STACK_SIZE 8192

#define RFPROC      0x0001
#define RFFDG       0x0002
#define RFNAMEG     0x0004
#define RFENVG      0x0008
#define RFMEM       0x0010
#define RFNOWAIT    0x0020

#define M4K_SPAWN_NEW_NS      0x0001
#define M4K_SPAWN_NEW_FD      0x0002
#define M4K_SPAWN_NEW_ENV     0x0004
#define M4K_SPAWN_NOWAIT      0x0008
#define M4K_SPAWN_TRACE       0x0010

#define M4K_PRIO_HIGH   0
#define M4K_PRIO_NORMAL 1
#define M4K_PRIO_LOW    2

struct mkrn_task {
    uint32_t pid;
    uint32_t ppid;

    uid_t   mkrn_u32Uid;
    uid_t   mkrn_u32Euid;
    gid_t   mkrn_u32Gid;
    gid_t   mkrn_u32Egid;
    gid_t   mkrn_pGroupList[16];
    uint32_t mkrn_u32GroupCount;

    uint64_t mkrn_u64State;

    uint32_t priority;
    uint32_t thread_esp;
    uint32_t sleep_ticks;
    uint32_t exit_status;
    uint32_t pending_signals;
    uint32_t zombie_children;

    char name[32];
    char cwd[256];
    char **argv;
    char **envp;

    m4k_namespace_t ns;

    struct mkrn_task *next;
    struct mkrn_task *children;
    struct mkrn_task *sibling;
    struct mkrn_task *wait_next;
};
