#pragma once
#include <stdint.h>
#include <m4k/types.h>

struct mkrn_yafs_inode {
    uint32_t    u32Ino;
    uint64_t    u64Size;
    uint16_t    u16Mode;
    uid_t       u32Uid;
    gid_t       u32Gid;
    uint32_t    u32NLink;
    uint32_t    u32FileType;
    char        szName[128];
};

static inline void mkrn_yafs_inode_from_value(
    struct mkrn_yafs_inode *pOut,
    const struct yafs_inode_value *pVal)
{
    pOut->u32Ino = 0;
    pOut->u64Size = pVal->size;
    pOut->u16Mode = (uint16_t)pVal->mode;
    pOut->u32Uid = pVal->uid;
    pOut->u32Gid = pVal->gid;
    pOut->u32NLink = pVal->link_count;
    pOut->u32FileType = pVal->file_type;
}

static inline void mkrn_yafs_inode_to_value(
    struct yafs_inode_value *pVal,
    const struct mkrn_yafs_inode *pIn)
{
    pVal->size = pIn->u64Size;
    pVal->mode = pIn->u16Mode;
    pVal->uid = pIn->u32Uid;
    pVal->gid = pIn->u32Gid;
    pVal->link_count = pIn->u32NLink;
    pVal->file_type = pIn->u32FileType;
}
