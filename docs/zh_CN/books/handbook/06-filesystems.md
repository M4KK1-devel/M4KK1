# 第 6 章 文件系统

## 6.1 文件系统层次

```
/
├── bin/          # 可执行文件
├── dev/          # 设备文件
├── export/       # 用户数据和配置
│   ├── cfg/      # 配置文件 (passwd.db, groups.db)
│   ├── home/     # 用户家目录
│   └── root/     # root 家目录
├── etc/          # 系统配置
├── mnt/          # 挂载点
├── sys/
│   ├── proc/     # 进程信息伪文件系统
│   └── sessions/ # 会话管理
├── tmp/          # 临时文件
└── usr/          # 用户程序
```

## 6.2 支持的文件系统

| 类型 | 说明 | 状态 |
|------|------|------|
| YAFS | COW B+Tree 文件系统 | ✅ 基础功能 |
| ramfs | 内存文件系统 | ✅ |
| ProcFS | 进程伪文件系统 | ✅ |

## 6.3 VFS 层

虚拟文件系统层 (VFS) 提供统一的文件操作接口。

### 文件操作

```c
int mkrn_vfs_open(const char *path, int flags);
int mkrn_vfs_close(int fd);
int mkrn_vfs_read(int fd, void *buf, int count);
int mkrn_vfs_write(int fd, const void *buf, int count);
int mkrn_vfs_seek(int fd, int offset, int whence);
```

### 文件描述符

- 每个进程最多 16 个文件描述符
- FD 0/1/2: 标准输入/输出/错误（串口）

### 打开标志

```c
#define M4K_O_RDONLY  0x0001
#define M4K_O_WRONLY  0x0002
#define M4K_O_RDWR    0x0004
#define M4K_O_CREAT   0x0100
#define M4K_O_EXCL    0x0200
#define M4K_O_TRUNC   0x1000
#define M4K_O_APPEND  0x2000
```

## 6.4 YAFS 文件系统

YAFS (Yet Another File System) 是一个日志结构、写时复制 (COW) B+Tree 文件系统，运行在 RAM 磁盘上（16 MB）。

### 磁盘布局

```
Block 0:  超级块 (备份 x2)
Block 1:  超级块备份
Block 2+: B+Tree 节点 + 文件数据 + 空闲位图
```

### B+Tree

- 每个节点 = 一个磁盘块 (4096 字节)
- 内部节点: 键值对数组 + 子节点指针
- 叶子节点: 文件条目 (文件名 → inode 映射)

### 写时复制 (COW)

修改 B+Tree 时，从叶子到根的所有路径节点都被复制到新位置，根指针更新。旧根仍然完整，可实现快照。

### 性能特征

| 操作 | 复杂度 |
|------|--------|
| 文件查找 | O(log N) |
| 文件创建 | O(log N) |
| 文件删除 | O(log N) |
| 目录遍历 | O(N) |
| 块分配 | O(1) |

### 当前状态

已实现:
- 超级块创建/读取
- B+Tree 插入、查找、删除、遍历
- 空闲块分配/释放
- FHS 目录树创建
- 文件创建/读取/写入/删除
- VFS 集成

未实现:
- 日志/崩溃一致性
- 快照/回滚
- AES-128 XTS 加密
- 磁盘后端 (ATA/AHCI)
