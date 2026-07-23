# 第 4 章 系统管理

## 4.1 用户管理

### 用户数据库

文件: `/export/cfg/passwd.db`

格式: `username:uid:gid:home:shell:gecos:password_hash`

默认用户:
- `root` (UID 0) — 超级用户
- `testuser` (UID 1001) — 测试用户
- `nobody` (UID 65534) — 无特权用户

### 组数据库

文件: `/export/cfg/groups.db`

格式: `groupname:gid:member1,member2,...`

默认组:
- `prime` (GID 1001) — 特权组，成员可执行管理操作

### 用户管理命令

| 命令 | 功能 | 权限要求 |
|------|------|---------|
| `passwd` | 修改密码 | 任意用户（改自己）/ prime（改他人） |
| `usermod` | 修改用户账户 | prime 组 |
| `groupmod` | 修改组 | prime 组 |
| `cu <用户名>` | 强制注销用户 | prime 组 |
| `who` | 显示活跃会话 | 任意用户 |
| `userlog` | 登录历史 | 任意用户 |

### usermod 选项

```
-a, --add <组>     添加用户到附加组
-d, --del <组>     从组中移除用户
-s, --shell <路径>  更改登录 Shell
-g, --group <组>   设置主组
-L, --lock         锁定账户
-U, --unlock       解锁账户
```

## 4.2 登录流程

1. `init` 启动 `login` 进程
2. 提示用户名和密码
3. 验证 `/export/cfg/passwd.db`
4. 成功: 设置 UID/GID、绑定家目录、启动 Shell
5. 失败: 最多重试 3 次

## 4.3 权限系统

### quell 命令

以 `prime` 组成员身份执行特权命令，替代 `sudo`：

```bash
quell mount /dev/yafs0 /mnt
quell usermod -a --group prime alice
```

### prime 组

`prime` 组（GID 1001）是 M4KK1 中所有特权操作的单一人口点。组成员可无需密码执行管理命令。

### 资源限制

文件: `/export/login.conf`（BSD 风格登录类别）

```conf
default:\
    :cputime=infinity:\
    :datasize=256M:\
    :stacksize=8M:\
    :maxproc=100:\
    :openfiles=64:
```

## 4.4 系统调用接口

### 用户/组相关

| 系统调用 | 编号 | 功能 |
|----------|------|------|
| `m4k_getuid` | `0x4D000020` | 获取用户 ID |
| `m4k_setuid` | `0x4D000024` | 设置用户 ID |
| `m4k_getgid` | `0x4D000022` | 获取组 ID |
| `m4k_setgid` | `0x4D000025` | 设置组 ID |
| `m4k_getgroups` | `0x4D000026` | 获取组列表 |
| `m4k_chmod` | `0x4D000028` | 修改文件权限 |
| `m4k_chown` | `0x4D000029` | 修改文件所有者 |

### 会话管理

- `m4k_register_session(tty, uid, username)` — 注册登录会话
- `m4k_get_session_list(buf, max)` — 获取会话列表

会话信息通过 `/sys/sessions/<TTY>/` 暴露给用户空间。
