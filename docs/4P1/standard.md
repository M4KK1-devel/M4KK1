# M4KK1 4P1 Standard Specification
**Version**: v1.1
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
5. Libraries
6. Appendices & Examples

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

### System Calls

| Syscall                                          | Number            | Description                              |
|:-------------------------------------------------|:------------------|:-----------------------------------------|
| `m4k_brk(void *addr)`                            | `M4K_SYS_BRK`     | Set program break; return new break addr |
| `m4k_map_memory(void *addr, size_t len, int prot, int flags, int fd, off_t off)` | `M4K_SYS_MMAP`  | Map memory region                        |
| `m4k_unmap_memory(void *addr, size_t len)`       | `M4K_SYS_MUNMAP`  | Unmap memory region                      |

### Protection Flags

```c
#define M4K_PROT_NONE   0x00
#define M4K_PROT_READ   0x01
#define M4K_PROT_WRITE  0x02
#define M4K_PROT_EXEC   0x04
```

### Mapping Flags

```c
#define M4K_MAP_PRIVATE     0x01
#define M4K_MAP_SHARED      0x02
#define M4K_MAP_ANONYMOUS   0x04
#define M4K_MAP_FIXED       0x08
#define M4K_MAP_GROWSDOWN   0x10
```

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

/* Debug / control tags */
#define M4K_TRACED          0x00010000ULL  /* Under ptrace-like inspection */
#define M4K_STOPPED         0x00020000ULL  /* Suspended by ctl or signal */

/* Lifecycle tags */
#define M4K_ZOMBIE          0x01000000ULL  /* Exited, not yet reaped */
#define M4K_IDLE            0x02000000ULL  /* Kernel idle process */

/* Convenience masks */
#define M4K_STATE_SCHED_MASK   0x00000007ULL  /* SCHED_READY|RUNNING|SLEEPING */
#define M4K_STATE_WAIT_MASK    0x00001F00ULL  /* All WAIT_* bits */
#define M4K_STATE_CTRL_MASK    0x00030000ULL  /* TRACED|STOPPED */
#define M4K_STATE_LIFE_MASK    0x03000000ULL  /* ZOMBIE|IDLE */
#define M4K_STATE_RESERVED     (~0x03031F07ULL) /* All reserved bits — writes rejected */
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
| `SCHED_RUNNING` + `TRACED`                         | Valid      |
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

**Behavior**:

1. Kernel reads the executable from `path` via VFS.
2. Validates the ELF header (magic, class, endianness, machine type).
3. Allocates a fresh `proc` struct, new address space, new PID.
4. Loads segments into memory per ELF program headers.
5. Sets up initial stack with `argv` and `envp`.
6. Applies spawn flags (namespace, FD, environment isolation).
7. Sets initial state: `[SCHED_READY]`.
8. Returns child PID to parent. The child starts executing when
   scheduled.

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

**`inherit_mask` Semantics**:

The child starts with **zeroed state tags** and then sets exactly
the tags specified in `inherit_mask` (from the parent's current state).
Tags not in `inherit_mask` are **not inherited**, even if set in the
parent. This allows callers to:

- Clone a process but strip debug flags:
  `m4k_fork_status(~M4K_TRACED & 0xFFFFFFFF, RFPROC | RFFDG | RFMEM);`

- Create a process born in a "timed wait" state:
  `m4k_fork_status(M4K_SCHED_SLEEPING | M4K_WAIT_TIMER, RFPROC);`

- Create a thread (shared address space, no special state):
  `m4k_fork_status(0, RFPROC | RFMEM | RFFDG);`

**Validations**:

| Inherit Mask Violation                  | Error           |
|:----------------------------------------|:----------------|
| Inheriting `M4K_ZOMBIE`                 | `-M4K_EINVAL`   |
| Inheriting `M4K_IDLE`                   | `-M4K_EINVAL`   |
| Inheriting reserved bits                | `-M4K_EINVAL`   |
| Calling without `RFPROC`                | `-M4K_EINVAL`   |
| `RFNAMEG` set by unprivileged caller    | `-M4K_EPERM`    |

#### 4.3.3 Comparison: `m4k_spawn` vs `m4k_fork_status`

| Aspect              | `m4k_spawn`                            | `m4k_fork_status`                          |
|:--------------------|:---------------------------------------|:-------------------------------------------|
| Address space       | Fresh (loads ELF)                      | Copied or shared (`RFMEM`)                 |
| State inheritance   | Always `[SCHED_READY]`                 | Selective via `inherit_mask`               |
| Use case            | Starting a new program                 | Cloning a process, creating threads        |
| Equivalent POSIX    | `fork() + exec()`                      | `fork()` / `clone()`                       |
| Error on nonexec    | `-M4K_ENOENT`                          | N/A (no file load)                         |

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
State: [SCHED_SLEEPING, WAIT_PIPE]
Memory: 4096 KB (heap) + 64 KB (stack)
User: root
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

| Command                           | Description                                     |
|:----------------------------------|:------------------------------------------------|
| `kill <signal>`                   | Send signal (e.g., `kill 9`)                   |
| `suspend`                         | Set `M4K_STOPPED`, clear `SCHED_READY`         |
| `resume`                          | Clear `M4K_STOPPED`, set `SCHED_READY`         |
| `niceness <value>`                | Set scheduling priority                     |
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

#### 4.5.4 Control Flow Diagram

```
User Command (echo)
       |
       v
  echo "kill 9" > /sys/proc/1234/ctl
       |
       v
  VFS write() -> /sys/proc pseudo-fs handler
       |
       v
  Kernel: resolve 1234 -> struct proc *
       |
       v
  process_control(proc, "kill 9") -> parse command -> kill(proc, 9)
       |
       v
  Kernel: set M4K_SCHED_READY on target
       |       (if not already dying)
       v
  scheduler() -> dispatches signal delivery
```

#### 4.5.5 Permissions

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
#define M4K_SYS_EXIT        0xM4K00001
#define M4K_SYS_SPAWN       0xM4K00002
#define M4K_SYS_WAIT        0xM4K00003
#define M4K_SYS_GETPID      0xM4K00004
#define M4K_SYS_KILL        0xM4K00005
#define M4K_SYS_GETPPID     0xM4K00006
#define M4K_SYS_FORK_ST     0xM4K00007
#define M4K_SYS_SETNS       0xM4K00008
```

#### 4.7.3 Error Codes (Process Subsystem)

```c
#define M4K_EACCES      0xM4K00001  /* Permission denied            */
#define M4K_ENOENT      0xM4K00002  /* No such file or process      */
#define M4K_EINVAL      0xM4K00004  /* Invalid argument             */
#define M4K_ENOMEM      0xM4K00005  /* Out of memory                */
#define M4K_EEXEC       0xM4K00006  /* Exec format error            */
#define M4K_EMFILE      0xM4K00007  /* Too many processes           */
#define M4K_ECHILD      0xM4K00008  /* No child process             */
#define M4K_EPERM       0xM4K00009  /* Operation not permitted      */
#define M4K_ESRCH       0xM4K0000A  /* No such process              */
#define M4K_EBUSY       0xM4K0000B  /* Resource busy (e.g., signal pending) */
```

---

## 5. Libraries

*This section is reserved for the M4KK1 Standard Library specification.
Refer to `docs/api/README.md` for the current API reference.*

---

## 6. Appendices & Examples

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

#### A.6 Forcing a Tag at Creation

```c
// Child born in "sleeping on timer" state
pid_t child = m4k_fork_status(
    M4K_SCHED_SLEEPING | M4K_WAIT_TIMER,
    RFPROC | RFFDG
);
```

#### A.7 Dynamic Namespace — Private /tmp

```c
// Point our own /tmp to a private directory
m4k_setns("/tmp", "/home/user/.cache/m4kk1/private_tmp",
          M4K_MNT_PRIVATE | M4K_MNT_BIND);
```

#### A.8 Sending Signals via ctl

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
| `TRACED`         | `0x00010000`            | Under debugger inspection         |
| `STOPPED`        | `0x00020000`            | Suspended (suspend signal)        |
| `ZOMBIE`         | `0x01000000`            | Exited, waiting for reaper        |
| `IDLE`           | `0x02000000`            | Kernel idle process               |

### C. Revision History

| Version | Date       | Changes                                                     |
|:--------|:-----------|:------------------------------------------------------------|
| v1.0    | 2026-06-01 | Initial 4P1 standard (memory management, basic syscalls)    |
| v1.1    | 2026-07-16 | Added Chapter 4: Process Management with state tag sets,    |
|         |            | `m4k_fork_status`, per-process namespaces, `/sys/proc` fs   |

---

*End of 4P1 Standard Specification v1.1*
