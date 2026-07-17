#include <m4k/types.h>
#include <m4k/stat.h>
#include <m4k/state.h>
#include <stdint.h>
#include <stdbool.h>
#include <yafs.h>
#include <vfs.h>
#include <process.h>
#include <kernel.h>

/* mkrn_process_get_current is declared in process.h */

int mkrn_check_access(struct yafs_inode_value *pInode, uint32_t u32Mode)
{
    struct mkrn_process *cur = mkrn_process_get_current();
    if (!cur || !pInode)
        return -M4K_EACCES;

    uint16_t u16Perm = (uint16_t)pInode->mode;

    if (cur->euid == M4K_UID_ROOT) {
        if (u32Mode & M4K_ACCESS_EXEC) {
            return (u16Perm & (M4K_IXUSR | M4K_IXGRP | M4K_IXOTH)) ? 0 : -M4K_EACCES;
        }
        return 0;
    }

    if (cur->euid == pInode->uid) {
        if ((u32Mode & M4K_ACCESS_READ) && !(u16Perm & M4K_IRUSR)) return -M4K_EACCES;
        if ((u32Mode & M4K_ACCESS_WRITE) && !(u16Perm & M4K_IWUSR)) return -M4K_EACCES;
        if ((u32Mode & M4K_ACCESS_EXEC) && !(u16Perm & M4K_IXUSR)) return -M4K_EACCES;
        return 0;
    }

    for (uint32_t i = 0; i < cur->group_count; i++) {
        if (cur->group_list[i] == pInode->gid) {
            if ((u32Mode & M4K_ACCESS_READ) && !(u16Perm & M4K_IRGRP)) return -M4K_EACCES;
            if ((u32Mode & M4K_ACCESS_WRITE) && !(u16Perm & M4K_IWGRP)) return -M4K_EACCES;
            if ((u32Mode & M4K_ACCESS_EXEC) && !(u16Perm & M4K_IXGRP)) return -M4K_EACCES;
            return 0;
        }
    }

    if ((u32Mode & M4K_ACCESS_READ) && !(u16Perm & M4K_IROTH)) return -M4K_EACCES;
    if ((u32Mode & M4K_ACCESS_WRITE) && !(u16Perm & M4K_IWOTH)) return -M4K_EACCES;
    if ((u32Mode & M4K_ACCESS_EXEC) && !(u16Perm & M4K_IXOTH)) return -M4K_EACCES;

    return 0;
}
