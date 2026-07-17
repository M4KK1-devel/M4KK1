好的，我会在现有 4P1 标准文档中**新增完整的第 6 章：用户与身份模型（4P1 Identity Model）**，并**扩展现有章节中不完善的部分**（内存管理、进程管理的细节补充）。

以下是完整的修订版标准文档，版本号升级为 **v1.2**。

---

# M4KK1 4P1 Standard Specification
**Version**: v1.2
**Status**: Draft
**Scope**: Complete system interface standard for M4KK1 operating system

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [System Architecture](#2-system-architecture)
3. [Memory Management](#3-memory-management)
4. [Process Management (4P1 Process Model)](#4-process-management-4p1-process-model)
   - 4.1 Process Identifiers (PID, PPID)
   - 4.2 Process State Model (The Tag Set Approach)
   - 4.3 Process Creation: `m4k_spawn` and `m4k_fork_status`
   - 4.4 Process Namespaces (Isolation & Views)
   - 4.5 The `/sys/proc` Filesystem Interface
   - 4.6 Process Control & Signals (Minimalist)
   - 4.7 System Call Reference
5. [Libraries](#5-libraries)
6. [User & Identity Model (4P1 Identity Model)](#6-user--identity-model-4p1-identity-model)
   - 6.1 Design Philosophy
   - 6.2 User Identifiers (UID, GID)
   - 6.3 Directory Structure & Auto-Mounting
   - 6.4 System Accounts & Service Users
   - 6.5 User Database (`/export/cfg/passwd.db`)
   - 6.6 Group Database (`/export/cfg/groups.db`)
   - 6.7 Password Storage & Hashing
   - 6.8 User Management Tools
   - 6.9 Privileged Execution: `quell` & `prime` Group
   - 6.10 Resource Limits: `login.conf` (BSD-style)
   - 6.11 Login Session Management
   - 6.12 User-Related System Calls
7. [Error Codes Reference](#7-error-codes-reference)
8. [Appendices & Examples](#8-appendices--examples)

---

## 1. Introduction

M4KK1 is an independent operating system that follows the **4P1 Standard**
("Leet of API"). The standard defines a lean, deterministic, and
minimalist interface between user-space programs and the kernel.

### Core Design Principles

- **Clarity over compatibility**: No POSIX baggage. Interfaces are
  designed for readability and predictability, not historical accident.
- **Simplicity over bloat**: Few interfaces, deterministic behavior,
  easy to understand and implement.
- **Native over emulation**: All tools and libraries built on the native
  M4KK1 ABI. No shim layers, no legacy compatibility.

### ABI Core Identifiers

| Element       | Value              |
|:--------------|:-------------------|
| Syscall trap  | `int 0x4D` (ASCII 'M') |
| Syscall prefix| `M4K_SYS_*`        |
| Error prefix  | `M4K_E*`           |
| Error range   | `0xM4Kxxxxx`       |

---

## 2. System Architecture

The M4KK1 kernel runs as a single address-space, freestanding i386 ELF
loaded by GRUB. Key architectural properties:

- **Kernel type**: Monolithic with modular subsystems (VFS, YAFS, IPC,
  Process Manager).
- **Scheduling**: Cooperative pre-emptive round-robin with state-tag
  dispatch.
- **I/O model**: Serial (COM1, 38400 8N1) as primary console; VFS
  abstraction for file-backed and pipe-backed I/O.
- **Executable format**: M4KK1 native ELF (freestanding, no libc,
  no dynamic linker).
- **Syscall convention**: `int 0x4D`, arguments in registers per
  x86_64 System V ABI (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`),
  return in `%rax` (negative values indicate errors; absolute value
  identifies the error code).

### Syscall Dispatch

All system calls are dispatched through a central handler in
`syscall.c`. The kernel decodes the syscall number from `%rax`,
validates arguments from userspace, and delegates to the appropriate
subsystem handler.

---

## 3. Memory Management

The kernel manages physical memory through a page allocator and
provides per-process virtual address space control via BRK and MMAP.

### 3.1 Physical Memory Allocator

- **Algorithm**: Buddy system with page bitmap tracking.
- **Minimum allocation unit**: 4 KiB (one page).
- **Maximum order**: 4 (16 contiguous pages; 64 KiB).
- **Reserved memory**: Kernel text, data, and BSS are locked and never
  returned to the allocator.

### 3.2 Virtual Memory Layout

| Range              | Purpose                     |
|:-------------------|:----------------------------|
| `0x00000000`       | Reserved / NULL-trap region |
| `0x00100000`       | Kernel loaded (1 MiB)       |
| `0x40000000`       | User code (executable load) |
| `0xC0000000`       | Kernel heap & pagetables    |
| `0xE0000000`       | User stack (grows downward) |

### 3.3 System Calls

| Syscall                                          | Number            | Description                              |
|:-------------------------------------------------|:------------------|:-----------------------------------------|
| `m4k_brk(void *addr)`                            | `M4K_SYS_BRK`     | Set program break; return new break addr |
| `m4k_map_memory(void *addr, size_t len, int prot, int flags, int fd, off_t off)` | `M4K_SYS_MMAP`  | Map memory region                        |
| `m4k_unmap_memory(void *addr, size_t len)`       | `M4K_SYS_MUNMAP`  | Unmap memory region                      |
| `m4k_get_memory_usage(uint32_t *pTotal, uint32_t *pUsed, uint32_t *pFree)` | `M4K_SYS_MEMINFO` | Query memory statistics |

### 3.4 Protection Flags

```c
#define M4K_PROT_NONE   0x00
#define M4K_PROT_READ   0x01
#define M4K_PROT_WRITE  0x02
#define M4K_PROT_EXEC   0x04
```

### 3.5 Mapping Flags

```c
#define M4K_MAP_PRIVATE     0x01
#define M4K_MAP_SHARED      0x02
#define M4K_MAP_ANONYMOUS   0x04
#define M4K_MAP_FIXED       0x08
#define M4K_MAP_GROWSDOWN   0x10
```

### 3.6 Memory Statistics (`m4k_get_memory_usage`)

Fills the provided pointers with:
- `pTotal`: Total physical memory in pages (4 KiB each).
- `pUsed`: Pages currently allocated (kernel + user).
- `pFree`: Pages available for allocation.

---

## 4. Process Management (4P1 Process Model)

The 4P1 process model replaces the traditional POSIX `fork/exec`
paradigm with a powerful, composable interface based on **state tag
sets**, **selective inheritance (`m4k_fork_status`)**, **per-process
namespaces**, and a **`/sys/proc` filesystem interface**.

### 4.1 Process Identifiers (PID, PPID)

Every process in M4KK1 is identified by a non-negative integer called
a **Process ID (PID)**. PIDs are allocated sequentially and recycled
only after the zombie is reaped.

#### PID 0 (Idle Process)

PID 0 is the kernel idle process. It is always in the `[SCHED_READY,
IDLE]` state and runs only when no other process is schedulable.

#### PID 1 (Init / M4SH)

PID 1 is the first user-space process (the M4KK1 shell, `M4SH`).
PID 1 has special properties:

- It is never killed (even `SIGKILL` is ignored).
- Orphaned children are re-parented to PID 1.
- PID 1 is responsible for reaping orphaned zombies.

#### Parent PID (PPID)

Each process carries a `ppid` field referencing its creator. When a
parent exits, all children are re-parented to PID 1. The PPID is
read-only from userspace and visible via `/sys/proc/<PID>/status`.

---

### 4.2 Process State Model (The Tag Set Approach)

#### 4.2.1 Principle

A process state is **not** a single mutually-exclusive value (e.g.,
`TASK_RUNNING`). Instead, it is a **set of boolean attributes** stored
as a `uint64_t` bitmask. Any combination of tags may be active
simultaneously, provided it is semantically coherent.

#### 4.2.2 Tag Definitions

```c
/* Scheduling class (exactly one of these SHALL be set) */
#define M4K_SCHED_READY     0x00000001ULL  /* Runnable, waiting for CPU */
#define M4K_SCHED_RUNNING   0x00000002ULL  /* Currently executing on CPU */
#define M4K_SCHED_SLEEPING  0x00000004ULL  /* Blocked, not runnable */

/* Waiting reasons (blocking — zero or more may be set with SLEEPING) */
#define M4K_WAIT_FS         0x00000100ULL  /* Disk / filesystem I/O */
#define M4K_WAIT_PIPE       0x00000200ULL  /* Pipe read/write blocked */
#define M4K_WAIT_TIMER      0x00000400ULL  /* Sleeping on a timer/tick */
#define M4K_WAIT_IPC        0x00000800ULL  /* IPC message send/recv */
#define M4K_WAIT_CHILD      0x00001000ULL  /* Waiting for child exit */
#define M4K_WAIT_LOCK       0x00002000ULL  /* Waiting for mutex/spinlock */
#define M4K_WAIT_TERMINAL   0x00004000ULL  /* Waiting for terminal input */

/* Debug / control tags */
#define M4K_TRACED          0x00010000ULL  /* Under ptrace-like inspection */
#define M4K_STOPPED         0x00020000ULL  /* Suspended by ctl or signal */

/* Lifecycle tags */
#define M4K_ZOMBIE          0x01000000ULL  /* Exited, not yet reaped */
#define M4K_IDLE            0x02000000ULL  /* Kernel idle process */

/* Convenience masks */
#define M4K_STATE_SCHED_MASK   0x00000007ULL  /* SCHED_READY|RUNNING|SLEEPING */
#define M4K_STATE_WAIT_MASK    0x00007F00ULL  /* All WAIT_* bits */
#define M4K_STATE_CTRL_MASK    0x00030000ULL  /* TRACED|STOPPED */
#define M4K_STATE_LIFE_MASK    0x03000000ULL  /* ZOMBIE|IDLE */
#define M4K_STATE_RESERVED     (~0x03037F07ULL) /* All reserved bits — writes rejected */
```

#### 4.2.3 Semantics

The scheduler checks **only** `M4K_SCHED_READY`. A process must have
`SCHED_READY` set (and `SCHED_RUNNING` clear) to be eligible for
scheduling. When the scheduler dispatches the process, it clears
`SCHED_READY` and sets `SCHED_RUNNING`. When the process yields or
blocks, the kernel clears `SCHED_RUNNING` and sets either `SCHED_READY`
(if still runnable) or `SCHED_SLEEPING` (if blocked).

The waiting-reason tags (`WAIT_FS`, `WAIT_PIPE`, etc.) are
**informational**; they help diagnostic tools (`/sys/proc/<PID>/status`)
and the VFS/IPC subsystems understand why a process is blocked. They
SHOULD be set by the subsystem that initiates the blocking operation.

#### 4.2.4 State Coherence Rules

| Condition                                          | Valid?     |
|:---------------------------------------------------|:-----------|
| `SCHED_READY` + `SCHED_RUNNING`                    | INVALID    |
| `SCHED_READY` + `SCHED_SLEEPING`                   | INVALID    |
| `SCHED_RUNNING` + `SCHED_SLEEPING`                 | INVALID    |
| `SCHED_SLEEPING` + `WAIT_PIPE`                     | Valid      |
| `SCHED_READY` + `WAIT_FS`                          | Valid      |
| `SCHED_SLEEPING` + `WAIT_FS` + `WAIT_TIMER`        | Valid      |
| `SCHED_SCHED_RUNNING` + `TRACED`                   | Valid      |
| `ZOMBIE` + `SCHED_READY`                           | INVALID    |
| `ZOMBIE` + any `WAIT_*`                            | INVALID    |

#### 4.2.5 Userspace Representation

When exposed to userspace (e.g., via `/sys/proc/<PID>/status`), the
state set is rendered as a comma-separated, bracket-delimited list.
Order is irrelevant — the representation is a **set**, not a vector.

```
State: [SCHED_SLEEPING, WAIT_PIPE]
State: [SCHED_READY]
State: [SCHED_RUNNING, TRACED]
State: [ZOMBIE]
```

---

### 4.3 Process Creation: `m4k_spawn` and `m4k_fork_status`

#### 4.3.1 `m4k_spawn` — Load and Execute a New Program

`m4k_spawn` creates a new process by loading an executable (M4KK1 ELF)
from the filesystem into a fresh address space and starting it.
It replaces the traditional POSIX `fork() + exec()` pattern.

```c
pid_t m4k_spawn(const char *path, char *const argv[],
                char *const envp[], uint32_t flags);
```

**Arguments**:

| Argument | Description                                      |
|:---------|:-------------------------------------------------|
| `path`   | Absolute or relative path to executable          |
| `argv`   | Null-terminated array of argument strings        |
| `envp`   | Null-terminated array of `KEY=VALUE` environment |
| `flags`  | Bitmask of spawn flags (see below)              |

**Spawn flags**:

```c
#define M4K_SPAWN_NEW_NS      0x0001  /* Create an empty namespace for child */
#define M4K_SPAWN_NEW_FD      0x0002  /* Copy (not share) file descriptor table */
#define M4K_SPAWN_NEW_ENV     0x0004  /* Copy (not share) environment vector */
#define M4K_SPAWN_NOWAIT      0x0008  /* Daemon mode — no zombie on exit */
#define M4K_SPAWN_TRACE       0x0010  /* Child starts with M4K_TRACED set */
```

**Return values**:

| Result             | Meaning                                       |
|:-------------------|:----------------------------------------------|
| `>= 0`             | PID of the new child process                  |
| `-M4K_EACCES`      | Permission denied on executable               |
| `-M4K_ENOENT`      | Executable not found                          |
| `-M4K_ENOMEM`      | Insufficient memory for new process           |
| `-M4K_EEXEC`       | Executable format invalid or corrupted        |
| `-M4K_EMFILE`      | Too many processes (system limit)             |

#### 4.3.2 `m4k_fork_status` — Selective State Inheritance

`m4k_fork_status` creates a new process that is a **clone of the
caller**, but inherits **only** the explicitly requested state tags.
This replaces the all-or-nothing POSIX `fork()` with a composable
"genetic filter."

```c
pid_t m4k_fork_status(uint64_t inherit_mask, uint32_t flags);
```

**Arguments**:

| Argument       | Description                                                   |
|:---------------|:--------------------------------------------------------------|
| `inherit_mask` | Bitmask of state flags the child inherits from the parent     |
| `flags`        | `RFPROC` plus optional `RF*` modifiers (see below)            |

**Flag Modifiers (`flags`)**:

```c
#define RFPROC      0x0001  /* Create new process (mandatory for fork) */
#define RFFDG       0x0002  /* Copy file descriptor table (else shared) */
#define RFNAMEG     0x0004  /* Copy namespace (else shared)          */
#define RFENVG      0x0008  /* Copy environment (else shared)        */
#define RFMEM       0x0010  /* Share memory address space (threads)  */
#define RFNOWAIT    0x0020  /* Daemon mode — child never becomes zombie */
```

**Validations**:

| Inherit Mask Violation                  | Error           |
|:----------------------------------------|:----------------|
| Inheriting `M4K_ZOMBIE`                 | `-M4K_EINVAL`   |
| Inheriting `M4K_IDLE`                   | `-M4K_EINVAL`   |
| Inheriting reserved bits                | `-M4K_EINVAL`   |
| Calling without `RFPROC`                | `-M4K_EINVAL`   |
| `RFNAMEG` set by unprivileged caller    | `-M4K_EPERM`    |

---

### 4.4 Process Namespaces (Isolation & Views)

#### 4.4.1 Concept

Every process carries a `struct m4k_namespace` that defines its
**private view** of the filesystem. Namespaces enable:

- Per-process mount tables (e.g., private `/tmp`, custom `/bin`).
- Redirecting `/dev/cons` to a pipe or window handle.
- Sandboxed execution without VFS-level chroot.

#### 4.4.2 Namespace Structure

```c
struct m4k_namespace_entry {
    char mnt_path[256];          /* Mount point (e.g., "/tmp") */
    char target_path[256];       /* Backing directory or device */
    uint32_t mnt_flags;          /* Mount flags (see below)    */
};

struct m4k_namespace {
    struct m4k_namespace_entry entries[16];  /* Per-process mount table */
    uint32_t entry_count;
};
```

**Mount flags**:

```c
#define M4K_MNT_RDONLY      0x0001  /* Read-only bind */
#define M4K_MNT_NOSUID      0x0002  /* Ignore setuid/setgid */
#define M4K_MNT_PRIVATE     0x0004  /* Not propagated to children */
#define M4K_MNT_BIND        0x0008  /* Bind-mount an existing path */
```

#### 4.4.3 Namespace Resolution Order

When the VFS resolves a path, it follows this priority:

1. If the path matches a mount point in the **calling process's**
   `ns_mount_table`, redirect to `target_path`.
2. If no match, consult the **global VFS root** (the M4KK1 FHS tree).

This means a process can shadow any path in the global tree without
affecting sibling processes.

#### 4.4.4 Setting Namespace Entries

```c
int m4k_setns(const char *path, const char *target, uint32_t flags);
```

| Argument | Description                                     |
|:---------|:------------------------------------------------|
| `path`   | Mount point in the calling process's view       |
| `target` | Existing global path to bind                     |
| `flags`  | `M4K_MNT_*` flags                               |

**Security rule**: Creating a **more isolated** namespace entry
(e.g., pointing `/tmp` to a process-specific directory) is allowed
for any process. Reducing isolation (e.g., sharing the parent's
namespace without copying) requires `CAP_SYS_ADMIN`.

---

### 4.5 The `/sys/proc` Filesystem Interface

#### 4.5.1 Mount Point

The process filesystem is mounted at `/sys/proc`. It is a
kernel-generated pseudo-filesystem (no persistent backing store).

#### 4.5.2 Directory Structure

```
/sys/proc/
├── <PID>/
│   ├── status      -- Human-readable state tags and metadata
│   ├── cmdline     -- Null-separated command-line arguments
│   ├── environ     -- Null-separated environment variables
│   ├── cwd         -- Symlink to current working directory
│   ├── ns/         -- Namespace mount entries for this process
│   └── ctl         -- Write-only control file (see §4.5.4)
```

#### 4.5.3 Per-File Specification

##### `status`

Example content:

```
PID: 42
PPID: 1
UID: 1000
GID: 1000
State: [SCHED_SLEEPING, WAIT_PIPE]
Memory: 4096 KB (heap) + 64 KB (stack)
User: alice
CMD: sead 's/foo/bar/' input.txt
```

##### `cmdline`

Null-separated argument vector, identical to the `argv` provided at
spawn time.

##### `environ`

Null-separated environment strings (`KEY=VALUE\0KEY=VALUE\0`).

##### `cwd`

A symbolic link pointing to the process's current working directory
(as resolved through its namespace). Reading the symlink returns the
absolute path.

##### `ns/`

A directory listing namespace mount entries for the process, one file
per active mount. Each file contains the target path and flags:

```
/tmp  ->  /home/user/.cache/m4kk1/tmp.42  (PRIVATE)
/bin  ->  /sandbox/bin                    (RDONLY, PRIVATE, BIND)
```

##### `ctl` (Control File)

The `ctl` file accepts **write-only** commands. Each command is a
single line. Multiple commands may be written in a single write call
by separating them with newlines.

**Supported commands**:

| Command                           | Description                                    |
|:----------------------------------|:-----------------------------------------------|
| `kill <signal>`                   | Send signal (e.g., `kill 9`)                   |
| `suspend`                         | Set `M4K_STOPPED`, clear `SCHED_READY`         |
| `resume`                          | Clear `M4K_STOPPED`, set `SCHED_READY`         |
| `niceness <value>`                | Set scheduling priority                        |
| `chroot <path>`                   | Change filesystem root for this process        |
| `chdir <path>`                    | Change current working directory               |
| `setenv <KEY>=<VALUE>`            | Set or update environment variable             |
| `unsetenv <KEY>`                  | Remove environment variable                    |
| `state +<TAG>`                    | Add a state tag (e.g., `+WAIT_TIMER`)          |
| `state -<TAG>`                    | Remove a state tag                             |
| `state =<TAG>`                    | Clear all tags, set only this one              |

**Examples**:

```bash
# Kill process 1234
echo "kill 9" > /sys/proc/1234/ctl

# Suspend a runaway process
echo "suspend" > /sys/proc/1234/ctl

# Force a process into traced state
echo "state +TRACED" > /sys/proc/1234/ctl
```

**Error responses**: Writes to `ctl` return the number of bytes
written on success, or `-M4K_EINVAL` / `-M4K_EPERM` on error for
the **first** failing command. The kernel processes commands
sequentially and stops at the first error.

#### 4.5.4 Permissions

| File    | Read | Write | Notes                                      |
|:--------|:-----|:------|:-------------------------------------------|
| status  | any  | —     | All processes visible (no hiding)          |
| cmdline | any  | —     |                                             |
| environ | any  | —     |                                             |
| cwd     | any  | —     | Symlink; target readable by all             |
| ns/     | any  | —     |                                             |
| ctl     | —    | owner | Only the process owner (or root) may write  |

**Rationale**: Process visibility is wide-open by design to simplify
diagnosis and monitoring. A future capability system may restrict
visibility, but the default 4P1 model favors transparency.

---

### 4.6 Process Control & Signals (Minimalist)

#### 4.6.1 Signals Model

M4KK1 defines a minimal signal set. Unlike POSIX, signals are **not**
the primary inter-process communication mechanism; they are reserved
for process control (termination, stop, resume, trace).

#### 4.6.2 Signal Definitions

```c
#define M4K_SIGABRT     1   /* Abort — abnormal termination       */
#define M4K_SIGKILL     2   /* Kill — immediate termination       */
#define M4K_SIGTERM     3   /* Term — graceful termination        */
#define M4K_SIGSTOP     4   /* Stop — suspend execution           */
#define M4K_SIGCONT     5   /* Continue — resume execution        */
#define M4K_SIGTRAP     6   /* Trap — debugger / trace event      */
#define M4K_SIGCHLD     7   /* Child — child exited or stopped    */
#define M4K_SIGPIPE     8   /* Pipe — write to broken pipe        */
#define M4K_SIGUSR1     9   /* User-defined 1                     */
#define M4K_SIGUSR2     10  /* User-defined 2                     */
```

#### 4.6.3 Signal Delivery

Signals are **not** queued. If a signal of a given number is already
pending for a process, a subsequent `m4k_kill` with the same number
has no effect. Signals are delivered at the next scheduler timeslice
or immediately if the target is in `SCHED_SLEEPING` (the kernel wakes
it).

| Signal    | Default Action              | Maskable?  | Notes                       |
|:----------|:----------------------------|:-----------|:----------------------------|
| SIGABRT   | Terminate + core-like dump  | No         |                             |
| SIGKILL   | Terminate immediately       | No         | PID 1 immune                |
| SIGTERM   | Terminate gracefully        | Yes        | Process may clean up        |
| SIGSTOP   | Suspend (set `STOPPED`)     | No         |                             |
| SIGCONT   | Resume (clear `STOPPED`)    | No         |                             |
| SIGTRAP   | Enter traced state          | No         | Set `TRACED`, wake tracer   |
| SIGCHLD   | Ignored by default          | Yes        | Sent to parent on child exit|
| SIGPIPE   | Terminate                   | Yes        |                             |
| SIGUSR1   | Ignored by default          | Yes        |                             |
| SIGUSR2   | Ignored by default          | Yes        |                             |

#### 4.6.4 Signal Wrapper: `m4k_kill`

```c
int m4k_kill(pid_t pid, int sig);
```

The `m4k_kill` syscall sends signal `sig` to process `pid`. It returns
0 on success or one of:

| Error           | Condition                                        |
|:----------------|:-------------------------------------------------|
| `-M4K_EINVAL`   | `sig` is not a valid signal number               |
| `-M4K_ESRCH`    | No process with `pid` exists                     |
| `-M4K_EPERM`    | Caller does not have permission to signal `pid`  |
| `-M4K_EBUSY`    | Signal already pending for this process          |

#### 4.6.5 Exit and Wait

```c
void m4k_exit(int status) __attribute__((noreturn));
pid_t m4k_waitpid(pid_t pid, int *status, int options);
```

**`m4k_exit`** terminates the calling process:

1. All open file descriptors are closed.
2. Children are re-parented to PID 1.
3. The process state is set to `[ZOMBIE]` (unless `RFNOWAIT` was set).
4. `SIGCHLD` is sent to the parent.
5. The scheduler is invoked (the process is never run again).

**`m4k_waitpid`** collects exit status:

| `options` flag     | Behavior                                     |
|:-------------------|:---------------------------------------------|
| `0`                | Block until `pid` exits                      |
| `M4K_WNOHANG`      | Return immediately if `pid` has not exited   |
| `M4K_WUNTRACED`    | Also return for stopped processes            |
| `M4K_WCONTINUED`   | Also return for resumed processes            |

| `pid` value    | Targets                                       |
|:---------------|:----------------------------------------------|
| `> 0`          | Specific PID                                  |
| `-1`           | Any child                                     |
| `0`            | Any child in the same process group           |
| `< -1`         | Any child in the process group `-pid`         |

Return values: PID of the child on success, 0 with `M4K_WNOHANG` if no
child has exited, or `-M4K_ECHILD` if no matching children exist.

#### 4.6.6 Status Encoding

The `status` integer returned by `m4k_waitpid` encodes exit information:

```c
#define M4K_WEXITSTATUS(s)  (((s) >> 8) & 0xFF)   /* Exit code         */
#define M4K_WTERMSIG(s)     ((s) & 0x7F)           /* Terminating signal */
#define M4K_WCOREDUMP(s)    ((s) & 0x80)           /* Core dumped?      */
#define M4K_WIFEXITED(s)    (M4K_WTERMSIG(s) == 0) /* Normal exit?      */
#define M4K_WIFSIGNALED(s)  (M4K_WTERMSIG(s) > 0)  /* Killed by signal? */
#define M4K_WIFSTOPPED(s)   ((s) & 0x10000)        /* Stopped?          */
#define M4K_WSTOPSIG(s)     (((s) >> 16) & 0xFF)   /* Stop signal       */
```

---

### 4.7 System Call Reference

#### 4.7.1 Process Management Syscalls

| Syscall                                                          | Number             | Description                                           |
|:-----------------------------------------------------------------|:-------------------|:------------------------------------------------------|
| `m4k_exit(int status)`                                           | `M4K_SYS_EXIT`     | Terminate current process                             |
| `m4k_spawn(const char *path, char *const argv[], char *const envp[], uint32_t flags)` | `M4K_SYS_SPAWN`    | Load and execute a new program                        |
| `m4k_fork_status(uint64_t inherit_mask, uint32_t flags)`         | `M4K_SYS_FORK_ST`  | Clone process with selective state inheritance        |
| `m4k_waitpid(pid_t pid, int *status, int options)`               | `M4K_SYS_WAIT`     | Wait for process state change                         |
| `m4k_kill(pid_t pid, int sig)`                                   | `M4K_SYS_KILL`     | Send signal to process                                |
| `m4k_getpid(void)`                                               | `M4K_SYS_GETPID`   | Return current PID                                    |
| `m4k_getppid(void)`                                              | `M4K_SYS_GETPPID`  | Return parent PID                                     |
| `m4k_setns(const char *path, const char *target, uint32_t flags)`| `M4K_SYS_SETNS`    | Add namespace mount entry for calling process         |

#### 4.7.2 Syscall Number Allocation

```c
#define M4K_SYS_EXIT        0x4D000001
#define M4K_SYS_SPAWN       0x4D000002
#define M4K_SYS_WAIT        0x4D000003
#define M4K_SYS_GETPID      0x4D000004
#define M4K_SYS_KILL        0x4D000005
#define M4K_SYS_GETPPID     0x4D000006
#define M4K_SYS_FORK_ST     0x4D000007
#define M4K_SYS_SETNS       0x4D000008
```

---

## 5. Libraries

### 5.1 Standard Library (`libm4k.a`)

M4KK1 provides a minimal C library for user-space programs. It is
**not** POSIX-compliant; it exposes only the native M4KK1 ABI.

**Headers**:
- `<m4k/syscall.h>`: System call wrappers (`m4k_*`).
- `<m4k/stdio.h>`: `musr_printf`, `musr_scanf`, `musr_getchar`, `musr_putchar`.
- `<m4k/stdlib.h>`: `musr_malloc`, `musr_free`, `musr_atoi`, `musr_rand`.
- `<m4k/string.h>`: `musr_strlen`, `musr_strcpy`, `musr_strcmp`, `musr_memcpy`.
- `<m4k/unistd.h>`: `musr_getpid`, `musr_getuid`, `musr_setuid`, etc.
- `<m4k/pwd.h>`: `musr_getpwnam`, `musr_getpwuid`, `musr_update_passwd`.
- `<m4k/grp.h>`: `musr_getgrnam`, `musr_in_group`.

### 5.2 Library Naming Convention

All user-space library functions carry the `musr_` prefix (defined in
the 4P1 Coding Style Specification). The system call wrappers use the
`m4k_` prefix reserved for the ABI layer.

---

## 6. User & Identity Model (4P1 Identity Model)

M4KK1 adopts a **"data/configuration separation"** architecture,
replacing the cluttered Linux `/etc/passwd` + `/home` model with a
clean, service-aware identity system.

### 6.1 Design Philosophy

1. **Data (heavy)**: User private files are stored under `/export`.
2. **Configuration (light)**: System-wide and user-specific configs
   are stored in `/export/cfg`, separate from data.
3. **Service isolation**: System services (DNS, Web, Mail) run under
   unprivileged system accounts stored in `/export/srv`, isolated
   from human users.

### 6.2 User Identifiers (UID, GID)

The kernel tracks **only numeric UID/GID** values. Username-to-UID
mapping is performed entirely in user-space by `libm4k`.

| Type         | Range          | Purpose                       |
|:-------------|:---------------|:------------------------------|
| `uid_t`      | `uint32_t`     | User ID (0-4294967295)        |
| `gid_t`      | `uint32_t`     | Group ID (0-4294967295)       |

**Reserved UIDs**:

| UID  | Name      | Purpose                        |
|:-----|:----------|:-------------------------------|
| 0    | `root`    | Superuser (unrestricted)       |
| 1    | `daemon`  | System daemon base             |
| 2    | `operator`| System operations (backup)     |
| 53   | `bind`    | DNS service (if used)          |
| 80   | `www`     | Web server service             |
| 200  | `news`    | News service                   |
| 1000 | `alice`   | First human user (demo)        |
| 65534| `nobody`  | Unprivileged catch-all         |

### 6.3 Directory Structure & Auto-Mounting

#### 6.3.1 Home Directory Mapping

**Rule**: `/home` is a **dynamic mount point**, not physical storage.
Physical storage lives under `/export`.

| User Type          | Physical Path                     | Mounted at       |
|:-------------------|:----------------------------------|:-----------------|
| Superuser (root)   | `/export/root`                    | `/root`          |
| Human user         | `/export/home/{username}`         | `/home/{username}`|
| Service user       | `/export/srv/{service_name}`      | Not mounted      |

**Implementation**: The login manager (`login`) calls `m4k_mount("/export/home/alice", "/home/alice", M4K_MNT_BIND)` after authentication. The mount is automatically unmounted on logout (via `pam_session` hook).

#### 6.3.2 System Configuration Directory (`/export/cfg`)

- **Location**: `/export/cfg/`
- **Purpose**: Replaces `/etc`. All global configuration files reside here.
- **Subdirectories**:
  - `/export/cfg/system/` — Hostname, network, boot scripts.
  - `/export/cfg/users/` — User-specific configuration (e.g., `.vimrc` symlinks).
  - `/export/cfg/services/` — Service configs (e.g., `httpd.conf`).

**Permissions**: Only `root` and members of the `prime` group may write
to `/export/cfg/`.

#### 6.3.3 PATH Storage (`/export/PATH`)

- **Principle**: `PATH` is managed centrally by the system, not via
  shell scripts.
- **Structure**:
  ```
  /export/PATH/
  ├── root          # Content: /bin:/sbin:/usr/bin:/usr/sbin
  ├── alice         # Content: /home/alice/bin:/usr/local/bin:/bin
  └── default       # Fallback path (for users without custom file)
  ```
- **Behavior**: On login, the Shell reads `/export/PATH/$USER` and sets
  the `PATH` environment variable accordingly.

### 6.4 System Accounts & Service Users

System accounts are used by **daemons**. Their home directories are
under `/export/srv` and their default shell is `/sbin/nologin`
(no interactive login).

**Standard System Users (predefined by 4P1)**:

| Username  | UID  | Purpose                     | Home Directory              |
|:----------|:-----|:----------------------------|:----------------------------|
| `daemon`  | 1    | Low-level system services   | `/export/srv/daemon`        |
| `operator`| 2    | System maintenance (backup) | `/export/srv/operator`      |
| `bind`    | 53   | DNS service                 | `/export/srv/bind`          |
| `www`     | 80   | Web server                  | `/export/srv/www`           |
| `news`    | 200  | News services               | `/export/srv/news`          |
| `nobody`  | 65534| Unprivileged catch-all      | `/export/srv/nobody`        |

**Security rule**: `usermod` must NOT grant login shells to system
accounts unless explicitly overridden with `-s /bin/m4sh`.

### 6.5 User Database (`/export/cfg/passwd.db`)

**Format**: Plain text, one user per line, colon-separated (7 fields):

```
username:uid:gid:home:shell:gecos:password_hash
```

**Example**:
```
root:0:0:/root:/bin/m4sh:System Administrator:$2a$10$...
alice:1001:1001:/home/alice:/bin/m4sh:Alice User:
nobody:65534:65534:/export/srv/nobody:/sbin/nologin:Unprivileged:
```

**Library functions** (`libm4k/musr_pwd.c`):

| Function                         | Description                                      |
|:---------------------------------|:-------------------------------------------------|
| `musr_getpwnam(const char *name)`| Look up entry by username                        |
| `musr_getpwuid(uid_t uid)`       | Look up entry by UID                             |
| `musr_update_passwd(entry)`      | Write entry back atomically (temp file + rename) |
| `musr_validate_password(entry, passwd)` | Verify password against hash              |

**Concurrency**: `usermod` and `groupmod` use `flock` on the database
file to prevent simultaneous writes.

### 6.6 Group Database (`/export/cfg/groups.db`)

**Format**: Plain text, one group per line, colon-separated (3 fields):

```
groupname:gid:user1,user2,user3
```

**Example**:
```
prime:1001:alice,root
staff:1002:bob,charlie
```

**Library functions** (`libm4k/musr_grp.c`):

| Function                         | Description                                      |
|:---------------------------------|:-------------------------------------------------|
| `musr_getgrnam(const char *name)`| Look up group by name                            |
| `musr_getgrgid(gid_t gid)`       | Look up group by GID                             |
| `musr_in_group(uid_t uid, const char *group)` | Check if user is in group           |
| `musr_update_group(entry)`       | Write group entry back atomically                |

### 6.7 Password Storage & Hashing

- **Algorithm**: `bcrypt` (cost factor 10, or hardware-adjusted).
- **Format**: `$2a$10$...` in the `password_hash` field.
- **Fallback**: If `crypt` is unavailable, SHA-256 with 16-byte salt
  is acceptable (`$sha256$...`).
- **Security**: `/export/cfg/passwd.db` must be `chmod 600` (read/write
  only by root and `prime` group members).

### 6.8 User Management Tools

All tools reside in `/bin` or `/usr/sbin` and follow the 4P1 command-line
standard (short `-x`, long `--word`, no flag stacking).

#### 6.8.1 `usermod` — Modify User Account

**Syntax**: `usermod [options] <username>`

| Option | Long        | Description                                      |
|:-------|:------------|:-------------------------------------------------|
| `-a`   | `--add`     | Add user to supplementary group                  |
| `-d`   | `--del`     | Remove user from group                           |
| `-s`   | `--shell`   | Change login shell                               |
| `-g`   | `--group`   | Set primary group                                |
| `-L`   | `--lock`    | Lock account (prevent login)                     |
| `-U`   | `--unlock`  | Unlock account                                   |
| `-h`   | `--help`    | Show help                                        |

**Examples**:
```bash
usermod -a --group prime alice   # Add alice to prime group
usermod -s /bin/m4sh bob         # Change bob's shell
```

**Permissions**: Requires membership in the `prime` group.

#### 6.8.2 `groupmod` — Modify Group

**Syntax**: `groupmod [options] <groupname>`

| Option | Long        | Description                                      |
|:-------|:------------|:-------------------------------------------------|
| `-a`   | `--add`     | Add user to group                                |
| `-d`   | `--del`     | Remove user from group                           |
| `-n`   | `--new`     | Rename group                                     |
| `-h`   | `--help`    | Show help                                        |

#### 6.8.3 `passwd` — Change Password

**Syntax**: `passwd [username]`
- No argument: Change current user's password.
- With argument (requires `prime` group): Force-change another user's password.

**Behavior**: Prompts for new password twice, updates the hash in
`/export/cfg/passwd.db`.

#### 6.8.4 `cu` — Cancel User (Complete Unlogin)

**Syntax**: `cu <username>`

**Function**:
1. Immediately lock the account (set `locked` flag in `passwd.db`).
2. Terminate all active processes owned by the user (send `SIGKILL`).
3. Unmount `/home/<username>`.
4. Log the action to `/var/log/auth.log`.

**Permissions**: Requires `prime` group membership.

**Restore**: Use `usermod -U <username>` to unlock.

#### 6.8.5 `who` — Show Active Sessions

**Syntax**: `who`

**Output**:
```
USER     TTY     LOGIN_TIME          FROM
alice    tty1    2026-07-17 14:23    local
root     ttyS0   2026-07-17 09:15    serial
```

**Data source**: Reads `/sys/sessions/` (kernel-exported session info).

#### 6.8.6 `userlog` — Show Login History

**Syntax**: `userlog [username]`

**Function**: Display `/var/log/userlog` entries.

**Output**:
```
2026-07-17 14:23:45  alice   login   success  tty1
2026-07-17 14:30:12  alice   logout  normal   tty1
2026-07-17 09:15:03  root    login   success  ttyS0
```

### 6.9 Privileged Execution: `quell` & `prime` Group

M4KK1 **does not** include `sudo`. Privilege escalation is handled by
`quell`, based on **group membership** (`prime` group) instead of
complex `/etc/sudoers` files.

#### 6.9.1 `quell` Principle

- **Command**: `quell <command> [args...]`
- **Mechanism**: Check if the caller belongs to the `prime` group.
- **If yes**: Execute `<command>` with `root` privileges (`euid=0`).
- **If no**: Return `M4K_EPERM` and log to `/var/log/auth.log`.

#### 6.9.2 The `prime` Group

The `prime` group (GID `1001`) is the **single entry point** for all
privileged operations. Users in this group can execute **any** command
via `quell` without a password.

**Operations allowed via `quell`** (no extra checks):
- Modify `/export/cfg/*` (including user accounts, service configs).
- Execute `mount` / `umount`.
- Execute `usermod`, `groupmod`, `passwd`.
- Execute `kill`, `nice`.

**Why `quell` is superior to `sudo`**:
- Configuration = group membership. No complex syntax.
- Auditing: Just monitor `quell` invocations in `/var/log/auth.log`.

#### 6.9.3 Implementation

`quell` is a SUID-root binary (`chmod u+s /bin/quell`). It:

1. Reads the real UID of the caller (`getuid()`).
2. Looks up the user's group list (via `/sys/proc/self/status` or
   `m4k_getgroups` syscall).
3. If `prime` is in the list: `execvp(<command>, args)` with `euid=0`.
4. Otherwise: print `quell: permission denied` and exit with code 1.

### 6.10 Resource Limits: `login.conf` (BSD-style)

M4KK1 adopts **login classes** (BSD style) for resource limiting,
instead of `ulimit` scripts.

#### 6.10.1 Configuration File

- **Path**: `/export/login.conf`
- **Format**:
  ```
  <class_name>:\
      :<key>=<value>:\
      :<key>=<value>:
  ```

#### 6.10.2 Standard Classes

```conf
# Default (unclassified users)
default:\
    :cputime=infinity:\
    :datasize=256M:\
    :stacksize=8M:\
    :maxproc=100:\
    :openfiles=64:

# Privileged staff (relaxed)
staff:\
    :cputime=infinity:\
    :datasize=1G:\
    :maxproc=512:\
    :openfiles=1024:

# Daemons (strict, to prevent DoS)
daemon:\
    :cputime=600:\
    :datasize=64M:\
    :maxproc=20:\
    :openfiles=32:
```

#### 6.10.3 Resource Keys (4P1 subset)

| Key           | Description                         | Example         |
|:--------------|:------------------------------------|:----------------|
| `cputime`     | CPU time in seconds                 | `600`           |
| `datasize`    | Data segment + heap                 | `256M`          |
| `stacksize`   | Stack size                          | `8M`            |
| `maxproc`     | Maximum number of processes         | `100`           |
| `openfiles`   | Maximum open files                  | `64`            |
| `memorylocked`| Locked memory limit                 | `64M`           |

**Implementation**: The `login` program reads `/export/login.conf`,
matches the user's class (or `default`), and calls `m4k_setrlimit()`
before spawning the user's shell.

### 6.11 Login Session Management

#### 6.11.1 Login Flow

1. `init` spawns `login` on each active console (`tty1`, `ttyS0`, etc.).
2. `login` prompts for username and password.
3. Validates against `/export/cfg/passwd.db` (using `musr_validate_password`).
4. On success:
   - Calls `m4k_setuid(user_uid)` to drop privileges.
   - Calls `m4k_chdir(home_dir)`.
   - Binds `/export/home/$USER` to `/home/$USER` (for human users).
   - Sets `PATH` from `/export/PATH/$USER`.
   - Spawns the user's shell (`/bin/m4sh`).
5. On failure:
   - Increments failure count.
   - Logs to `/var/log/userlog`.
   - Returns to prompt.

#### 6.11.2 Sessions and `/sys/sessions`

The kernel maintains a session list for each active login session.
Sessions are exposed under `/sys/sessions/<TTY>/`:

```
/sys/sessions/tty1/
├── uid          # 1001
├── pid          # 456
├── login_time   # 1721124309
└── username     # alice
```

These are used by `who`, `userlog`, and `cu`.

### 6.12 User-Related System Calls

#### 6.12.1 UID/GID Syscalls

| Syscall                              | Number          | Description                               |
|:-------------------------------------|:----------------|:------------------------------------------|
| `m4k_getuid(void)`                   | `0x4D000020`    | Return real UID                           |
| `m4k_geteuid(void)`                  | `0x4D000021`    | Return effective UID                      |
| `m4k_getgid(void)`                   | `0x4D000022`    | Return real GID                           |
| `m4k_getegid(void)`                  | `0x4D000023`    | Return effective GID                      |
| `m4k_setuid(uid_t uid)`              | `0x4D000024`    | Set both RUID and EUID (restricted)       |
| `m4k_setgid(gid_t gid)`              | `0x4D000025`    | Set both RGID and EGID (restricted)       |
| `m4k_getgroups(int size, gid_t *list)`| `0x4D000026`    | Get supplementary group list              |
| `m4k_setgroups(int size, gid_t *list)`| `0x4D000027`    | Set supplementary group list (root-only)  |

#### 6.12.2 Permission Control Syscalls

| Syscall                              | Number          | Description                               |
|:-------------------------------------|:----------------|:------------------------------------------|
| `m4k_chmod(const char *path, mode_t mode)` | `0x4D000028` | Change file permissions                   |
| `m4k_chown(const char *path, uid_t uid, gid_t gid)` | `0x4D000029` | Change file owner                   |
| `m4k_access(const char *path, int mode)` | `0x4D00002A` | Check file access permissions              |

#### 6.12.3 Resource Limit Syscalls

| Syscall                              | Number          | Description                               |
|:-------------------------------------|:----------------|:------------------------------------------|
| `m4k_setrlimit(int resource, const struct m4k_rlimit *rlp)` | `0x4D000030` | Set resource limit               |
| `m4k_getrlimit(int resource, struct m4k_rlimit *rlp)` | `0x4D000031` | Get resource limit               |

#### 6.12.4 Setuid Semantics

| Caller EUID | Target UID | Behavior                                      |
|:------------|:-----------|:----------------------------------------------|
| `0` (root)  | Any        | Sets both RUID and EUID to the target.        |
| Non-root    | Current RUID or EUID | Sets EUID only (drops privileges temporarily). |
| Non-root    | Other      | Returns `-M4K_EPERM` (cannot escalate).       |

**Rule**: The kernel **never** allows unprivileged processes to raise
their privileges. Once dropped, privileges cannot be regained unless
the process was started as root.

---

## 7. Error Codes Reference

All error codes are negative values. The absolute value is defined
below.

| Error Code       | Value         | Description                              |
|:-----------------|:--------------|:-----------------------------------------|
| `M4K_EPERM`      | `0x4D000001`  | Permission denied                        |
| `M4K_ENOENT`     | `0x4D000002`  | No such file or directory                |
| `M4K_EINTR`      | `0x4D000003`  | Interrupted system call                  |
| `M4K_EIO`        | `0x4D000004`  | I/O error                                |
| `M4K_ENOMEM`     | `0x4D000005`  | Out of memory                            |
| `M4K_EACCES`     | `0x4D000006`  | Access denied                            |
| `M4K_EFAULT`     | `0x4D000007`  | Bad address                              |
| `M4K_EBUSY`      | `0x4D000008`  | Device or resource busy                  |
| `M4K_EEXIST`     | `0x4D000009`  | File already exists                      |
| `M4K_ENODEV`     | `0x4D00000A`  | No such device                           |
| `M4K_ENOTDIR`    | `0x4D00000B`  | Not a directory                          |
| `M4K_EISDIR`     | `0x4D00000C`  | Is a directory                           |
| `M4K_EINVAL`     | `0x4D00000D`  | Invalid argument                         |
| `M4K_ENFILE`     | `0x4D00000E`  | Too many open files in system            |
| `M4K_EMFILE`     | `0x4D00000F`  | Too many open files for this process     |
| `M4K_ENOSPC`     | `0x4D000010`  | No space left on device                  |
| `M4K_ESPIPE`     | `0x4D000011`  | Illegal seek                             |
| `M4K_EROFS`      | `0x4D000012`  | Read-only filesystem                     |
| `M4K_ETIMEOUT`   | `0x4D000013`  | Operation timed out                      |
| `M4K_ECHILD`     | `0x4D000014`  | No child process                         |
| `M4K_ESRCH`      | `0x4D000015`  | No such process                          |
| `M4K_EEXEC`      | `0x4D000016`  | Exec format error                        |
| `M4K_EMFILEPROC` | `0x4D000017`  | Too many processes                       |

---

## 8. Appendices & Examples

### A. Shell Examples

#### A.1 Creating an Isolated Sandbox Shell

```bash
# Spawn a new M4SH with a fresh namespace and FD table
m4k_spawn /bin/m4sh --isolate
```

#### A.2 Detaching a Debugger (Clearing TRACED)

```c
// Clone current process, but strip the TRACED flag
pid_t child = m4k_fork_status(~M4K_TRACED & 0xFFFFFFFF,
                               RFPROC | RFFDG | RFMEM);
```

#### A.3 Suspending a Runaway Process via /sys

```bash
echo "suspend" > /sys/proc/1234/ctl
```

#### A.4 Diagnosing a Blocked Process

```bash
cat /sys/proc/1234/status
# Output: State: [SCHED_SLEEPING, WAIT_PIPE]
```

#### A.5 Creating a Thread (Shared Address Space)

```c
pid_t tid = m4k_fork_status(
    0,                          // No special state inherited
    RFPROC | RFMEM | RFFDG      // Thread: shared mem + fds
);
```

#### A.6 Dynamic Namespace — Private /tmp

```c
// Point our own /tmp to a private directory
m4k_setns("/tmp", "/home/user/.cache/m4kk1/private_tmp",
          M4K_MNT_PRIVATE | M4K_MNT_BIND);
```

#### A.7 Sending Signals via ctl

```bash
echo "kill 9" > /sys/proc/42/ctl
echo "state +WAIT_PIPE" > /sys/proc/42/ctl
echo "niceness 10" > /sys/proc/42/ctl
```

### B. State Tag Quick Reference

| Tag              | Mask                    | Meaning                           |
|:-----------------|:------------------------|:----------------------------------|
| `SCHED_READY`    | `0x00000001`            | Runnable, awaiting CPU            |
| `SCHED_RUNNING`  | `0x00000002`            | Currently executing               |
| `SCHED_SLEEPING` | `0x00000004`            | Blocked, not runnable             |
| `WAIT_FS`        | `0x00000100`            | Blocked on filesystem I/O         |
| `WAIT_PIPE`      | `0x00000200`            | Blocked on pipe                   |
| `WAIT_TIMER`     | `0x00000400`            | Blocked on timer                  |
| `WAIT_IPC`       | `0x00000800`            | Blocked on IPC message            |
| `WAIT_CHILD`     | `0x00001000`            | Blocked on child exit             |
| `WAIT_LOCK`      | `0x00002000`            | Blocked on mutex/spinlock         |
| `WAIT_TERMINAL`  | `0x00004000`            | Blocked on terminal input         |
| `TRACED`         | `0x00010000`            | Under debugger inspection         |
| `STOPPED`        | `0x00020000`            | Suspended (suspend signal)        |
| `ZOMBIE`         | `0x01000000`            | Exited, waiting for reaper        |
| `IDLE`           | `0x02000000`            | Kernel idle process               |

### C. User Management Tool Command Summary

| Command      | Syntax                                | Description                         |
|:-------------|:--------------------------------------|:------------------------------------|
| `usermod`    | `usermod [-a|-d] -g GNAME USER`       | Modify user account                 |
| `groupmod`   | `groupmod [-a|-d] USER GNAME`         | Modify group                        |
| `passwd`     | `passwd [USER]`                       | Change password                     |
| `cu`         | `cu USER`                             | Cancel user login                   |
| `who`        | `who`                                 | Show active sessions                |
| `userlog`    | `userlog [USER]`                      | Show login history                  |
| `quell`      | `quell CMD [ARGS...]`                 | Execute privileged command          |

### D. Revision History

| Version | Date       | Changes                                                     |
|:--------|:-----------|:------------------------------------------------------------|
| v1.0    | 2026-06-01 | Initial 4P1 standard (memory management, basic syscalls)    |
| v1.1    | 2026-07-16 | Added Chapter 4: Process Management                         |
| v1.2    | 2026-07-17 | Added Chapter 6: User & Identity Model; expanded Chapter 3  |
|         |            | (Memory Management details) and Chapter 4 (waiting reasons) |

---

*End of 4P1 Standard Specification v1.2*