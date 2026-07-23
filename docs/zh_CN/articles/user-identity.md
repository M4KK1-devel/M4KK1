# 用户身份模型

## 设计哲学

M4KK1 采用 "数据/配置分离" 架构，区别于 Linux `/etc/passwd` + `/home` 模型:

1. **数据 (重)**: 用户私有文件存储在 `/export`
2. **配置 (轻)**: 系统配置存储在 `/export/cfg/`
3. **服务隔离**: 系统服务运行在无特权的系统账户下

## 目录结构

```
/export/
├── cfg/              # 配置文件
│   ├── passwd.db     # 用户数据库
│   ├── groups.db     # 组数据库
│   └── timezone      # 时区
├── home/             # 用户家目录（物理存储）
│   └── {username}/
├── root/             # root 家目录
├── PATH/             # 各用户的 PATH 配置
│   ├── root          # root 的 PATH
│   ├── testuser      # testuser 的 PATH
│   └── default       # 默认 PATH
└── srv/              # 服务账户目录
    ├── daemon/
    ├── nobody/
    └── ...
```

## 用户数据库

文件: `/export/cfg/passwd.db`

格式: `username:uid:gid:home:shell:gecos:password_hash`

### 默认用户

| 用户 | UID | 用途 |
|------|-----|------|
| root | 0 | 超级用户 |
| daemon | 1 | 系统服务 |
| testuser | 1001 | 测试用户 |
| nobody | 65534 | 无特权用户 |

## 组数据库

文件: `/export/cfg/groups.db`

格式: `groupname:gid:member1,member2,...`

### prime 组

`prime` 组（GID 1001）是 M4KK1 权限系统的核心。组成员可执行管理操作:

- 修改 `/export/cfg/*`（包括用户账户、服务配置）
- 执行 `mount` / `umount`
- 执行 `usermod`、`groupmod`、`passwd`
- 执行 `kill`、`nice`

### quell 命令

替代 `sudo`，基于组成员身份而非复杂配置文件:

```bash
# 以 prime 组成员身份执行特权命令
quell usermod -a --group prime alice
quell mount /dev/yafs0 /mnt
```

## 密码哈希

使用 SHA-256 + 随机 salt:

```
$SHA$<base64_salt>$<base64_hash>
```

## 家目录自动挂载

登录时自动将 `/export/home/{username}` bind mount 到 `/home/{username}`，登出时自动卸载。

## PATH 管理

PATH 通过 `/export/PATH/<用户名>` 集中管理，无需 Shell 脚本配置:

```
/export/PATH/
├── root          # /bin:/sbin:/usr/bin:/usr/sbin
├── testuser      # /home/testuser/bin:/usr/local/bin:/bin
└── default       # 默认 PATH
```
