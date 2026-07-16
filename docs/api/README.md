# M4KK1 4P1 API Reference Manual
**Version**: 1.0
**Standard**: 4P1 (Leet of API)
**Scope**: All user-space programs running on M4KK1 kernel

---

## 1. Overview

M4KK1 operating system follows the **4P1 Standard**, providing an
independent, lean, and efficient system interface.

**Core design principles**:
- **Clarity over compatibility**: No POSIX baggage.
- **Simplicity over bloat**: Few interfaces, deterministic behavior,
  easy to understand and implement.
- **Native over emulation**: All tools and libraries built on the
  native M4KK1 ABI.

**ABI core identifiers**:
- Syscall interrupt: `int 0x4D` (ASCII 'M')
- Syscall number namespace: `M4K_SYS_*`
- Error code namespace: `M4K_E*`, range `0xM4Kxxxxx`

---

## 2. Syscall Interface (Kernel ABI)

All syscalls are issued via `int 0x4D`. Parameter passing follows the
**x86_64 System V** register order:

- Call number  -> `%rax`
- Arg1         -> `%rdi`
- Arg2         -> `%rsi`
- Arg3         -> `%rdx`
- Arg4         -> `%rcx`
- Arg5         -> `%r8`
- Arg6         -> `%r9`
- Return value -> `%rax` (negative means error)

> **Note**: Error codes are negative; the absolute value identifies
> the error type (see Section 5).

---

### 2.1 Process Management

M4KK1 has **no `fork()`**. It uses **explicit process creation
(Spawn)**.

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_exit(int status)` | `M4K_SYS_EXIT (0xM4K00001)` | Terminate current process, return status to parent. |
| `m4k_spawn(const char *path, char *const argv[], char *const envp[])` | `M4K_SYS_SPAWN (0xM4K00002)` | Create a new process loading an M4LL executable. Returns PID, or negative on error. |
| `m4k_waitpid(int pid, int *status, int options)` | `M4K_SYS_WAIT (0xM4K00003)` | Wait for a child process. Options supports `M4K_WNOHANG`. |
| `m4k_getpid(void)` | `M4K_SYS_GETPID (0xM4K00004)` | Return the current PID. |
| `m4k_kill(int pid, int sig)` | `M4K_SYS_KILL (0xM4K00005)` | Send a signal to a process (see Section 6). |

> `m4k_spawn` replaces the traditional `fork() + exec()` pattern by
> loading the program directly.

---

### 2.2 File I/O

File operations use file descriptors (non-negative integers).

Standard descriptors:
- `0` -- standard input
- `1` -- standard output
- `2` -- standard error

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_open(const char *path, int flags, int mode)` | `M4K_SYS_OPEN (0xM4K00010)` | Open a file. Returns fd on success. |
| `m4k_close(int fd)` | `M4K_SYS_CLOSE (0xM4K00011)` | Close a file descriptor. Returns 0 on success. |
| `m4k_read(int fd, void *buf, size_t count)` | `M4K_SYS_READ (0xM4K00012)` | Read data. Returns bytes read. |
| `m4k_write(int fd, const void *buf, size_t count)` | `M4K_SYS_WRITE (0xM4K00013)` | Write data. Returns bytes written. |
| `m4k_lseek(int fd, off_t offset, int whence)` | `M4K_SYS_LSEEK (0xM4K00014)` | Seek. whence: `M4K_SEEK_SET/CUR/END`. |
| `m4k_unlink(const char *path)` | `M4K_SYS_UNLINK (0xM4K00015)` | Delete a file (decrement link count). |
| `m4k_mkdir(const char *path, int mode)` | `M4K_SYS_MKDIR (0xM4K00016)` | Create a directory. |
| `m4k_rmdir(const char *path)` | `M4K_SYS_RMDIR (0xM4K00017)` | Remove an empty directory. |
| `m4k_stat(const char *path, struct m4k_stat *buf)` | `M4K_SYS_STAT (0xM4K00018)` | Get file metadata (see 2.2.2). |

#### 2.2.1 Open Flags (`flags`)

```c
#define M4K_O_RDONLY    0x0001
#define M4K_O_WRONLY    0x0002
#define M4K_O_RDWR      0x0004
#define M4K_O_CREAT     0x0100
#define M4K_O_EXCL      0x0200
#define M4K_O_TRUNC     0x1000
#define M4K_O_APPEND    0x2000
#define M4K_O_NONBLOCK  0x4000
```

#### 2.2.2 File Status Structure (`struct m4k_stat`)

```c
struct m4k_stat {
    uint32_t st_dev;      /* device ID */
    uint32_t st_ino;      /* inode number */
    uint16_t st_mode;     /* file type and permissions */
    uint32_t st_nlink;    /* hard link count */
    uint32_t st_uid;      /* user ID */
    uint32_t st_gid;      /* group ID */
    uint64_t st_size;     /* file size (bytes) */
    uint64_t st_atime;    /* last access time */
    uint64_t st_mtime;    /* last modification time */
    uint64_t st_ctime;    /* status change time */
};
```

---

### 2.3 Memory Management

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_brk(void *addr)` | `M4K_SYS_BRK (0xM4K00020)` | Set program break. Returns the new break address. |
| `m4k_map_memory(void *addr, size_t length, int prot, int flags, int fd, off_t offset)` | `M4K_SYS_MMAP (0xM4K00021)` | Map memory. Returns the mapped address. |
| `m4k_unmap_memory(void *addr, size_t length)` | `M4K_SYS_MUNMAP (0xM4K00022)` | Unmap memory. |

**Protection flags (`prot`)**:
```c
#define M4K_PROT_NONE   0x00
#define M4K_PROT_READ   0x01
#define M4K_PROT_WRITE  0x02
#define M4K_PROT_EXEC   0x04
```

**Mapping flags (`flags`)**:
```c
#define M4K_MAP_SHARED      0x01
#define M4K_MAP_PRIVATE     0x02
#define M4K_MAP_FIXED       0x10
#define M4K_MAP_ANONYMOUS   0x20
```

---

### 2.4 Directory Operations

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_opendir(const char *path)` | `M4K_SYS_OPENDIR (0xM4K00030)` | Open a directory. Returns a `M4K_DIR*`. |
| `m4k_readdir(M4K_DIR *dirp)` | `M4K_SYS_READDIR (0xM4K00031)` | Read a directory entry. Returns `struct m4k_dirent*`. |
| `m4k_closedir(M4K_DIR *dirp)` | `M4K_SYS_CLOSEDIR (0xM4K00032)` | Close a directory stream. |

```c
struct m4k_dirent {
    uint32_t d_ino;      /* inode number */
    uint16_t d_reclen;   /* record length */
    uint8_t  d_type;     /* file type (DT_REG, DT_DIR, ...) */
    char     d_name[256];/* null-terminated filename */
};
```

---

### 2.5 Time and Timers

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_gettimeofday(struct m4k_timeval *tv)` | `M4K_SYS_GETTIME (0xM4K00040)` | Get current time (sec + usec). |
| `m4k_sleep(uint32_t seconds)` | `M4K_SYS_SLEEP (0xM4K00041)` | Sleep for the given number of seconds. |

```c
struct m4k_timeval {
    uint64_t tv_sec;    /* seconds */
    uint32_t tv_usec;   /* microseconds */
};
```

---

### 2.6 Device Control

| Syscall | Number | Description |
| :--- | :--- | :--- |
| `m4k_ioctl(int fd, int request, void *argp)` | `M4K_SYS_IOCTL (0xM4K00050)` | Device control. Request codes are device-specific. |

---

## 3. Library Functions (4P1 Standard Library)

The standard library provides utility functions built on top of
syscalls. All functions reside in `libm4k.a`; link with `-lm4k`.

### 3.1 String Functions

```c
size_t m4k_strlen(const char *s);
char *m4k_strcpy(char *dest, const char *src);
int m4k_strcmp(const char *s1, const char *s2);
char *m4k_strcat(char *dest, const char *src);
char *m4k_strchr(const char *s, int c);
char *m4k_strstr(const char *haystack, const char *needle);
```

### 3.2 Memory Functions

```c
void *m4k_memcpy(void *dest, const void *src, size_t n);
void *m4k_memset(void *s, int c, size_t n);
int m4k_memcmp(const void *s1, const void *s2, size_t n);
void *m4k_memmove(void *dest, const void *src, size_t n);
```

### 3.3 Formatted I/O (fd-based)

```c
int m4k_printf(const char *format, ...);        /* stdout */
int m4k_fprintf(int fd, const char *format, ...);
int m4k_sprintf(char *str, const char *format, ...);
int m4k_vprintf(const char *format, va_list ap);

int m4k_scanf(const char *format, ...);
int m4k_fscanf(int fd, const char *format, ...);
int m4k_sscanf(const char *str, const char *format, ...);
```

> Supported format specifiers: `%d`, `%u`, `%x`, `%s`, `%c`, `%p`.
> Not supported: `%f` (float), `%n`.

### 3.4 Memory Allocation (backed by `m4k_brk`)

```c
void *m4k_malloc(size_t size);
void *m4k_calloc(size_t nmemb, size_t size);
void *m4k_realloc(void *ptr, size_t size);
void m4k_free(void *ptr);
```

---

## 4. Process Model and Signals

**Process model**:
- Processes are created via `m4k_spawn`. Each has an independent
  address space.
- Inter-process communication uses **event queues** (preferred over
  signals):
  - `m4k_send_event(pid, event_code, data)` -- send an event.
  - `m4k_recv_event(event_code, data)` -- block and receive an event.

**Signals**: A minimal set retained for porting legacy programs, but
the event mechanism is recommended for new code.

| Signal | Number | Purpose |
| :--- | :--- | :--- |
| `M4K_SIGTERM` | 1 | Terminate process |
| `M4K_SIGKILL` | 2 | Force kill (uncatchable) |
| `M4K_SIGUSR1` | 3 | User-defined 1 |
| `M4K_SIGUSR2` | 4 | User-defined 2 |

> **Recommendation**: New programs should use event queues instead of
> signals.

---

## 5. Error Codes

Error codes are negative; the absolute value is defined below. All
errors use the `M4K_E` prefix.

| Error | Value | Description |
| :--- | :--- | :--- |
| `M4K_EPERM`   | `0xM4K10001` | Permission denied |
| `M4K_ENOENT`  | `0xM4K20001` | No such file or directory |
| `M4K_EINTR`   | `0xM4K00001` | Interrupted |
| `M4K_EIO`     | `0xM4K00002` | I/O error |
| `M4K_ENOMEM`  | `0xM4K30001` | Out of memory |
| `M4K_EACCES`  | `0xM4K10002` | Access denied |
| `M4K_EFAULT`  | `0xM4K00003` | Bad address |
| `M4K_EBUSY`   | `0xM4K00004` | Device or resource busy |
| `M4K_EEXIST`  | `0xM4K20002` | File exists |
| `M4K_ENODEV`  | `0xM4K20003` | No such device |
| `M4K_ENOTDIR` | `0xM4K20004` | Not a directory |
| `M4K_EISDIR`  | `0xM4K20005` | Is a directory |
| `M4K_EINVAL`  | `0xM4K00005` | Invalid argument |
| `M4K_ENFILE`  | `0xM4K20006` | Too many open files (system) |
| `M4K_EMFILE`  | `0xM4K20007` | Too many open files (process) |
| `M4K_ENOSPC`  | `0xM4K20008` | No space left on device |
| `M4K_ESPIPE`  | `0xM4K00006` | Illegal seek |
| `M4K_EROFS`   | `0xM4K20009` | Read-only filesystem |
| `M4K_ETIMEOUT`| `0xM4K00007` | Timeout |

---

## 6. Compilation and Linking

Use the native M4KK1 toolchain (M4LL format):

```bash
# Compile C to object file
m4k-gcc -c hello.c -o hello.o

# Link static executable
m4k-ld -static hello.o -o hello -lm4k

# Or link via gcc directly
m4k-gcc hello.o -o hello -lm4k
```

**Standard library**: `libm4k.a` lives in `/usr/lib`; headers live in
`/usr/include/m4k/`.

**System header**: `#include <m4k/syscall.h>` for all syscall
declarations.

---

## 7. Examples

### Example 1: Hello World (raw syscalls)

```c
#include <m4k/syscall.h>

int main(void)
{
    const char *szMsg = "Hello, M4KK1!\n";
    m4k_write(1, szMsg, 14);
    m4k_exit(0);
}
```

### Example 2: File Copy (standard library)

```c
#include <m4k/syscall.h>
#include <m4k/stdio.h>
#include <m4k/stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        m4k_printf("Usage: %s <src> <dst>\n", argv[0]);
        return 1;
    }
    int fdSrc = m4k_open(argv[1], M4K_O_RDONLY, 0);
    if (fdSrc < 0) {
        m4k_printf("Cannot open %s\n", argv[1]);
        return 1;
    }
    int fdDst = m4k_open(argv[2],
        M4K_O_WRONLY | M4K_O_CREAT | M4K_O_TRUNC, 0644);
    if (fdDst < 0) {
        m4k_printf("Cannot create %s\n", argv[2]);
        m4k_close(fdSrc);
        return 1;
    }
    char buf[4096];
    ssize_t n;
    while ((n = m4k_read(fdSrc, buf, sizeof(buf))) > 0) {
        m4k_write(fdDst, buf, n);
    }
    m4k_close(fdSrc);
    m4k_close(fdDst);
    return 0;
}
```

### Example 3: Process Spawn and Wait

```c
#include <m4k/syscall.h>

int main(void)
{
    int pid = m4k_spawn("/bin/hello", NULL, NULL);
    if (pid < 0) {
        m4k_write(2, "spawn failed\n", 13);
        return 1;
    }
    int status;
    m4k_waitpid(pid, &status, 0);
    m4k_printf("Child %d exited with status %d\n", pid, status);
    return 0;
}
```

---

## 8. Debugging and Tracing

M4KK1 provides lightweight debugging tools:

- **`m4k_strace`** -- print syscall trace of a process.
- **`m4k_ltrace`** -- print library call trace (via
  `-finstrument-functions`).
- **`m4k_gdb`** -- symbolic debugger (breakpoints, single-step,
  memory inspection).

Enable debugging:
- Compile with `-g` for debug symbols.
- Set environment variable `M4K_DEBUG=1` at runtime for kernel-level
  tracing.

---

## 9. Best Practices

1. **Use 4P1 style**: Do not call POSIX functions (`open`, `read`,
   `fork`). Use the `m4k_` versions.
2. **Check errors**: Every syscall return value must be checked.
3. **Clean up resources**: Use `m4k_close`, `m4k_free` to release
   resources.
4. **Avoid signals**: New programs should use the event mechanism.
5. **Follow path conventions**: Use the 4P1 filesystem layout
   (`/bin`, `/sys`, `/dev`, etc.).
6. **Prefer static linking**: Minimize runtime dependencies.

---

## Appendix A: Syscall Quick Reference

| Syscall | Macro | Description |
| :--- | :--- | :--- |
| `m4k_exit` | `M4K_SYS_EXIT` | Exit process |
| `m4k_spawn` | `M4K_SYS_SPAWN` | Create process |
| `m4k_waitpid` | `M4K_SYS_WAIT` | Wait for child |
| `m4k_getpid` | `M4K_SYS_GETPID` | Get PID |
| `m4k_kill` | `M4K_SYS_KILL` | Send signal |
| `m4k_open` | `M4K_SYS_OPEN` | Open file |
| `m4k_close` | `M4K_SYS_CLOSE` | Close file |
| `m4k_read` | `M4K_SYS_READ` | Read from file |
| `m4k_write` | `M4K_SYS_WRITE` | Write to file |
| `m4k_lseek` | `M4K_SYS_LSEEK` | Seek |
| `m4k_unlink` | `M4K_SYS_UNLINK` | Delete file |
| `m4k_mkdir` | `M4K_SYS_MKDIR` | Create directory |
| `m4k_rmdir` | `M4K_SYS_RMDIR` | Remove directory |
| `m4k_stat` | `M4K_SYS_STAT` | Get file metadata |
| `m4k_brk` | `M4K_SYS_BRK` | Adjust heap |
| `m4k_map_memory` | `M4K_SYS_MMAP` | Memory map |
| `m4k_unmap_memory` | `M4K_SYS_MUNMAP` | Unmap memory |
| `m4k_opendir` | `M4K_SYS_OPENDIR` | Open directory |
| `m4k_readdir` | `M4K_SYS_READDIR` | Read directory entry |
| `m4k_closedir` | `M4K_SYS_CLOSEDIR` | Close directory |
| `m4k_gettimeofday` | `M4K_SYS_GETTIME` | Get time |
| `m4k_sleep` | `M4K_SYS_SLEEP` | Sleep |
| `m4k_ioctl` | `M4K_SYS_IOCTL` | Device control |

---

*This manual corresponds strictly to the M4KK1 kernel implementation.
In case of discrepancy, the kernel source is authoritative.*
