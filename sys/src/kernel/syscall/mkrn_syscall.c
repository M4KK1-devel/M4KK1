#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <kernel.h>
#include <m4k/types.h>
#include <m4k/stat.h>
#include <m4k/state.h>
#include <m4k_syscall.h>
#include <process.h>
#include <syscall.h>
#include <idt.h>
#include <vfs.h>
#include <yafs.h>

static int mkrn_syscall_identity(uint32_t num, uint32_t arg1, uint32_t arg2,
                                  uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;

    switch (num) {
    case M4K_SYS_GETUID:
        return (int)mkrn_process_get_uid();

    case M4K_SYS_GETEUID:
        return (int)mkrn_process_get_euid();

    case M4K_SYS_SETUID: {
        uid_t new_uid = (uid_t)arg1;
        struct mkrn_process *cur = mkrn_process_get_current();
        if (!cur) return -M4K_EPERM;

        if (cur->euid == M4K_UID_ROOT) {
            cur->uid = new_uid;
            cur->euid = new_uid;
            return 0;
        } else if (new_uid == cur->uid || new_uid == cur->euid) {
            cur->euid = new_uid;
            return 0;
        }
        return -M4K_EPERM;
    }

    case M4K_SYS_GETGID:
        return (int)mkrn_process_get_gid();

    case M4K_SYS_GETEGID:
        return (int)mkrn_process_get_egid();

    case M4K_SYS_SETGID: {
        gid_t new_gid = (gid_t)arg1;
        struct mkrn_process *cur = mkrn_process_get_current();
        if (!cur) return -M4K_EPERM;

        if (cur->euid == M4K_UID_ROOT) {
            cur->gid = new_gid;
            cur->egid = new_gid;
            return 0;
        } else if (new_gid == cur->gid || new_gid == cur->egid) {
            cur->egid = new_gid;
            return 0;
        }
        return -M4K_EPERM;
    }

    case M4K_SYS_GETGROUPS: {
        int size = (int)arg1;
        uint32_t *list = (uint32_t *)arg2;
        return mkrn_process_get_groups(list, size);
    }

    case M4K_SYS_SETGROUPS: {
        int size = (int)arg1;
        const uint32_t *list = (const uint32_t *)arg2;
        return mkrn_process_set_groups(size, list);
    }

    default:
        return -M4K_EINVAL;
    }
}

static int mkrn_syscall_fork_st(uint32_t arg1, uint32_t arg2,
                                 uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    uint64_t inherit_mask = ((uint64_t)arg2 << 32) | arg1;
    uint32_t flags = arg2;

    if (!(flags & RFPROC))
        return -M4K_EINVAL;
    if (inherit_mask & (M4K_ZOMBIE | M4K_IDLE))
        return -M4K_EINVAL;

    struct mkrn_process *parent = mkrn_process_get_current();
    if (!parent)
        return -M4K_EPERM;

    return (int)mkrn_fork_status(inherit_mask, flags);
}

int mkrn_syscall_dispatch(uint32_t num, uint32_t arg1, uint32_t arg2,
                           uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
    switch (num) {
    case M4K_SYS_GETUID:
    case M4K_SYS_GETEUID:
    case M4K_SYS_SETUID:
    case M4K_SYS_GETGID:
    case M4K_SYS_GETEGID:
    case M4K_SYS_SETGID:
    case M4K_SYS_GETGROUPS:
    case M4K_SYS_SETGROUPS:
        return mkrn_syscall_identity(num, arg1, arg2, arg3, arg4, arg5);

    case M4K_SYS_FORK_ST:
        return mkrn_syscall_fork_st(arg1, arg2, arg3, arg4, arg5);

    default:
        return -M4K_EINVAL;
    }
}
