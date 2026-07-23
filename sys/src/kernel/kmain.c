/*
 * M4KK1 4P1 - kmain.c
 * Description: Main kernel entry point and initialization.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "kernel.h"
#include "multiboot.h"
#include "console.h"
#include "memory.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "process.h"
#include "m4k_syscall.h"
#include "syscall.h"
#include "ldso.h"
#include "elf.h"
#include "sessions.h"
#include "device_tree.h"
#include "vfs.h"
#include <yafs.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int yafs_dev_read(u64 lba, void *buf);
int yafs_dev_write(u64 lba, const void *buf);

void m4k_syscall_init(void);
void mkrn_shell_main(void);

extern unsigned char init_init_elf[];
extern unsigned int init_init_elf_len;
extern unsigned char login_init_elf[];
extern unsigned int login_init_elf_len;
extern unsigned char m4sh_init_elf[];
extern unsigned int m4sh_init_elf_len;

void mkrn_yafs_test(void);
int mkrn_yafs_create_fhs(u64 *root_lba);
struct yafs_superblock;
int mkrn_yafs_dev_read(u64 lba, void *buf);
int mkrn_yafs_dev_write(u64 lba, const void *buf);

static mkrn_info_t kernel_info = {
    .magic = M4K_MAGIC,
    .version = (M4K_VERSION_MAJOR << 16)
               | (M4K_VERSION_MINOR << 8)
               | M4K_VERSION_PATCH,
    .build_date = M4K_BUILD_DATE,
    .build_time = M4K_BUILD_TIME,
    .uptime_seconds = 0,
    .process_count = 0,
    .memory_total = 0,
    .memory_free = 0,
    .memory_used = 0
};

void mkrn_main(multiboot_info_t *mb_info, u32 magic)
{
    mkrn_console_init();
    mkrn_console_write("=====================================\n");
    mkrn_console_write("    M4KK1 Y4KU Kernel Starting...\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("Bootloader Magic: 0x");
    mkrn_console_write_hex(magic);
    mkrn_console_write("\n");

    if (magic != M4K_MULTIBOOT_BOOTLOADER_MAGIC) {
        mkrn_console_write(
            "ERROR: Invalid bootloader magic! Halting.\n");
        mkrn_panic("Invalid bootloader magic");
    }

    mkrn_console_write("Kernel Version: ");
    mkrn_console_write_dec((kernel_info.version >> 16) & 0xFF);
    mkrn_console_write(".");
    mkrn_console_write_dec((kernel_info.version >> 8) & 0xFF);
    mkrn_console_write(".");
    mkrn_console_write_dec(kernel_info.version & 0xFF);
    mkrn_console_write("-");
    mkrn_console_write(M4K_VERSION_TYPE);
    mkrn_console_write("\n");
    mkrn_console_write("Build Date: ");
    mkrn_console_write(kernel_info.build_date);
    mkrn_console_write(" ");
    mkrn_console_write(kernel_info.build_time);
    mkrn_console_write("\n");

    mkrn_console_write("=====================================\n");
    mkrn_console_write("Initializing System Components...\n");
    mkrn_console_write("=====================================\n");

    mkrn_console_write("1. Initializing Memory Management...\n");
    if (mb_info == NULL) {
        mkrn_console_write("ERROR: Invalid multiboot info!\n");
        mkrn_panic("Invalid multiboot info");
    }

    mkrn_memory_init(mb_info);
    u32 total_mem = mkrn_memory_get_total();
    u32 free_mem = mkrn_memory_get_free();

    mkrn_console_write("   Memory management initialized.\n");
    mkrn_console_write("   Total memory: ");
    mkrn_console_write_dec(total_mem / 1024);
    mkrn_console_write(" KB\n");
    mkrn_console_write("   Free memory: ");
    mkrn_console_write_dec(free_mem / 1024);
    mkrn_console_write(" KB\n");

    mkrn_console_write("2. Initializing GDT...\n");
    mkrn_gdt_init();
    mkrn_console_write("   GDT initialized.\n");

    mkrn_console_write("3. Initializing IDT and Interrupts...\n");
    mkrn_idt_init();
    mkrn_console_write("   IDT initialized.\n");

    mkrn_console_write("4. Initializing Timer System...\n");
    mkrn_timer_init(1000);
    mkrn_console_write("   Timer initialized (1000 Hz).\n");

    mkrn_console_write("5. Initializing Process Management...\n");
    mkrn_process_init();
    mkrn_console_write("   Process management initialized.\n");

    mkrn_console_write("6. Initializing System Calls...\n");
    mkrn_syscall_init();
    m4k_syscall_init();
    mkrn_vfs_init();
    mkrn_procfs_init();
    mkrn_sessions_init();
    mkrn_device_tree_init();
    mkrn_console_write(
        "   Standard and M4KK1 system calls initialized.\n");

    mkrn_console_write("7. Initializing Dynamic Linker...\n");
    if (mkrn_ll_init() != 0) {
        mkrn_console_write(
            "   WARNING: Dynamic linker initialization failed!\n");
        mkrn_console_write(
            "   Some features may not be available.\n");
    } else {
        mkrn_console_write("   Dynamic linker initialized.\n");
    }

    mkrn_console_write("=====================================\n");
    mkrn_console_write("System Initialization Complete!\n");
    mkrn_console_write("=====================================\n");

    total_mem = mkrn_memory_get_total();
    free_mem = mkrn_memory_get_free();
    u32 used_mem = total_mem - free_mem;

    mkrn_console_write("Final Memory Status:\n");
    mkrn_console_write("   Total: ");
    mkrn_console_write_dec(total_mem / 1024);
    mkrn_console_write(" KB\n");
    mkrn_console_write("   Used:  ");
    mkrn_console_write_dec(used_mem / 1024);
    mkrn_console_write(" KB\n");
    mkrn_console_write("   Free:  ");
    mkrn_console_write_dec(free_mem / 1024);
    mkrn_console_write(" KB\n");

    mkrn_console_write("8. Initializing YAFS filesystem...\n");
    mkrn_yafs_test();

    static u8 sb_buf[4096];
    mkrn_yafs_dev_read(0, sb_buf);
    struct yafs_superblock *yafs_sb =
        (struct yafs_superblock *)sb_buf;
    u64 yafs_root = yafs_sb->root_tree_addr;
    mkrn_console_write("   YAFS root tree LBA: 0x");
    mkrn_console_write_hex(yafs_root);
    mkrn_console_write("\n");

    mkrn_console_write(
        "   Creating FHS directory tree (yafs_root=0x");
    mkrn_console_write_hex(yafs_root);
    mkrn_console_write(")...\n");
    if (mkrn_yafs_create_fhs(&yafs_root) == 0) {
        mkrn_console_write(
            "   FHS directory tree created.\n");

        yafs_sb->root_tree_addr = yafs_root;
        mkrn_yafs_dev_write(0, sb_buf);
        mkrn_yafs_dev_write(1, sb_buf);

        mkrn_vfs_set_root_yafs(NULL, yafs_root);
        mkrn_vfs_mount("/dev/yafs0", "/", "yafs");
        mkrn_console_write(
            "   YAFS mounted as root filesystem.\n");

        mkrn_console_write(
            "   Root dir listing (kernel):\n");
        static struct mkrn_vfs_dirent root_entries[40];
        int n_entries = mkrn_yafs_readdir(
            yafs_root, 1, root_entries, 40);
        for (int i = 0; i < n_entries; i++) {
            mkrn_console_write("     ");
            if (root_entries[i].type == 2)
                mkrn_console_write("D ");
            else
                mkrn_console_write("F ");
            mkrn_console_write(root_entries[i].name);
            mkrn_console_write("\n");
        }
        mkrn_console_write("   (");
        mkrn_console_write_dec(n_entries);
        mkrn_console_write(" entries)\n");
    } else {
        mkrn_console_write(
            "   WARNING: FHS tree creation failed, using "
            "ramfs fallback.\n");
    }

    mkrn_console_write("9. Writing userspace ELFs and user DB to YAFS...\n");
    {
        int fd = mkrn_vfs_open("/bin/login",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, login_init_elf,
                login_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/login written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/login\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/m4sh",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, m4sh_init_elf,
                m4sh_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/m4sh written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/m4sh\n");
        }
    }
    {
        mkrn_vfs_create_file_yafs("/export/home/testuser");
    }
    {
        static const char pw_root[]   = "root:0:0:/export/root:/bin/m4sh:System Administrator:\n";
        static const char pw_test[]   = "testuser:1001:1001:/home/testuser:/bin/m4sh:Test User:\n";
        static const char pw_nobody[] = "nobody:65534:65534:/export/srv/nobody:/sbin/nologin:Unprivileged:\n";
        int fd = mkrn_vfs_open("/export/cfg/passwd.db",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            mkrn_vfs_write(fd, pw_root, sizeof(pw_root) - 1);
            mkrn_vfs_write(fd, pw_test, sizeof(pw_test) - 1);
            mkrn_vfs_write(fd, pw_nobody, sizeof(pw_nobody) - 1);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /export/cfg/passwd.db created\n");
        } else {
            mkrn_console_write("   WARNING: failed to create passwd.db\n");
        }
    }
    {
        static const char gr_prime[] = "prime:1001:testuser,root\n";
        int fd = mkrn_vfs_open("/export/cfg/groups.db",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            mkrn_vfs_write(fd, gr_prime, sizeof(gr_prime) - 1);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /export/cfg/groups.db created\n");
        } else {
            mkrn_console_write("   WARNING: failed to create groups.db\n");
        }
    }
    {
        static const char tz_default[] = "+0000\n";
        int fd = mkrn_vfs_open("/export/cfg/timezone",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            mkrn_vfs_write(fd, tz_default, sizeof(tz_default) - 1);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /export/cfg/timezone created\n");
        } else {
            mkrn_console_write("   WARNING: failed to create timezone\n");
        }
    }

    mkrn_console_write("10. Loading init...\n");
    int exec_ret = mkrn_execve(
        init_init_elf, init_init_elf_len, "init");
    if (exec_ret != 0) {
        mkrn_console_write(
            "   ERROR: Failed to load init (err=");
        mkrn_console_write_dec((u32)(-exec_ret));
        mkrn_console_write(")\n");
        mkrn_console_write(
            "   Entering rescue shell...\n");
        mkrn_console_write(
            "=====================================\n");
        mkrn_console_write(" =( Sadly... \n");
        mkrn_shell_main();
        __builtin_unreachable();
    }

    mkrn_console_write("   Init loaded.\n");
    mkrn_console_write("=====================================\n");
    mkrn_console_write("Starting Scheduler...\n");
    mkrn_console_write("=====================================\n");
    mkrn_console_write("Switching to init process...\n");

    mkrn_process_switch_first();
    __builtin_unreachable();
}

void mkrn_panic(const char *message)
{
    mkrn_console_write("\nKERNEL PANIC: ");
    mkrn_console_write(message);
    mkrn_console_write("\n");

    M4K_CLI();

    while (1) {
        M4K_HLT();
    }
}

void mkrn_assert_failed(
    const char *file, int line, const char *expression)
{
    mkrn_console_write("\nAssertion failed: ");
    mkrn_console_write(expression);
    mkrn_console_write(" at ");
    mkrn_console_write(file);
    mkrn_console_write(":");
    mkrn_console_write_dec(line);
    mkrn_console_write("\n");

    mkrn_panic("Assertion failed");
}

void stack_overflow_handler(void)
{
    mkrn_panic("Kernel stack overflow");
}

void divide_by_zero_handler(void)
{
    mkrn_panic("Divide by zero error");
}

void invalid_opcode_handler(void)
{
    mkrn_panic("Invalid opcode");
}

void page_fault_handler(u32 address, u32 error_code)
{
    mkrn_console_write("\nPage fault at address: 0x");
    mkrn_console_write_hex(address);
    mkrn_console_write(", error code: 0x");
    mkrn_console_write_hex(error_code);
    mkrn_console_write("\n");

    mkrn_panic("Page fault");
}

void general_protection_fault_handler(u32 error_code)
{
    mkrn_console_write(
        "\nGeneral protection fault, error code: 0x");
    mkrn_console_write_hex(error_code);
    mkrn_console_write("\n");

    mkrn_panic("General protection fault");
}

void mkrn_debug_dump(void)
{
    mkrn_console_write("\n--- Kernel Debug Info ---\n");
    mkrn_console_write("Kernel magic: 0x");
    mkrn_console_write_hex(kernel_info.magic);
    mkrn_console_write("\n");

    mkrn_console_write("Version: ");
    mkrn_console_write_dec((kernel_info.version >> 16) & 0xFF);
    mkrn_console_write(".");
    mkrn_console_write_dec((kernel_info.version >> 8) & 0xFF);
    mkrn_console_write(".");
    mkrn_console_write_dec(kernel_info.version & 0xFF);
    mkrn_console_write("-");
    mkrn_console_write(M4K_VERSION_TYPE);
    mkrn_console_write("\n");

    u32 total = mkrn_memory_get_total();
    u32 free = mkrn_memory_get_free();
    u32 used = total - free;

    mkrn_console_write("Memory: ");
    mkrn_console_write_dec(total / 1024);
    mkrn_console_write(" KB total, ");
    mkrn_console_write_dec(used / 1024);
    mkrn_console_write(" KB used, ");
    mkrn_console_write_dec(free / 1024);
    mkrn_console_write(" KB free\n");

    mkrn_console_write("--- End Debug Info ---\n");
}
