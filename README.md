# M4KK1 Operating System

**Author:** makkichan947

**Repository:** https://github.com/makkichan947/M4KK1

**License:** GPL-3.0

**Documentation:**
- **[中文 (zh-CN)](docs/README.md)** — 完整的系统文档（25 个文件）
- **[English (en)](docs/en/README.md)** — 英文翻译文档（10 个文件）

M4KK1 is an operating system for i386, developed from scratch in freestanding C
and NASM assembly. It is an early-stage, experimental project — many things are
aspirational rather than implemented. This README describes what **actually** exists.

---

## Current State

### Kernel (Y4KU)

- **Architecture:** i386 (32-bit x86), GRUB-booted ELF with multiboot, protected mode
- **Language:** Freestanding C (`-m32 -nostdlib -nostdinc -ffreestanding`)
- **Design:** Macro-kernel (not microkernel)
- **CPU setup:** GDT, IDT, 1000 Hz PIT timer
- **Syscalls:** `int 0x80` path for compatibility, `int 0x4D` dispatch for the M4KK1
  syscall namespace (numbers use `0x4D00XXXX` format)
- **Process management:** `uint64_t` state_tag bitmask (SCHED_READY, RUNNING,
  SLEEPING, WAIT_* flags, ZOMBIE, etc.), `m4k_fork_status` selective state
  inheritance, `mkrn_waitpid` with zombie reaping
- **Signals:** 10-signal model (SIGABRT, SIGKILL, SIGTERM, SIGSTOP, SIGCONT,
  SIGTRAP, SIGCHLD, SIGPIPE, SIGUSR1, SIGUSR2)
- **Namespaces:** Per-process mount table (16 entries)
- **Pseudo-filesystem:** `/sys/proc` (status, cmdline, environ, cwd, ns, ctl per PID)
- **Filesystem:** YAFS — a log-structured COW B+Tree filesystem (basic: format,
  superblock, B+Tree insert/lookup/delete, free-space tracking, FHS directory
  creation)
- **VFS layer:** ramfs and YAFS mounting; ProcFS routing through VFS
- **Process listing:** `ps` via `getprocs` syscall
- **Console:** Serial I/O via COM1 at 38400 baud

### Userspace (M4SH)

- Single-process, Ring 0, freestanding C ELF loaded as init by the kernel
- Interactive shell with character-by-character echo and command-line editing (backspace, ANSI colour)
- 35+ built-in commands: ls, cat, cd, pwd, mkdir, rm, rmdir, mv, cp, ps, kill, nice,
  time, grep, wc, export, mount, umount, calc, sead, diff, blkid, cal, date, clear,
  uname, free, df, at, batch, help, exit, and more
- Environment variables, tab-completion (planned)
- No dynamic linking

### Build System

Build via `tools/build/build_krn.sh` — a Bash script that:

1. Compiles kernel C sources with `gcc -m32 -nostdlib -nostdinc -ffreestanding`
2. Assembles NASM entry/boot files (`nasm -f elf32`)
3. Links with `ld -m elf_i386` using `sys/src/init/linker.ld`
4. Builds the M4SH userspace shell ELF (`m4sh/m4sh.elf`)
5. Embeds the M4SH ELF as a C array (`init/init_elf.c`)
6. Produces a flat kernel binary (`m4kk1.krn`)
7. Creates a GRUB-bootable ISO via `grub-mkrescue` (`m4kk1-test.iso`)

There is no Makefile for the kernel build — the build script is the source of truth.

---

## Build & Run

### Prerequisites

- GCC (i386 cross-compiler or multilib-capable host GCC)
- NASM
- GNU ld (i386 support)
- grub-mkrescue (from GRUB utilities)
- QEMU (for testing)

### Build

```bash
git clone https://github.com/makkichan947/M4KK1.git
cd M4KK1
bash tools/build/build_krn.sh
```

### Test in QEMU

```bash
qemu-system-x86_64 -cdrom m4kk1-test.iso -serial stdio
```

(The `-cdrom` flag is used because the ISO is a CD image; the kernel itself is
32-bit i386, but QEMU's system profile is x86_64 for convenience.)

---

## Project Structure

```
M4KK1/
├── sys/src/                  # Kernel source tree
│   ├── kernel/               # Core kernel (kmain.c, process.c, execve.c,
│   │                         #   ldso.c, shell.c, syscall.c, syscall_m4k.c,
│   │                         #   signal.c, namespace.c)
│   ├── arch/m4kk1/           # Architecture-specific code (timer.c, gdt.c/asm,
│   │                         #   idt.c/asm)
│   ├── fs/                   # Filesystems (vfs.c, procfs.c, yafs/)
│   │   └── yafs/             # YAFS B+Tree COW filesystem
│   ├── mm/memory.c           # Simple physical page allocator
│   ├── drivers/console.c     # Serial console (COM1)
│   ├── lib/                  # Utility code (string.c, debug.c)
│   ├── init/                 # Linker script, entry.asm
│   └── include/              # Kernel headers
├── m4sh/                     # Userspace shell (init ELF)
├── init/                     # Generated init_elf.c (embedded M4SH binary)
├── tools/build/              # Build scripts
├── iso/                      # GRUB ISO staging directory
├── docs/                     # Design notes & full documentation (中文, 22 files)
│   ├── architecture.md       # 系统架构
│   ├── kernel/               # 内核子系统文档 (7 files)
│   ├── userspace/            # 用户空间文档 (5 files)
│   ├── fs/                   # 文件系统文档 (3 files)
│   ├── dev/                  # 开发指南文档 (4 files)
│   └── plan/roadmap.md       # 开发路线图
├── arch/                     # Architecture stubs (arm, arm64, powerpc,
│                             #   riscv, x86_64, x86)
├── usr/                      # (empty — future user programs)
├── pkg/                      # (pkgmgr stubs — not functional)
├── test/                     # Test framework stubs
└── scripts/                  # Helper scripts (build_iso.sh, install.sh)
```

---

## Limitations

- **i386 only:** No 64-bit support, no SMP, no APIC (legacy PIC only)
- **Single-process userspace:** M4SH runs as the sole user process at Ring 0
- **No MMU isolation:** No virtual memory, no page-level protection
- **No dynamic linking:** All code is statically linked
- **No networking stack**
- **No graphics** — serial console only
- **YAFS is basic:** format/read/write/delete work but the implementation is
  incomplete (no journaling, no crash recovery)
- **No block cache:** Direct disk I/O
- **No POSIX compatibility layer**
- **No multi-user support**
- **No real-time guarantees**

## Planned (near-term)

- Userspace Ring 3 transition (separate kernel/userspace privilege levels)
- Dynamic linker / shared library support
- ATA/PATA and AHCI disk drivers
- Block cache for filesystem performance
- Expanded /sys pseudo-filesystem
- More complete YAFS implementation
- Networking (NE2000 / RTL8139 driver, lwIP stack)
- Simple framebuffer console
