#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <vfs.h>
#include <kernel.h>

#define DT_MAX_FILES 32
#define DT_MAX_NAME  64
#define DT_MAX_DEVICES 16

typedef struct {
    const char *name;
    const char *type;
    uint32_t io_base;
    uint32_t irq;
    const char *desc;
    bool present;
} m4k_device_t;

static m4k_device_t dt_devices[DT_MAX_DEVICES];
static int dt_num_devices;

typedef struct {
    int fd;
    char name[DT_MAX_NAME];
    bool in_use;
    int device_idx;
    int read_offset;
} dt_file_t;

static dt_file_t dt_files[DT_MAX_FILES];
static int dt_next_fd = 3000;

void mkrn_device_tree_init(void)
{
    memset(dt_files, 0, sizeof(dt_files));
    memset(dt_devices, 0, sizeof(dt_devices));
    dt_num_devices = 0;

    m4k_device_t *d = &dt_devices[dt_num_devices++];
    d->name = "serial";
    d->type = "serial";
    d->io_base = 0x3F8;
    d->irq = 4;
    d->desc = "NS16550A-compatible serial port (COM1)";
    d->present = true;

    d = &dt_devices[dt_num_devices++];
    d->name = "rtc";
    d->type = "rtc";
    d->io_base = 0x70;
    d->irq = 8;
    d->desc = "MC146818 RTC/CMOS";
    d->present = true;

    d = &dt_devices[dt_num_devices++];
    d->name = "timer";
    d->type = "timer";
    d->io_base = 0x40;
    d->irq = 0;
    d->desc = "Intel 8253 PIT (Programmable Interval Timer)";
    d->present = true;

    d = &dt_devices[dt_num_devices++];
    d->name = "pic";
    d->type = "pic";
    d->io_base = 0x20;
    d->irq = 2;
    d->desc = "Intel 8259A PIC (Programmable Interrupt Controller)";
    d->present = true;

    d = &dt_devices[dt_num_devices++];
    d->name = "keyboard";
    d->type = "input";
    d->io_base = 0x60;
    d->irq = 1;
    d->desc = "i8042 PS/2 keyboard controller";
    d->present = false;

    mkrn_console_write("[INFO] Device tree initialized: ");
    mkrn_console_write_dec((uint32_t)dt_num_devices);
    mkrn_console_write(" devices\n");
}

static int dt_find_device_by_name(const char *name)
{
    for (int i = 0; i < dt_num_devices; i++) {
        if (strcmp(dt_devices[i].name, name) == 0)
            return i;
    }
    return -1;
}

static dt_file_t *dt_alloc_file(void)
{
    for (int i = 0; i < DT_MAX_FILES; i++) {
        if (!dt_files[i].in_use) {
            dt_files[i].in_use = true;
            dt_files[i].fd = dt_next_fd++;
            dt_files[i].read_offset = 0;
            dt_files[i].device_idx = -1;
            return &dt_files[i];
        }
    }
    return NULL;
}

static void dt_free_file(dt_file_t *f)
{
    f->in_use = false;
}

static dt_file_t *dt_find_by_fd(int fd)
{
    for (int i = 0; i < DT_MAX_FILES; i++) {
        if (dt_files[i].in_use && dt_files[i].fd == fd)
            return &dt_files[i];
    }
    return NULL;
}

int mkrn_device_tree_open(const char *path, int flags, int *out_fd)
{
    (void)flags;

    const char *p = path;
    while (*p == '/') p++;

    /* Handle /device or /device/ (directory listing) */
    if (strcmp(p, "device") == 0 || strcmp(p, "device/") == 0) {
        dt_file_t *f = dt_alloc_file();
        if (!f) return -1;
        strncpy(f->name, path, DT_MAX_NAME - 1);
        f->name[DT_MAX_NAME - 1] = '\0';
        f->device_idx = -1;
        *out_fd = f->fd;
        return 0;
    }

    /* Handle /device/<name> */
    if (strncmp(p, "device/", 7) == 0) {
        const char *dname = p + 7;
        int idx = dt_find_device_by_name(dname);
        if (idx < 0) return -1;
        dt_file_t *f = dt_alloc_file();
        if (!f) return -1;
        strncpy(f->name, path, DT_MAX_NAME - 1);
        f->name[DT_MAX_NAME - 1] = '\0';
        f->device_idx = idx;
        *out_fd = f->fd;
        return 0;
    }

    return -1;
}

int mkrn_device_tree_close(int fd)
{
    dt_file_t *f = dt_find_by_fd(fd);
    if (!f) return -1;
    dt_free_file(f);
    return 0;
}

int mkrn_device_tree_read(int fd, void *buf, uint32_t count)
{
    dt_file_t *f = dt_find_by_fd(fd);
    if (!f) return -1;

    if (f->read_offset > 0)
        return 0;

    /* Directory listing of /device/ — not readable via read() */
    if (f->device_idx < 0)
        return 0;

    m4k_device_t *d = &dt_devices[f->device_idx];
    char tmp[256];
    int len = 0;
    char nb[16];
    int ni, ri;
    uint32_t n;

#define ADD(s) do { for (int _i = 0; (s)[_i] && len < 250; _i++) tmp[len++] = (s)[_i]; } while(0)
#define ADD_NL do { tmp[len++] = '\n'; } while(0)
#define ADD_UINT(v) do { \
    n = (v); ni = 0; \
    if (n == 0) { nb[ni++] = '0'; } \
    else { ri = 0; while (n > 0) { rev[ri++] = '0' + (n % 10); n /= 10; } \
          while (ri > 0) nb[ni++] = rev[--ri]; } \
    for (int _j = 0; _j < ni; _j++) tmp[len++] = nb[_j]; \
} while(0)
#define ADD_HEX(v) do { \
    n = (v); ni = 0; \
    if (n == 0) { nb[ni++] = '0'; } \
    else { ri = 0; while (n > 0) { int d = n % 16; rev[ri++] = d < 10 ? '0'+d : 'a'+d-10; n /= 16; } \
          while (ri > 0) nb[ni++] = rev[--ri]; } \
    for (int _j = 0; _j < ni; _j++) tmp[len++] = nb[_j]; \
} while(0)

    char rev[16];
    ADD("name: "); ADD(d->name); ADD_NL;
    ADD("type: "); ADD(d->type); ADD_NL;
    ADD("io_base: 0x"); ADD_HEX(d->io_base); ADD_NL;
    ADD("irq: "); ADD_UINT(d->irq); ADD_NL;
    ADD("present: "); ADD(d->present ? "yes" : "no"); ADD_NL;
    ADD("desc: "); ADD(d->desc); ADD_NL;

#undef ADD
#undef ADD_NL
#undef ADD_UINT

    if (len > (int)count) len = (int)count;
    memcpy(buf, tmp, (uint32_t)len);
    f->read_offset = len;
    return len;
}

int mkrn_device_tree_write(int fd, const void *buf, uint32_t count)
{
    (void)fd; (void)buf; (void)count;
    return -1;
}

int mkrn_device_tree_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max)
{
    dt_file_t *f = dt_find_by_fd(fd);
    if (!f || !buf || max == 0) return 0;

    uint32_t written = 0;

    /* Listing /device/ — return all device names */
    for (int i = 0; i < dt_num_devices && written < max; i++) {
        memset(&buf[written], 0, sizeof(struct mkrn_vfs_dirent));
        strncpy(buf[written].name, dt_devices[i].name, M4K_VFS_MAX_FILENAME - 1);
        buf[written].name[M4K_VFS_MAX_FILENAME - 1] = '\0';
        buf[written].type = 0;
        written++;
    }

    return (int)written;
}

int mkrn_device_tree_is_dt_fd(int fd)
{
    return dt_find_by_fd(fd) ? 1 : 0;
}
