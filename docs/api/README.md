# M4KK1 API Reference Manual

## 1. Overview

M4KK1 is a minimal operating system for the **i386** (32-bit x86) architecture. Syscalls are issued via one of two interrupt paths:

- **`int 0x80`** — the Linux-compatible entry, used by programs compiled with a standard i386 toolchain. Syscall numbers use the `M4K_SC_*` namespace.
- **`int 0x4D`** — the native M4KK1 entry, used for M4KK1-specific operations. Syscall numbers use the `M4K_SYS_*` namespace (format `0x4D00XXXX`).

Both paths pass arguments in registers:

| Register | Purpose           |
| :------- | :---------------- |
| `%eax`   | Syscall number    |
| `%ebx`   | Argument 1        |
| `%ecx`   | Argument 2        |
| `%edx`   | Argument 3        |
| `%eax`   | Return value      |

On success the return value is 0 or positive. On failure it is `M4K_SC_ERROR` (`0xFFFFFFFF`).

---

## 2. Syscall Reference

### 2.1 Process Management

| Syscall (`int 0x80`)         | Number    | Description                                              |
| :--------------------------- | :-------- | :------------------------------------------------------- |
| `exit(int status)`           | `0x01`    | Terminate current process; status returned to parent.    |
| `fork()`                     | `0x02`    | Duplicate process (internally calls `m4k_fork_status`).  |
| `waitpid(pid, *status, opt)` | `0x07`    | Wait for child process.                                  |
| `execve(path, argv, envp)`   | `0x08`    | Replace process image.                                   |
| `getpid()`                   | `0x09`    | Return current PID.                                      |
| `getppid()`                  | `0x0A`    | Return parent PID.                                       |
| `kill(pid, sig)`             | `0x64`    | Send signal to process.                                  |
| `getprocs(*buf, max)`        | `0x85`    | Fill array of `struct procinfo`.                         |

| Syscall (`int 0x4D`)               | Number        | Description                                      |
| :--------------------------------- | :------------ | :----------------------------------------------- |
| `m4k_exit(int status)`             | `0x4D000001`  | Terminate current process.                       |
| `m4k_spawn(...)`                   | `0x4D000002`  | **Stub** — always returns -1 (not implemented).  |
| `m4k_fork_status(lo, hi, flags)`   | `0x4D000010`  | Fork with selective state inheritance (see 3.1). |
| `m4k_waitpid(pid, *status, opt)`   | `0x4D000011`  | Wait for child.                                  |
| `m4k_kill(pid, sig)`              | `0x4D000012`  | Send signal.                                     |
| `m4k_getppid()`                    | `0x4D000013`  | Return parent PID.                               |
| `m4k_getpid()`                     | `0x4D000014`  | Return current PID.                              |
| `m4k_setns(path, target, flags)`   | `0x4D000015`  | Set namespace.                                   |
| `m4k_getprocs(*buf, max)`          | `0x4D000016`  | Fill process info array.                         |

```c
struct procinfo { uint32_t pid, ppid, state; char name[32]; };
```

### 2.2 File I/O

| Syscall (`int 0x80`)          | Number    | Description                          |
| :---------------------------- | :-------- | :----------------------------------- |
| `open(path, flags)`           | `0x05`    | Open file; returns fd or error.      |
| `close(fd)`                   | `0x06`    | Close file descriptor.               |
| `read(fd, buf, count)`        | `0x03`    | Read from fd.                        |
| `write(fd, buf, count)`       | `0x04`    | Write to fd.                         |
| `unlink(path)`                | `0x15`    | Delete file.                         |
| `rename(old, new)`            | `0x16`    | Rename file.                         |
| `mkdir(path)`                 | `0x12`    | Create directory.                    |
| `rmdir(path)`                 | `0x13`    | Remove empty directory.              |
| `getdents(fd, *dirent, cnt)`  | `0x47`    | Read directory entries.              |

| Syscall (`int 0x4D`)          | Number        | Description                          |
| :---------------------------- | :------------ | :----------------------------------- |
| `m4k_read(fd, buf, count)`   | `0x4D000003`  | Read from fd.                        |
| `m4k_write(fd, buf, count)`  | `0x4D000004`  | Write to fd.                         |

**Open flags**:

```c
#define O_RDONLY   0x0001
#define O_WRONLY   0x0002
#define O_RDWR     0x0004
#define O_CREAT    0x0100
#define O_EXCL     0x0200
#define O_TRUNC    0x1000
#define O_APPEND   0x2000
#define O_NONBLOCK 0x4000
```

**Directory entry**:

```c
struct dirent { uint64_t inode; uint32_t type; char name[256]; uint64_t size; };
```

### 2.3 Memory

| Syscall (`int 0x80`) | Number    | Description                             |
| :------------------- | :-------- | :-------------------------------------- |
| `brk(addr)`          | `0x0B`    | Set program break; returns new break.   |

### 2.4 System Info & Time

| Syscall (`int 0x80`)              | Number    | Description                          |
| :-------------------------------- | :-------- | :----------------------------------- |
| `getcwd(buf, size)`              | `0x10`    | Get current working directory.       |
| `chdir(path)`                    | `0x11`    | Change working directory.            |
| `time()`                         | `0x1E`    | Get current timestamp.               |
| `uname(*utsname)`                | `0x73`    | Get system information.              |
| `reboot()`                       | `0x6D`    | Reboot the system.                   |
| `sysinfo(*sysinfo)`              | `0x84`    | Get system statistics.               |
| `statfs(*statfs)`                | `0x86`    | Get filesystem statistics.           |
| `pipe(int[2])`                   | `0x28`    | Create a pipe.                       |
| `dup2(old, new)`                 | `0x2A`    | Duplicate file descriptor.           |
| `mount(src, tgt, fstype)`        | `0x87`    | Mount a filesystem.                  |
| `umount(target)`                 | `0x88`    | Unmount a filesystem.                |
| `mountinfo(buf, max)`            | `0x89`    | Get mount table info.                |

**Data structures**:

```c
struct sysinfo { uint32_t total_ram, free_ram, used_ram, process_count; };
struct statfs  { uint32_t block_size, total_blocks, free_blocks, used_blocks; };
struct utsname { char sysname[65], nodename[65], release[65], version[65], machine[65]; };
```

---

## 3. Process Model

### 3.1 Process Creation

Processes are created via `m4k_fork_status(uint64_t inherit_mask, uint32_t flags)`. The `inherit_mask` is a bitmask that selects which parts of the parent's state the child inherits. There is **no `fork()`** in the native ABI; `M4K_SC_FORK` (int 0x80) is a compatibility wrapper that calls `m4k_fork_status` with a default mask. The `m4k_spawn` syscall is a stub that always returns -1.

### 3.2 Process States

Each process carries a state tag — a bitmask combining a scheduling state and optional wait/flag bits:

| Tag               | Description                              |
| :---------------- | :--------------------------------------- |
| `SCHED_READY`     | Runnable, waiting for CPU.               |
| `SCHED_RUNNING`   | Currently executing.                     |
| `SCHED_SLEEPING`  | Blocked on a resource.                   |
| `WAIT_FS`         | Waiting for filesystem I/O.              |
| `WAIT_PIPE`       | Waiting for pipe data.                   |
| `WAIT_TIMER`      | Waiting for a timer.                     |
| `WAIT_IPC`        | Waiting for inter-process communication. |
| `WAIT_CHILD`      | Waiting for child process state change.  |
| `TRACED`          | Being traced by ptrace-like mechanism.   |
| `STOPPED`         | Stopped by SIGSTOP or ptrace.            |
| `ZOMBIE`          | Terminated, awaiting `waitpid`.          |
| `IDLE`            | Idle (kernel internal).                  |

### 3.3 Signals

M4KK1 supports 10 signals:

| Signal     | Number | Default Action | Notes                         |
| :--------- | :----- | :------------- | :---------------------------- |
| `SIGABRT`  | 1      | Terminate      |                               |
| `SIGKILL`  | 2      | Terminate      | Uncatchable; PID 1 is immune. |
| `SIGTERM`  | 3      | Terminate      |                               |
| `SIGSTOP`  | 4      | Stop process   |                               |
| `SIGCONT`  | 5      | Ignore         | Resumes stopped process.      |
| `SIGTRAP`  | 6      | Terminate      |                               |
| `SIGCHLD`  | 7      | Ignore         | Child state change.           |
| `SIGPIPE`  | 8      | Terminate      | Broken pipe.                  |
| `SIGUSR1`  | 9      | Ignore         | User-defined.                 |
| `SIGUSR2`  | 10     | Ignore         | User-defined.                 |

Default action for unhandled signals: **fatal** (terminate) for KILL/TERM/ABRT, **stop** for STOP, **ignore** for all others.

### 3.4 Namespaces

Each process has an associated namespace with a **16-entry mount table** and per-process virtual filesystem resources. The `m4k_setns` syscall can reassign a process to a different namespace. `/sys/proc/` provides per-process status, cmdline, environ, cwd, ns, and ctl files.

---

## 4. Filesystem

M4KK1 uses **YAFS** — a log-structured copy-on-write B+Tree filesystem with 4096-byte blocks and 8-byte key-value entries. The VFS layer supports three filesystem types:

- **ramfs** — in-memory filesystem
- **YAFS** — persistent block-based filesystem
- **ProcFS** — virtual filesystem exposing kernel state

---

## 5. Error Codes

All syscalls return `M4K_SC_ERROR` (`0xFFFFFFFF`) on failure. The specific error type is not surfaced via a return value; programs should inspect errno-style state or consult the kernel log for details.

---

*This manual corresponds strictly to the M4KK1 kernel implementation. In case of discrepancy, the kernel source is authoritative.*
