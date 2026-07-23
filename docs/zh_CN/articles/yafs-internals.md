# YAFS 文件系统内部原理

## 概述

YAFS (Yet Another File System) 是一个日志结构、写时复制 (COW) B+Tree 文件系统，专为 M4KK1 设计。

## 磁盘布局

```
Block 0:  超级块
Block 1:  超级块备份
Block 2+: B+Tree 节点 + 文件数据 + 空闲位图
```

### 超级块

```c
struct yafs_superblock {
    uint64_t magic;              // YAFS_MAGIC
    uint64_t root_tree_addr;     // 根 B+Tree 的 LBA
    uint64_t total_blocks;       // 总块数
    uint64_t block_size;         // 块大小 (4096)
    uint64_t free_blocks;        // 空闲块
    uint64_t used_blocks;        // 已用块
};
```

## B+Tree 设计

### 节点结构

每个节点 = 一个磁盘块 (4096 字节):

- **内部节点**: 键值对数组 + 子节点指针
- **叶子节点**: 文件条目（文件名 → inode 映射）

### 操作复杂度

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 文件查找 | O(log N) | B+Tree 遍历 |
| 文件创建 | O(log N) | 插入叶子节点 |
| 文件删除 | O(log N) | 删除叶子节点 |
| 目录遍历 | O(N) | 叶子节点扫描 |
| 块分配 | O(1) | 空闲位图 |

## 写时复制 (COW)

COW 是 YAFS 的核心特性:

1. 修改数据时，修改的块被写入新位置
2. 从叶子到根的所有路径节点都被复制到新位置
3. 根指针更新为新根
4. 旧根仍然完整 → 可实现快照

## 空闲空间管理

使用位图跟踪空闲块:
- 每个 bit 对应一个块
- 块分配: 位图扫描 + 标记已用
- 块释放: 位图标记空闲

## RAM 磁盘后端

当前 YAFS 运行在 RAM 磁盘上:
- 4096 个块
- 每个块 4096 字节
- 总计 16 MB

## 实现文件

```
sys/src/fs/yafs/
├── core/yafs/
│   ├── btree.c           # B+Tree 核心操作
│   ├── yafs_test.c       # 测试和初始化
│   └── yafs_vfs.c        # VFS 集成
├── include/
│   ├── yafs.h            # 超级块、inode、快照结构
│   └── yafs_btree.h      # B+Tree 节点结构
└── mkrn_yafs_inode.h     # Inode 内部结构
```

## 状态与路线图

### 已实现
- 超级块创建/读取
- B+Tree 插入、查找、删除、遍历
- 空闲块分配/释放
- FHS 目录树创建
- 文件创建/读取/写入/删除
- VFS 集成

### 规划中
- 日志/崩溃一致性
- COW 快照/回滚
- AES-128 XTS 加密
- 磁盘后端 (ATA/AHCI)
