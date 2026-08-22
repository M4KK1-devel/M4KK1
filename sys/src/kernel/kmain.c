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
#include <video.h>
#include <mouse.h>
#include <keyboard.h>
#include <pci.h>
#include <ata.h>
#include <sb16.h>
#include <yafs.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int yafs_dev_read(u64 lba, void *buf);
int yafs_dev_write(u64 lba, const void *buf);

void m4k_syscall_init(void);
void mkrn_shell_main(void);

#ifndef M4K_MINIMAL
extern unsigned char init_init_elf[];
extern unsigned int init_init_elf_len;
extern unsigned char login_init_elf[];
extern unsigned int login_init_elf_len;
extern unsigned char m4sh_init_elf[];
extern unsigned int m4sh_init_elf_len;
extern unsigned char m4shg_init_elf[];
extern unsigned int m4shg_init_elf_len;
#ifdef M4K_FULL
extern unsigned char mdm_init_elf[];
extern unsigned int mdm_init_elf_len;
extern unsigned char mdm_mini_init_elf[];
extern unsigned int mdm_mini_init_elf_len;
extern unsigned char flip_test_init_elf[];
extern unsigned int flip_test_init_elf_len;
extern unsigned char copland_init_elf[];
extern unsigned int copland_init_elf_len;
extern unsigned char terminal_init_elf[];
extern unsigned int terminal_init_elf_len;
extern unsigned char cptest_init_elf[];
extern unsigned int cptest_init_elf_len;
extern unsigned char fm_init_elf[];
extern unsigned int fm_init_elf_len;
extern unsigned char altr_init_elf[];
extern unsigned int altr_init_elf_len;
extern unsigned char calc_init_elf[];
extern unsigned int calc_init_elf_len;
extern unsigned char sprach_stack_init_elf[];
extern unsigned int sprach_stack_init_elf_len;
extern unsigned char pcc_init_elf[];
extern unsigned int pcc_init_elf_len;
#endif
#ifdef M4K_RECOVERY
extern unsigned char fsck_init_elf[];
extern unsigned int fsck_init_elf_len;
extern unsigned char reset_passwd_init_elf[];
extern unsigned int reset_passwd_init_elf_len;
#endif
#endif

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

#ifdef M4K_MINIMAL
    /* Minimal build: boot banner only, then halt. No filesystem,
     * no drivers, no userspace. Used for boot smoke tests. */
    mkrn_console_write("=====================================\n");
    mkrn_console_write("[MINIMAL] System ready\n");
    mkrn_console_write("Minimal build - halting (no interaction).\n");
    mkrn_console_write("=====================================\n");
    for (;;) {
        M4K_HLT();
    }
#endif

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
    mkrn_tss_flush();
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
#ifdef M4K_FULL
    mkrn_vesa_init();
    /* Graphics test: draw a pattern on screen */
    mkrn_vesa_clear(VESA_COLOR_BLACK);
    mkrn_vesa_draw_rect(50, 50, 200, 150, VESA_COLOR_RED, 1);
    mkrn_vesa_draw_rect(50, 50, 200, 150, VESA_COLOR_WHITE, 0);
    mkrn_vesa_draw_rect(300, 50, 100, 100, VESA_COLOR_GREEN, 1);
    mkrn_vesa_draw_rect(450, 50, 100, 100, VESA_COLOR_BLUE, 1);
    mkrn_vesa_put_string(50, 250, "Hello M4KK1 GUI!", VESA_COLOR_WHITE, VESA_COLOR_BLACK);
    mkrn_vesa_put_string(50, 270, "Framebuffer 800x600x32", VESA_COLOR_YELLOW, VESA_COLOR_BLACK);
    mkrn_vesa_flip();
#endif

    mkrn_console_write(
        "   Standard and M4KK1 system calls initialized.\n");

    mkrn_console_write("6.3. Scanning PCI Bus...\n");
    int pci_count = mkrn_pci_scan_bus();
    if (pci_count > 0) {
        mkrn_console_write("   PCI bus scanned, ");
        mkrn_console_write_dec((u32)pci_count);
        mkrn_console_write(" device(s) detected.\n");
    } else {
        mkrn_console_write("   WARNING: No PCI devices found!\n");
    }

    mkrn_console_write("6.4. Initializing PS/2 Keyboard...\n");
    mkrn_kbd_init();
    if (mkrn_keyboard_is_initialized()) {
        mkrn_console_write("   PS/2 keyboard driver initialized.\n");
    } else {
        mkrn_console_write("   PS/2 keyboard not detected.\n");
    }

    mkrn_console_write("6.5. Initializing PS/2 Mouse...\n");
    mkrn_mouse_init();
    mkrn_idt_enable_interrupts();
    if (mkrn_mouse_is_initialized()) {
        mkrn_console_write("   PS/2 mouse driver initialized.\n");
#ifdef M4K_FULL
        mkrn_vesa_cursor_enable(1);
#endif
    } else
        mkrn_console_write("   PS/2 mouse not detected.\n");

    mkrn_console_write("6.6. Initializing ATA/IDE storage...\n");
    int ata_count = mkrn_ata_init();
    if (ata_count > 0) {
        mkrn_devfs_init();
        mkrn_console_write("   DevFS initialized (/dev/hda).\n");
    } else {
        mkrn_console_write("   No ATA drives detected.\n");
    }

    mkrn_console_write("6.7. Initializing SB16 audio...\n");
    if (mkrn_sb16_init() == 0) {
        mkrn_console_write("   Playing 440Hz boot beep...\n");
        if (mkrn_sb16_beep(440, 200) == 0)
            mkrn_console_write("   Boot beep complete.\n");
        else
            mkrn_console_write("   WARNING: boot beep failed.\n");
    } else {
        mkrn_console_write("   No SB16 audio device found.\n");
    }

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
#ifndef M4K_MINIMAL
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
        int fd = mkrn_vfs_open("/bin/m4shg",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, m4shg_init_elf,
                m4shg_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/m4shg written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/m4shg\n");
        }
    }
#endif /* M4K_MINIMAL */

#ifdef M4K_FULL
    {
        int fd = mkrn_vfs_open("/bin/mdm",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, mdm_init_elf,
                mdm_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/mdm written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/mdm\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/mdm_mini",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, mdm_mini_init_elf,
                mdm_mini_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/mdm_mini written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/mdm_mini\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/flip_test",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, flip_test_init_elf,
                flip_test_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/flip_test written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/flip_test\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/copland",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, copland_init_elf,
                copland_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/copland written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/copland\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/copland_status",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, copland_init_elf,
                copland_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/copland_status written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/copland_status\n");
        }
    }
    {
        /* Default window manager: Copland spawns /bin/sprach */
        int fd = mkrn_vfs_open("/bin/sprach",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, sprach_stack_init_elf,
                sprach_stack_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/sprach written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/sprach\n");
        }
    }
    {
        /* Terminal emulator: Sprach forks+execs this on Ctrl+Alt+T */
        int fd = mkrn_vfs_open("/bin/terminal",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, terminal_init_elf,
                terminal_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/terminal written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/terminal\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/cptest",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, cptest_init_elf,
                cptest_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/cptest written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/cptest\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/fm",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, fm_init_elf,
                fm_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/fm written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/fm\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/altr",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, altr_init_elf,
                altr_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/altr written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/altr\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/calcg",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, calc_init_elf,
                calc_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/calcg written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/calcg\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/sprach_stack",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, sprach_stack_init_elf,
                sprach_stack_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/sprach_stack written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/sprach_stack\n");
        }
    }
    {
        /* Self-hosted C compiler: installed as /bin/pcc and /bin/cc */
        int fd = mkrn_vfs_open("/bin/pcc",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, pcc_init_elf,
                pcc_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/pcc written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/pcc\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/cc",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, pcc_init_elf,
                pcc_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/cc written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/cc\n");
        }
    }
#endif /* M4K_FULL */

#ifdef M4K_RECOVERY
    {
        /* Recovery tools: filesystem check and password reset */
        int fd = mkrn_vfs_open("/bin/fsck",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, fsck_init_elf,
                fsck_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/fsck written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/fsck\n");
        }
    }
    {
        int fd = mkrn_vfs_open("/bin/reset-passwd",
            M4K_O_CREAT | M4K_O_WRONLY);
        if (fd >= 0) {
            int n = mkrn_vfs_write(fd, reset_passwd_init_elf,
                reset_passwd_init_elf_len);
            mkrn_vfs_close(fd);
            mkrn_console_write("   /bin/reset-passwd written (");
            mkrn_console_write_dec(n);
            mkrn_console_write(" bytes)\n");
        } else {
            mkrn_console_write("   WARNING: failed to create /bin/reset-passwd\n");
        }
    }
#endif /* M4K_RECOVERY */

    {
        mkrn_vfs_create_file_yafs("/export/home/testuser");
    }
    {
        /* Generate passwd.db with proper password hashes */
        /* Format: username:uid:gid:home:shell:gecos:password_hash */
        /* Password hash format: salt_hex + "$" + sha256_hash_hex */
        /* Using deterministic salt: salt[i] = i * 17 + 37 */
        /* root 哈希 = SHA-256("123456" + salt)，testuser = SHA-256("yakumakki" + salt)，
           与 musr_verify_password 的加盐验证算法一致 */
        static const char pw_root[]   = "root:0:0:/export/root:/bin/m4sh:System Administrator:25364758697a8b9cadbecfe0f1021324$5702872e7ea90f7450a2c8cb2e9d3aaf36a681a7f6ebc9905d21e4ef0093667b\n";
        static const char pw_test[]   = "testuser:1001:1001:/home/testuser:/bin/m4sh:Test User:25364758697a8b9cadbecfe0f1021324$b624c9a08f61ef9adb36f92d22b6f090d5d7cc3e9b8078873ba3ed3c70db3240\n";
        static const char pw_nobody[] = "nobody:65534:65534:/export/srv/nobody:/sbin/nologin:Unprivileged:25364758697a8b9cadbecfe0f1021324$0000000000000000000000000000000000000000000000000000000000000000\n";
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
#ifndef M4K_MINIMAL
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
#endif /* M4K_MINIMAL */
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
