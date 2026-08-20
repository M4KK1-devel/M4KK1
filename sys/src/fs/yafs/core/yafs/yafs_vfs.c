/*
 * M4KK1 4P1 - yafs_vfs.c
 * Description: YAFS VFS integration — directory ops, path
 *              resolution, FHS creation, file data I/O.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <vfs.h>
#include <yafs.h>
#include <yafs_btree.h>

extern uint64_t root_yafs_tree;
extern uint64_t total_allocated;
int mkrn_yafs_dev_read(uint64_t u64Lba, void *pBuf);
int mkrn_yafs_dev_write(uint64_t u64Lba,
                        const void *pBuf);
uint64_t mkrn_yafs_dev_alloc_block(void);
void mkrn_yafs_dev_free_block(uint64_t u64Lba);

/* Shared 4 KB scratch for raw inode block access.  The YAFS path is
 * non-reentrant (process context only); keeping this in BSS instead
 * of a 4 KB stack frame keeps kernel-stack depth flat on every
 * inode read/write. */
static uint8_t g_inode_scratch[4096];

static const char *
strrchr_y(const char *pS, int c)
{
    const char *p = NULL;
    while (*pS) {
        if (*pS == (char)c)
            p = pS;
        pS++;
    }
    return p;
}

static const char *
strchr_y(const char *pS, int c)
{
    while (*pS) {
        if (*pS == (char)c)
            return pS;
        pS++;
    }
    return NULL;
}

struct readdir_ctx {
    uint64_t           root_tree;
    uint64_t           dir_inode;
    struct mkrn_vfs_dirent *buf;
    uint32_t           max;
    uint32_t           written;
};

static int
readdir_cb(uint64_t u64Key, yafs_entry_t value,
           void *pCtx)
{
    struct readdir_ctx *pR =
        (struct readdir_ctx *)pCtx;
    if (pR->written >= pR->max)
        return 1;

    uint64_t u64Tag = mkrn_yafs_key_tag(u64Key);
    if (u64Tag != YAFS_KS_DIR_ENTRY)
        return 0;

    uint64_t u64Kv = mkrn_yafs_key_value(u64Key);
    uint64_t u64Parent = u64Kv >> 16;

    if (u64Parent != pR->dir_inode)
        return 0;

    uint64_t u64ChildInode = value;

    struct mkrn_vfs_dirent *pD = &pR->buf[pR->written];
    pD->inode = u64ChildInode;
    pD->type = 1;
    pD->size = 0;

    uint64_t u64InodeKey = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64ChildInode);
    yafs_entry_t inode_lba_val;

    if (mkrn_yafs_btree_lookup(
            pR->root_tree, u64InodeKey,
            &inode_lba_val)
        == 0)
    {
        uint64_t u64Lba = inode_lba_val;
        /* Shared BSS scratch, not a 4 KB stack frame — this callback
         * runs for every entry of every readdir walk. */
        uint8_t *iv_buf = g_inode_scratch;
        if (mkrn_yafs_dev_read(u64Lba, iv_buf) == 0) {
            struct yafs_inode_value *pIv =
                (struct yafs_inode_value *)iv_buf;
            pD->size = pIv->size;
            pD->type =
                (pIv->file_type == YAFS_FT_DIR) ? 2
                                                 : 1;
            if (pIv->file_type == YAFS_FT_SYMLINK) {
                pD->type = 3;
                const char *pTgt = pIv->name;
                pD->name[0] = '-';
                pD->name[1] = '>';
                int pos = 2;
                while (*pTgt
                       && pos
                              < (int)sizeof(pD->name)
                                    - 1)
                    pD->name[pos++] = *pTgt++;
                pD->name[pos] = '\0';
                pR->written++;
                return 0;
            }
            mkrn_strncpy(pD->name, pIv->name,
                    sizeof(pD->name) - 1);
            pD->name[sizeof(pD->name) - 1] = '\0';
            pR->written++;
            return 0;
        }
    }

    char buf[32];
    int len = 0;
    uint64_t u64N = u64ChildInode;
    do {
        buf[len++] = '0' + (u64N % 10);
        u64N /= 10;
    } while (u64N > 0);
    for (int i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    const char *pPfx = "ino_";
    int pos = 0;
    while (*pPfx
           && pos < (int)sizeof(pD->name) - 1)
        pD->name[pos++] = *pPfx++;
    for (int i = 0;
         i < len && pos < (int)sizeof(pD->name) - 1;
         i++)
        pD->name[pos++] = buf[i];
    pD->name[pos] = '\0';
    pR->written++;
    return 0;
}

int
mkrn_yafs_readdir(uint64_t u64RootTree,
                  uint64_t u64DirInode,
                  struct mkrn_vfs_dirent *pBuf,
                  uint32_t u32MaxEntries)
{
    if (u64RootTree == 0 || !pBuf)
        return -1;

    struct readdir_ctx ctx;
    ctx.root_tree = u64RootTree;
    ctx.dir_inode = u64DirInode;
    ctx.buf = pBuf;
    ctx.max = u32MaxEntries;
    ctx.written = 0;

    /* Dir-entry keys are tag|(parent<<16)|hash — every entry
     * of one directory sits in the contiguous half-open range
     * [parent<<16, (parent+1)<<16) inside the DIR_ENTRY tag
     * space.  Walking just that range touches only the btree
     * nodes on the path plus the directory's leaves; the old
     * full-tree walk read EVERY node block (INODE/EXTENT/
     * SNAPSHOT subtrees included) once per getdents call. */
    if (u64DirInode >= (UINT64_MAX >> 16))
        return -1;
    mkrn_yafs_btree_walk_range(
        u64RootTree,
        mkrn_yafs_make_key(YAFS_KS_DIR_ENTRY,
                           u64DirInode << 16),
        mkrn_yafs_make_key(YAFS_KS_DIR_ENTRY,
                           (u64DirInode + 1) << 16),
        readdir_cb, &ctx);

    return (int)ctx.written;
}

#define YAFS_RAMDISK_BLOCKS 1024
#define YAFS_BLOCK_SIZE 4096

int
mkrn_yafs_fs_stats(uint32_t *pBlockSize,
                   uint32_t *pTotalBlocks,
                   uint32_t *pFreeBlocks,
                   uint32_t *pUsedBlocks)
{
    if (!pBlockSize || !pTotalBlocks || !pFreeBlocks
        || !pUsedBlocks)
        return -1;
    *pBlockSize = YAFS_BLOCK_SIZE;
    *pTotalBlocks = YAFS_RAMDISK_BLOCKS;
    *pUsedBlocks = (uint32_t)total_allocated;
    *pFreeBlocks = *pTotalBlocks - *pUsedBlocks;
    return 0;
}

struct fhs_entry {
    const char *path;
    uint32_t    type;
    const char *target;
};

static const struct fhs_entry fhs_tree[] = {
    { "bin",                YAFS_FT_SYMLINK,
      "/usr/bin" },
    { "boot",               YAFS_FT_SYMLINK,
      "/sys/boot" },
    { "dev",                YAFS_FT_DIR,      NULL },
    { "devices",            YAFS_FT_DIR,      NULL },
    { "etc",                YAFS_FT_DIR,      NULL },
    { "export",             YAFS_FT_DIR,      NULL },
    { "home",               YAFS_FT_SYMLINK,
      "/export/home" },
    { "lib",                YAFS_FT_DIR,      NULL },
    { "mnt",                YAFS_FT_DIR,      NULL },
    { "opt",                YAFS_FT_DIR,      NULL },
    { "root",               YAFS_FT_SYMLINK,
      "/export/root" },
    { "sbin",               YAFS_FT_DIR,      NULL },
    { "sys",                YAFS_FT_DIR,      NULL },
    { "tmp",                YAFS_FT_DIR,      NULL },
    { "usr",                YAFS_FT_DIR,      NULL },
    { "var",                YAFS_FT_DIR,      NULL },
    { "dev/dsk",            YAFS_FT_DIR,      NULL },
    { "dev/rdsk",           YAFS_FT_DIR,      NULL },
    { "devices/fd",         YAFS_FT_DIR,      NULL },
    { "export/cfg",         YAFS_FT_DIR,      NULL },
    { "export/home",        YAFS_FT_DIR,      NULL },
    { "export/PATH",        YAFS_FT_DIR,      NULL },
    { "export/root",        YAFS_FT_DIR,      NULL },
    { "mnt/media",          YAFS_FT_DIR,      NULL },
    { "sys/proc",           YAFS_FT_DIR,      NULL },
    { "sys/srv",            YAFS_FT_DIR,      NULL },
    { "sys/boot",           YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel",    YAFS_FT_DIR,      NULL },
    { "sys/boot/efi",       YAFS_FT_DIR,      NULL },
    { "sys/boot/grub",      YAFS_FT_DIR,      NULL },
    { "sys/boot/initrd",    YAFS_FT_DIR,      NULL },
    { "sys/boot/firmware",  YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/genunix",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/kernel/arch",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/fs",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/drv",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/misc",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/arch/amd64",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/arch/i386",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/kernel/fs/yafs.ko",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/kernel/fs/tmpfs.ko",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/kernel/drv/virtio_blk.ko",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/kernel/drv/e1000.ko",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/kernel/misc/aes_x86_64.ko",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/efi/bootx64.efi",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/efi/bootia32.efi",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/efi/fonts",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/grub/grub.cfg",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/grub/locale",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/grub/themes",
      YAFS_FT_DIR,      NULL },
    { "sys/boot/initrd/initrd.img",
      YAFS_FT_REG_FILE, NULL },
    { "sys/boot/firmware/intel-ucode.img",
      YAFS_FT_REG_FILE, NULL },
    { "usr/bin",             YAFS_FT_DIR,      NULL },
    { "usr/include",         YAFS_FT_DIR,      NULL },
    { "usr/lib",             YAFS_FT_DIR,      NULL },
    { "usr/libexec",         YAFS_FT_DIR,      NULL },
    { "usr/local",           YAFS_FT_DIR,      NULL },
    { "usr/sbin",            YAFS_FT_DIR,      NULL },
    { "usr/share",           YAFS_FT_DIR,      NULL },
    { "var/run",             YAFS_FT_DIR,      NULL },
    { "var/tmp",             YAFS_FT_DIR,      NULL },
};

static int
create_inode(uint64_t *pRoot, uint64_t u64InodeNr,
             uint32_t u32FileType,
             const char *pName)
{
    uint8_t buf[4096];
    mkrn_memset(buf, 0, sizeof(buf));
    struct yafs_inode_value *pIv =
        (struct yafs_inode_value *)buf;
    pIv->file_type = u32FileType;
    pIv->link_count = 1;
    mkrn_strncpy(pIv->name, pName, sizeof(pIv->name) - 1);
    pIv->name[sizeof(pIv->name) - 1] = '\0';

    uint64_t u64Lba = mkrn_yafs_dev_alloc_block();
    if (u64Lba == 0)
        return -1;
    if (mkrn_yafs_dev_write(u64Lba, buf) != 0) {
        mkrn_yafs_dev_free_block(u64Lba);
        return -1;
    }

    uint64_t u64Key = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64InodeNr);
    bool bInserted;
    if (mkrn_yafs_btree_insert(pRoot, u64Key, u64Lba,
                               &bInserted)
        != 0)
    {
        mkrn_yafs_dev_free_block(u64Lba);
        return -1;
    }
    return 0;
}

static int
create_link(uint64_t *pRoot, uint64_t u64ParentInode,
            const char *pName, uint64_t u64ChildInode)
{
    uint64_t u64Hash = mkrn_yafs_name_hash(pName);
    uint64_t u64Key = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64ParentInode << 16)
            | (u64Hash & 0xFFFF));
    bool bInserted;
    if (mkrn_yafs_btree_insert(pRoot, u64Key,
                               u64ChildInode,
                               &bInserted)
        != 0)
        return -1;
    /* bInserted == false means the dir-entry key already exists —
     * either a duplicate name or a 16-bit name-hash collision with a
     * DIFFERENT sibling.  Either way the old entry would be silently
     * overwritten (value replaced: the old file becomes unreachable
     * = data loss).  Refuse instead.  The static FHS tree is
     * collision-free (audited), so boot-time creation is unaffected. */
    if (!bInserted)
        return -1;
    return 0;
}

struct parent_ctx {
    uint64_t root_tree;
    uint64_t child_inode;
    uint64_t parent_inode;
};

static int
parent_cb(uint64_t u64Key, yafs_entry_t value,
          void *pCtx)
{
    struct parent_ctx *pPc =
        (struct parent_ctx *)pCtx;
    uint64_t u64Tag = mkrn_yafs_key_tag(u64Key);
    if (u64Tag != YAFS_KS_DIR_ENTRY)
        return 0;
    if (value == pPc->child_inode) {
        uint64_t u64Kv = mkrn_yafs_key_value(u64Key);
        pPc->parent_inode = u64Kv >> 16;
        return 1;
    }
    return 0;
}

static uint64_t
lookup_parent(uint64_t u64RootTree,
              uint64_t u64ChildInode)
{
    if (u64ChildInode == 0)
        return 0;
    if (u64ChildInode == 1)
        return 1;
    struct parent_ctx pc;
    pc.child_inode = u64ChildInode;
    pc.parent_inode = 0;
    /* '..' resolution only needs DIR_ENTRY keys — restrict
     * the walk to the tag-1 key space instead of reading
     * the whole tree (INODE/EXTENT/SNAPSHOT blocks too). */
    mkrn_yafs_btree_walk_range(
        u64RootTree,
        mkrn_yafs_make_key(YAFS_KS_DIR_ENTRY, 0),
        mkrn_yafs_make_key(YAFS_KS_DIR_ENTRY + 1, 0),
        parent_cb, &pc);
    return pc.parent_inode;
}

static uint64_t
lookup_name(uint64_t u64RootTree,
            uint64_t u64ParentInode,
            const char *pName)
{
    uint64_t u64Hash = mkrn_yafs_name_hash(pName);
    uint64_t u64Key = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64ParentInode << 16)
            | (u64Hash & 0xFFFF));
    yafs_entry_t val;
    if (mkrn_yafs_btree_lookup(u64RootTree, u64Key,
                               &val)
        == 0)
        return val;
    return 0;
}

int
mkrn_yafs_lookup_name(uint64_t u64RootTree,
                      uint64_t u64DirInode,
                      const char *pName,
                      uint64_t *pChildInode)
{
    if (!pName || !pChildInode)
        return -1;
    uint64_t u64Ino =
        lookup_name(u64RootTree, u64DirInode, pName);
    if (u64Ino == 0)
        return -1;
    *pChildInode = u64Ino;
    return 0;
}

static uint64_t
lookup_path(uint64_t u64RootTree, const char *pPath)
{
    if (!pPath || !*pPath)
        return 1;
    uint64_t u64Inode = 1;
    const char *p = pPath;
    const char *pSlash;

    while ((pSlash = strchr_y(p, '/')) != NULL) {
        int plen = (int)(pSlash - p);
        if (plen == 0) {
            p++;
            continue;
        }
        char comp[256];
        int j;
        for (j = 0;
             j < plen && j < (int)sizeof(comp) - 1;
             j++)
            comp[j] = p[j];
        comp[j] = '\0';
        if (comp[0] == '.' && comp[1] == '\0') {
            p = pSlash + 1;
            continue;
        }
        if (comp[0] == '.' && comp[1] == '.'
            && comp[2] == '\0')
        {
            u64Inode =
                lookup_parent(u64RootTree, u64Inode);
            if (u64Inode == 0)
                return 0;
            p = pSlash + 1;
            continue;
        }
        u64Inode =
            lookup_name(u64RootTree, u64Inode, comp);
        if (u64Inode == 0)
            return 0;
        p = pSlash + 1;
    }
    if (*p) {
        if (p[0] == '.' && p[1] == '\0')
            return u64Inode;
        if (p[0] == '.' && p[1] == '.'
            && p[2] == '\0')
            return lookup_parent(u64RootTree, u64Inode);
        u64Inode =
            lookup_name(u64RootTree, u64Inode, p);
    }
    return u64Inode;
}

uint64_t
mkrn_yafs_lookup_path(uint64_t u64RootTree,
                      const char *pPath)
{
    return lookup_path(u64RootTree, pPath);
}

int
mkrn_yafs_mkdir(uint64_t *pRoot,
                uint64_t u64ParentInode,
                const char *pName,
                uint64_t u64InodeNr,
                uint64_t *pNewInodeOut)
{
    if (!pRoot || !pName || !pNewInodeOut)
        return -1;
    if (create_inode(pRoot, u64InodeNr, YAFS_FT_DIR,
                     pName)
        != 0)
        return -1;
    if (create_link(pRoot, u64ParentInode, pName,
                    u64InodeNr)
        != 0) {
        /* Roll back the inode created by create_inode so a refused
         * link (duplicate/collision) doesn't leak the block + orphan
         * the inode key. */
        bool bDeleted;
        uint64_t u64Ik = mkrn_yafs_make_key(
            YAFS_KS_INODE, u64InodeNr);
        yafs_entry_t lba_val;
        if (mkrn_yafs_btree_lookup(*pRoot, u64Ik,
                                   &lba_val) == 0)
            mkrn_yafs_dev_free_block(lba_val);
        mkrn_yafs_btree_delete(pRoot, u64Ik,
                               &bDeleted);
        return -1;
    }
    *pNewInodeOut = u64InodeNr;
    return 0;
}

int
mkrn_yafs_unlink(uint64_t *pRoot,
                 uint64_t u64ParentInode,
                 const char *pName)
{
    if (!pRoot || !pName)
        return -1;
    uint64_t u64Hash = mkrn_yafs_name_hash(pName);
    uint64_t u64Key = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64ParentInode << 16)
            | (u64Hash & 0xFFFF));
    bool bDeleted;
    int ret = mkrn_yafs_btree_delete(pRoot, u64Key,
                                     &bDeleted);
    if (ret != 0)
        return -1;
    return bDeleted ? 0 : -1;
}

int
mkrn_yafs_rmdir(uint64_t *pRoot,
                uint64_t u64ParentInode,
                const char *pName)
{
    return mkrn_yafs_unlink(pRoot, u64ParentInode,
                            pName);
}

int
mkrn_yafs_rename(uint64_t *pRoot,
                 uint64_t u64OldParent,
                 const char *pOldName,
                 uint64_t u64NewParent,
                 const char *pNewName)
{
    if (!pRoot || !pOldName || !pNewName)
        return -1;

    uint64_t u64Child =
        lookup_name(*pRoot, u64OldParent, pOldName);
    if (u64Child == 0)
        return -1;

    bool bInserted;
    uint64_t u64Nh = mkrn_yafs_name_hash(pNewName);
    uint64_t u64Nk = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64NewParent << 16) | (u64Nh & 0xFFFF));
    if (mkrn_yafs_btree_insert(pRoot, u64Nk, u64Child,
                               &bInserted)
        != 0)
        return -1;
    /* Same guard as create_link: bInserted == false means the new
     * dir-entry key already exists (duplicate name or 16-bit hash
     * collision).  Proceeding would silently overwrite the sibling
     * entry and orphan its file.  Nothing has been deleted yet, so a
     * plain refusal leaves both entries intact. */
    if (!bInserted)
        return -1;

    uint64_t u64Oh = mkrn_yafs_name_hash(pOldName);
    uint64_t u64Ok = mkrn_yafs_make_key(
        YAFS_KS_DIR_ENTRY,
        (u64OldParent << 16) | (u64Oh & 0xFFFF));
    bool bDeleted;
    if (mkrn_yafs_btree_delete(pRoot, u64Ok, &bDeleted)
            != 0
        || !bDeleted)
        return -1;

    uint64_t u64Ik = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64Child);
    yafs_entry_t inode_lba_val;
    if (mkrn_yafs_btree_lookup(*pRoot, u64Ik,
                               &inode_lba_val)
        == 0)
    {
        uint8_t *iv_buf = g_inode_scratch;
        if (mkrn_yafs_dev_read(inode_lba_val, iv_buf)
            == 0)
        {
            struct yafs_inode_value *pIv =
                (struct yafs_inode_value *)iv_buf;
            mkrn_strncpy(pIv->name, pNewName,
                    sizeof(pIv->name) - 1);
            pIv->name[sizeof(pIv->name) - 1] = '\0';
            mkrn_yafs_dev_write(inode_lba_val, iv_buf);
        }
    }

    return 0;
}

int
mkrn_yafs_create_fhs(uint64_t *pRootLba)
{
    uint64_t u64RootTree = *pRootLba;
    uint64_t u64NextInode = 2;
    int count =
        (int)(sizeof(fhs_tree) / sizeof(fhs_tree[0]));

    if (create_inode(&u64RootTree, 1, YAFS_FT_DIR, "/")
        != 0)
        goto err;

    mkrn_console_write(
        "YAFS: Creating FHS tree...\n");

    for (int i = 0; i < count; i++) {
        const char *pPath = fhs_tree[i].path;
        uint32_t u32Type = fhs_tree[i].type;
        const char *pTarget = fhs_tree[i].target;

        const char *pSlash = strrchr_y(pPath, '/');
        const char *pName;
        uint64_t u64Parent = 1;

        if (pSlash) {
            pName = pSlash + 1;
            int plen = (int)(pSlash - pPath);
            if (plen > 0) {
                char parent_path[256];
                int j;
                for (j = 0;
                     j < plen
                     && j < (int)sizeof(parent_path)
                                - 1;
                     j++)
                    parent_path[j] = pPath[j];
                parent_path[j] = '\0';
                u64Parent =
                    lookup_path(u64RootTree,
                                parent_path);
                if (u64Parent == 0) {
                    mkrn_console_write(
                        "YAFS: parent not found for ");
                    mkrn_console_write(pPath);
                    mkrn_console_write("\n");
                    goto err;
                }
            }
        } else {
            pName = pPath;
        }

        uint64_t u64MyInode = u64NextInode++;

        if (u32Type == YAFS_FT_SYMLINK) {
            if (create_inode(&u64RootTree, u64MyInode,
                             YAFS_FT_SYMLINK,
                             pTarget ? pTarget : pName)
                != 0)
                goto err;
        } else {
            const char *pLabel = pName;
            if (create_inode(&u64RootTree, u64MyInode,
                             u32Type, pLabel)
                != 0)
                goto err;
        }

        if (create_link(&u64RootTree, u64Parent,
                        pName, u64MyInode)
            != 0)
            goto err;
    }

    *pRootLba = u64RootTree;

    mkrn_console_write("YAFS: FHS-OK nxt=");
    mkrn_console_write_dec(u64NextInode);
    mkrn_console_write(" r=0x");
    mkrn_console_write_hex(u64RootTree);
    mkrn_console_write(" *rl=0x");
    mkrn_console_write_hex(*pRootLba);
    mkrn_console_write("\n");

    return 0;

err:
    mkrn_console_write(
        "YAFS: FHS creation FAILED\n");
    return -1;
}

#define YAFS_BLOCKS_PER_EXTENT 1
/* Extent keys encode the block index in 16 bits
 * ((inode << 16) | (blockIdx & 0xFFFF)), so a file can address at
 * most 65536 * 4 KB = 256 MB.  Past that the index silently wraps to
 * block 0 and the write path would clobber the file's own head —
 * refuse instead of corrupting. */
#define YAFS_MAX_FILE_BLOCKS 0x10000ULL

int
mkrn_yafs_read_file_data(struct yafs_mount *pM,
                         uint64_t u64Inode,
                         void *pBuf, uint64_t u64Offset,
                         uint32_t u32Size,
                         uint32_t *pRead)
{
    (void)pM;
    if (!pBuf || !pRead)
        return -1;
    *pRead = 0;
    if (u32Size == 0)
        return 0;

    uint8_t *pOut = (uint8_t *)pBuf;
    uint32_t u32Remaining = u32Size;
    uint32_t u32BlockSize = YAFS_DATA_BLOCK_SIZE;
    uint64_t u64BlockIdx = u64Offset / u32BlockSize;
    uint32_t u32BlockOff =
        (uint32_t)(u64Offset % u32BlockSize);

    /* Past the 16-bit block-index ceiling the key would wrap to
     * block 0 and alias the file's head — stop with a short read
     * instead of returning wrong data. */
    if (u64BlockIdx >= YAFS_MAX_FILE_BLOCKS)
        return 0;

    while (u32Remaining > 0) {
        uint64_t u64ExtentKey = mkrn_yafs_make_key(
            YAFS_KS_EXTENT,
            (u64Inode << 16)
                | (u64BlockIdx & 0xFFFF));
        yafs_entry_t ext_val;
        if (mkrn_yafs_btree_lookup(
                root_yafs_tree, u64ExtentKey,
                &ext_val)
            != 0)
            break;

        uint64_t u64Lba =
            mkrn_yafs_extent_lba(ext_val);
        uint32_t u32ExtentLen =
            mkrn_yafs_extent_length(ext_val);
        if (u32ExtentLen == 0)
            break;

        static uint8_t block_buf[YAFS_DATA_BLOCK_SIZE];
        for (uint32_t bi = 0;
             bi < u32ExtentLen && u32Remaining > 0;
             bi++)
        {
            uint32_t u32Copy =
                u32BlockSize - u32BlockOff;
            if (u32Copy > u32Remaining)
                u32Copy = u32Remaining;

            if (u32Copy == u32BlockSize) {
                /* Aligned full block: dev_read copies straight into
                 * the caller's buffer — no block_buf round trip. */
                if (mkrn_yafs_dev_read(u64Lba + bi,
                                       pOut)
                    != 0)
                    return -1;
            } else {
                if (mkrn_yafs_dev_read(u64Lba + bi,
                                       block_buf)
                    != 0)
                    return -1;
                mkrn_memcpy(pOut,
                       block_buf + u32BlockOff,
                       u32Copy);
            }
            pOut += u32Copy;
            u32Remaining -= u32Copy;
            *pRead += u32Copy;
            u32BlockOff = 0;
            u64BlockIdx++;
        }
    }

    return 0;
}

int
mkrn_yafs_write_file_data(struct yafs_mount *pM,
                          uint64_t u64Inode,
                          const void *pBuf,
                          uint64_t u64Offset,
                          uint32_t u32Size,
                          uint32_t *pWritten)
{
    (void)pM;
    if (!pBuf || !pWritten)
        return -1;
    *pWritten = 0;
    if (u32Size == 0)
        return 0;

    const uint8_t *pIn = (const uint8_t *)pBuf;
    uint32_t u32Remaining = u32Size;
    uint32_t u32BlockSize = YAFS_DATA_BLOCK_SIZE;
    uint64_t u64BlockIdx = u64Offset / u32BlockSize;
    uint32_t u32BlockOff =
        (uint32_t)(u64Offset % u32BlockSize);

    /* Same 16-bit block-index ceiling as the read path: a key past
     * 0xFFFF wraps to block 0 and overwrites the file's head.  Stop
     * with a short write so the caller sees the truncation. */
    if (u64BlockIdx >= YAFS_MAX_FILE_BLOCKS)
        return 0;

    while (u32Remaining > 0) {
        uint64_t u64ExtentKey = mkrn_yafs_make_key(
            YAFS_KS_EXTENT,
            (u64Inode << 16)
                | (u64BlockIdx & 0xFFFF));
        yafs_entry_t ext_val;
        bool bFound =
            (mkrn_yafs_btree_lookup(
                 root_yafs_tree, u64ExtentKey,
                 &ext_val)
             == 0);

        static uint8_t block_buf[YAFS_DATA_BLOCK_SIZE];
        uint64_t u64Lba;

        if (bFound) {
            u64Lba = mkrn_yafs_extent_lba(ext_val);
            uint32_t u32ExtentLen =
                mkrn_yafs_extent_length(ext_val);
            if (u32ExtentLen == 0)
                bFound = false;
            if (bFound
                && (u32BlockOff != 0
                    || u32Remaining < u32BlockSize))
            {
                if (mkrn_yafs_dev_read(u64Lba,
                                       block_buf)
                    != 0)
                    return -1;
            }
        }

        if (!bFound) {
            u64Lba = mkrn_yafs_dev_alloc_block();
            if (u64Lba == 0)
                return -1;
            mkrn_memset(block_buf, 0, u32BlockSize);
        }

        uint32_t u32Copy =
            u32BlockSize - u32BlockOff;
        if (u32Copy > u32Remaining)
            u32Copy = u32Remaining;

        if (bFound && u32BlockOff == 0
            && u32Copy == u32BlockSize)
        {
            /* Full aligned overwrite: write the caller's buffer
             * directly — skip the RMW read and block_buf copy. */
            if (mkrn_yafs_dev_write(u64Lba, pIn) != 0)
                return -1;
            pIn += u32Copy;
            u32Remaining -= u32Copy;
            *pWritten += u32Copy;
            u32BlockOff = 0;
            u64BlockIdx++;
            continue;
        }

        mkrn_memcpy(block_buf + u32BlockOff, pIn, u32Copy);

        if (mkrn_yafs_dev_write(u64Lba, block_buf)
            != 0)
        {
            if (!bFound)
                mkrn_yafs_dev_free_block(u64Lba);
            return -1;
        }

        if (!bFound) {
            bool bInserted;
            yafs_entry_t new_ext =
                mkrn_yafs_extent_pack(
                    u64Lba,
                    YAFS_BLOCKS_PER_EXTENT);
            if (mkrn_yafs_btree_insert(
                    &root_yafs_tree, u64ExtentKey,
                    new_ext, &bInserted)
                != 0)
            {
                mkrn_yafs_dev_free_block(u64Lba);
                return -1;
            }
        }

        pIn += u32Copy;
        u32Remaining -= u32Copy;
        *pWritten += u32Copy;
        u32BlockOff = 0;
        u64BlockIdx++;
    }

    uint64_t u64InodeKey = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64Inode);
    yafs_entry_t inode_lba_val;
    if (mkrn_yafs_btree_lookup(
            root_yafs_tree, u64InodeKey,
            &inode_lba_val)
        == 0)
    {
        uint8_t *iv_buf = g_inode_scratch;
        if (mkrn_yafs_dev_read(inode_lba_val, iv_buf)
            == 0)
        {
            struct yafs_inode_value *pIv =
                (struct yafs_inode_value *)iv_buf;
            uint64_t u64NewSize =
                u64Offset + *pWritten;
            uint64_t u64BlocksNeeded =
                (u64NewSize + u32BlockSize - 1)
                / u32BlockSize;
            if (u64NewSize > pIv->size)
                pIv->size = u64NewSize;
            if (u64BlocksNeeded > pIv->blocks)
                pIv->blocks = u64BlocksNeeded;
            mkrn_yafs_dev_write(inode_lba_val, iv_buf);
        }
    }

    return 0;
}

int
mkrn_yafs_truncate_inode(uint64_t u64Inode)
{
    /* O_TRUNC support: free every data extent, then reset the inode's
     * size/blocks to zero.  Walks by block index (0..blocks-1) using
     * per-key lookup+delete instead of btree_walk_range — mutating
     * the tree while a callback iterates it is unsafe.  inode->blocks
     * is maintained by write_file_data, so it is an upper bound on
     * live extent keys. */
    if (u64Inode == 0)
        return -1;
    uint64_t u64InodeKey = mkrn_yafs_make_key(
        YAFS_KS_INODE, u64Inode);
    yafs_entry_t inode_lba_val;
    if (mkrn_yafs_btree_lookup(
            root_yafs_tree, u64InodeKey, &inode_lba_val)
        != 0)
        return -1;

    uint8_t *iv_buf = g_inode_scratch;
    if (mkrn_yafs_dev_read(inode_lba_val, iv_buf) != 0)
        return -1;
    struct yafs_inode_value *pIv =
        (struct yafs_inode_value *)iv_buf;
    if (pIv->file_type != YAFS_FT_REG_FILE)
        return -1;

    for (uint64_t u64B = 0;
         u64B < pIv->blocks && u64B < YAFS_MAX_FILE_BLOCKS;
         u64B++)
    {
        uint64_t u64ExtentKey = mkrn_yafs_make_key(
            YAFS_KS_EXTENT,
            (u64Inode << 16) | (u64B & 0xFFFF));
        yafs_entry_t ext_val;
        if (mkrn_yafs_btree_lookup(
                root_yafs_tree, u64ExtentKey, &ext_val)
            == 0)
        {
            mkrn_yafs_dev_free_block(
                mkrn_yafs_extent_lba(ext_val));
            bool bDeleted;
            mkrn_yafs_btree_delete(&root_yafs_tree,
                                   u64ExtentKey,
                                   &bDeleted);
        }
    }

    pIv->size = 0;
    pIv->blocks = 0;
    return mkrn_yafs_dev_write(inode_lba_val, iv_buf);
}
