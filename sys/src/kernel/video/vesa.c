#include <stdint.h>
#include <stdbool.h>
#include <console.h>
#include <kernel.h>
#include <memory.h>
#include <video.h>
#include <mouse.h>

/* ── Static framebuffer info ── */
static struct {
    uint32_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    bool     initialized;
} fb_info;

/* ── Back buffer for double buffering ── */
static uint32_t *back_buffer = NULL;
static uint32_t  back_buffer_size = 0;

/* ── Font data ── */
#include "font_8x16.c"

/* ── Cursor overlay ── */
#define CURSOR_W 16
#define CURSOR_H 16

static const uint16_t cursor_bitmap[CURSOR_H] = {
    0x8000, 0xC000, 0xA000, 0x9000,
    0x8800, 0x8400, 0x8200, 0x8100,
    0x8080, 0x8040, 0x8020, 0x8010,
    0x8E08, 0x9100, 0xA100, 0xC100,
};

static int32_t cursor_x = 400;
static int32_t cursor_y = 300;
static bool    cursor_enabled = false;

void mkrn_vesa_set_cursor_pos(int32_t x, int32_t y)
{
    cursor_x = x;
    cursor_y = y;
}

void mkrn_vesa_cursor_enable(int enable)
{
    cursor_enabled = !!enable;
}

static void vesa_draw_cursor(void)
{
    if (!cursor_enabled || !fb_info.initialized)
        return;

    uint32_t *lfb = (uint32_t *)fb_info.phys_addr;
    int w = (int)fb_info.width;
    int h = (int)fb_info.height;
    int sx = cursor_x;
    int sy = cursor_y;

    for (int row = 0; row < CURSOR_H; row++) {
        int py = sy + row;
        if (py < 0 || py >= h) continue;
        uint16_t bits = cursor_bitmap[row];
        for (int col = 0; col < CURSOR_W; col++) {
            int px = sx + col;
            if (px < 0 || px >= w) continue;
            if (bits & (0x8000 >> col))
                lfb[py * w + px] ^= 0x00FFFFFF;
        }
    }
}

/* ── I/O port helpers ── */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ── Bochs VBE helpers ── */
static void vbe_write(uint16_t index, uint16_t value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static uint16_t vbe_read(uint16_t index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/* ── PCI config read/write ── */
static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000
        | ((uint32_t)bus   << 16)
        | ((uint32_t)slot  << 11)
        | ((uint32_t)func  << 8)
        | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    uint32_t address = 0x80000000
        | ((uint32_t)bus   << 16)
        | ((uint32_t)slot  << 11)
        | ((uint32_t)func  << 8)
        | (offset & 0xFC);
    outl(0xCF8, address);
    outl(0xCFC, val);
}

/* ── PCI device dump ── */
static void pci_dump_all(void)
{
    M4K_LOG_INFO("vesa: PCI device dump:\n");
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t vendor = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0);
            if (vendor == 0xFFFFFFFF || vendor == 0)
                continue;
            uint32_t dev_id = vendor >> 16;
            uint32_t ven_id = vendor & 0xFFFF;
            uint32_t class_rev = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 8);
            uint8_t class_code = (uint8_t)((class_rev >> 24) & 0xFF);
            uint8_t subclass   = (uint8_t)((class_rev >> 16) & 0xFF);
            mkrn_console_write("  ");
            mkrn_console_write_hex(bus);
            mkrn_console_write(":");
            mkrn_console_write_hex(slot);
            mkrn_console_write(".0  ven=");
            mkrn_console_write_hex(ven_id);
            mkrn_console_write(" dev=");
            mkrn_console_write_hex(dev_id);
            mkrn_console_write(" class=");
            mkrn_console_write_hex(class_code);
            mkrn_console_write(" sub=");
            mkrn_console_write_hex(subclass);
            mkrn_console_write("  BARs:");
            for (int bar = 0; bar < 6; bar++) {
                uint32_t b = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4);
                mkrn_console_write(" ");
                mkrn_console_write_hex(b);
            }
            mkrn_console_write("\n");
        }
    }
}

/* ── Find VGA controller BAR0 ── */
static uint32_t pci_find_vga_lfb(void)
{
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t vendor = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0);
            if (vendor == 0xFFFFFFFF || vendor == 0)
                continue;
            uint32_t dev_id = vendor >> 16;
            uint32_t ven_id = vendor & 0xFFFF;
            uint32_t class_rev = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 8);
            uint8_t class_code = (uint8_t)((class_rev >> 24) & 0xFF);
            if (class_code != 0x03)
                continue;
            /* VGA controller found - dump all BARs */
            M4K_LOG_INFO("vesa: VGA at PCI ");
            mkrn_console_write_hex(bus);
            mkrn_console_write(":");
            mkrn_console_write_hex(slot);
            mkrn_console_write(".0 (");
            mkrn_console_write_hex(ven_id);
            mkrn_console_write(":");
            mkrn_console_write_hex(dev_id);
            mkrn_console_write(")\n");
            for (int bar = 0; bar < 6; bar++) {
                uint32_t b = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4);
                uint32_t addr;
                if (b & 1) {
                    addr = b & 0xFFFFFFFC;
                    mkrn_console_write("  BAR");
                    mkrn_console_write_dec(bar);
                    mkrn_console_write(": I/O 0x");
                } else {
                    addr = b & 0xFFFFFFF0;
                    mkrn_console_write("  BAR");
                    mkrn_console_write_dec(bar);
                    mkrn_console_write(": MEM 0x");
                }
                mkrn_console_write_hex(addr);
                mkrn_console_write(" (raw=0x");
                mkrn_console_write_hex(b);
                mkrn_console_write(")\n");
            }
            /* Find the largest memory BAR >= 1MB - that's our LFB */
            uint32_t lfb = 0;
            uint32_t lfb_size = 0;
            for (int bar = 0; bar < 6; bar++) {
                uint32_t b = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4);
                if (b & 1) continue; /* skip I/O BARs */
                uint32_t addr = b & 0xFFFFFFF0;
                if (addr == 0) continue;
                /* Get BAR size by writing all 1s */
                pci_config_write((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4, 0xFFFFFFFF);
                uint32_t size_raw = pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4);
                pci_config_write((uint8_t)bus, (uint8_t)slot, 0, 0x10 + bar * 4, b); /* restore */
                uint32_t size = (~(size_raw & 0xFFFFFFF0)) + 1;
                if (size > lfb_size) {
                    lfb = addr;
                    lfb_size = size;
                }
            }
            if (lfb) {
                M4K_LOG_INFO("vesa: found LFB at 0x");
                mkrn_console_write_hex(lfb);
                mkrn_console_write(" (size=");
                mkrn_console_write_dec(lfb_size);
                mkrn_console_write(")\n");
            }
            return lfb;
        }
    }
    M4K_LOG_WARN("vesa: no VGA controller found via PCI scan\n");
    return 0;
}

/* ── Initialize Bochs VBE framebuffer ── */
void mkrn_vesa_init(void)
{
    M4K_LOG_INFO("vesa: initializing Bochs VBE\n");

    pci_dump_all();

    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id < 0xB0C0) {
        M4K_LOG_ERROR("vesa: Bochs VBE not available (ID=");
        mkrn_console_write_hex(id);
        mkrn_console_write(")\n");
        return;
    }

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, VBE_DEFAULT_WIDTH);
    vbe_write(VBE_DISPI_INDEX_YRES, VBE_DEFAULT_HEIGHT);
    vbe_write(VBE_DISPI_INDEX_BPP, VBE_DEFAULT_BPP);
    vbe_write(VBE_DISPI_INDEX_ENABLE,
        VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    uint32_t lfb = pci_find_vga_lfb();
    if (lfb == 0)
        lfb = 0xE0000000;

    fb_info.phys_addr   = lfb;
    fb_info.width       = VBE_DEFAULT_WIDTH;
    fb_info.height      = VBE_DEFAULT_HEIGHT;
    fb_info.bpp         = VBE_DEFAULT_BPP;
    fb_info.pitch       = VBE_DEFAULT_WIDTH * (VBE_DEFAULT_BPP / 8);
    fb_info.initialized = true;

    /* Allocate back buffer */
    back_buffer_size = fb_info.width * fb_info.height * (fb_info.bpp / 8);
    back_buffer = (uint32_t *)mkrn_alloc(back_buffer_size);
    if (!back_buffer) {
        M4K_LOG_ERROR("vesa: failed to allocate back buffer\n");
        return;
    }

    mkrn_console_write("[INFO] vesa: framebuffer at 0x");
    mkrn_console_write_hex(fb_info.phys_addr);
    mkrn_console_write(", ");
    mkrn_console_write_dec(fb_info.width);
    mkrn_console_write("x");
    mkrn_console_write_dec(fb_info.height);
    mkrn_console_write("x");
    mkrn_console_write_dec(fb_info.bpp);
    mkrn_console_write(", pitch=");
    mkrn_console_write_dec(fb_info.pitch);
    mkrn_console_write(", back_buf=0x");
    mkrn_console_write_hex((uint32_t)back_buffer);
    mkrn_console_write("\n");

    /* Clear to black */
    mkrn_vesa_clear(VESA_COLOR_BLACK);

    /* Quick self-test: write to LFB and verify readback */
    {
        uint32_t *lfb_test = (uint32_t *)fb_info.phys_addr;
        lfb_test[0] = 0x00FF0000;
        lfb_test[1] = 0x0000FF00;
        if (lfb_test[0] == 0x00FF0000 && lfb_test[1] == 0x0000FF00)
            M4K_LOG_INFO("vesa: LFB readback OK\n");
        else
            M4K_LOG_WARN("vesa: LFB readback FAILED\n");
    }
}

/* ── Phase 1: Double buffering ── */

void mkrn_vesa_flip(void)
{
    if (!fb_info.initialized || !back_buffer)
        return;

    if (mkrn_mouse_is_initialized()) {
        int32_t mx, my;
        mkrn_mouse_get_position(&mx, &my);
        mkrn_vesa_set_cursor_pos(mx, my);
    }

    uint32_t *lfb = (uint32_t *)fb_info.phys_addr;
    uint32_t pixels = fb_info.width * fb_info.height;

    __asm__ volatile("cld; rep movsl"
        : "+c"(pixels), "+D"(lfb)
        : "S"(back_buffer)
        : "memory");

    vesa_draw_cursor();
}

void mkrn_vesa_clear(uint32_t color)
{
    if (!back_buffer)
        return;

    uint32_t pixels = fb_info.width * fb_info.height;
    for (uint32_t i = 0; i < pixels; i++)
        back_buffer[i] = color;
}

/* ── Phase 2: Drawing primitives ── */

void mkrn_vesa_put_pixel(int x, int y, uint32_t color)
{
    if (!back_buffer)
        return;
    if (x < 0 || x >= (int)fb_info.width || y < 0 || y >= (int)fb_info.height)
        return;
    back_buffer[y * fb_info.width + x] = color;
}

void mkrn_vesa_draw_line(int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int stepx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0; if (dx < 0) dx = -dx;
    int stepy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0; if (dy < 0) dy = -dy;

    int err = dx - dy;
    int x = x1, y = y1;

    while (1) {
        mkrn_vesa_put_pixel(x, y, color);
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += stepx; }
        if (e2 < dx)  { err += dx; y += stepy; }
    }
}

void mkrn_vesa_draw_rect(int x, int y, int w, int h, uint32_t color, int fill)
{
    if (fill) {
        int x2 = x + w;
        int y2 = y + h;
        for (int row = y; row < y2; row++)
            for (int col = x; col < x2; col++)
                mkrn_vesa_put_pixel(col, row, color);
    } else {
        mkrn_vesa_draw_line(x, y, x + w - 1, y, color);
        mkrn_vesa_draw_line(x, y, x, y + h - 1, color);
        mkrn_vesa_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
        mkrn_vesa_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    }
}

void mkrn_vesa_put_char(int x, int y, char c, uint32_t fg, uint32_t bg)
{
    int idx = (int)(unsigned char)c - FONT_8X16_FIRST;
    if (idx < 0 || idx >= FONT_8X16_CHARS)
        return;

    const unsigned char *glyph = font_8x16 + idx * FONT_8X16_HEIGHT;
    for (int row = 0; row < FONT_8X16_HEIGHT && (y + row) < (int)fb_info.height; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < FONT_8X16_WIDTH && (x + col) < (int)fb_info.width; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && py >= 0)
                mkrn_vesa_put_pixel(px, py, (bits & (0x80 >> col)) ? fg : bg);
        }
    }
}

void mkrn_vesa_put_string(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    int cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += FONT_8X16_HEIGHT + 2;
        } else {
            mkrn_vesa_put_char(cx, y, *s, fg, bg);
            cx += FONT_8X16_WIDTH + 1;
        }
        s++;
    }
}

/* ── Draw test pattern (for userspace trigger) ── */
void mkrn_vesa_draw_test_pattern(void)
{
    if (!fb_info.initialized || !back_buffer)
        return;

    int w = (int)fb_info.width;
    int h = (int)fb_info.height;

    mkrn_vesa_clear(VESA_COLOR_BLACK);

    /* Rainbow stripes */
    int bar_h = h / 6;
    uint32_t colors[] = {
        VESA_COLOR_RED, VESA_COLOR_ORANGE, VESA_COLOR_YELLOW,
        VESA_COLOR_GREEN, VESA_COLOR_BLUE, VESA_COLOR_MAGENTA
    };
    for (int i = 0; i < 6; i++)
        mkrn_vesa_draw_rect(0, i * bar_h, w, bar_h, colors[i], 1);

    /* White border */
    mkrn_vesa_draw_rect(4, 4, w - 8, h - 8, VESA_COLOR_WHITE, 0);

    /* Centered text */
    const char *line1 = "M4KK1 Graphics Test";
    const char *line2 = "Framebuffer 800x600x32";
    int l1 = 0; while (line1[l1]) l1++;
    int l2 = 0; while (line2[l2]) l2++;
    int cx1 = (w - l1 * (FONT_8X16_WIDTH + 1)) / 2;
    int cx2 = (w - l2 * (FONT_8X16_WIDTH + 1)) / 2;
    int cy = h / 2;

    mkrn_vesa_put_string(cx1, cy - 20, line1, VESA_COLOR_WHITE, VESA_COLOR_BLACK);
    mkrn_vesa_put_string(cx2, cy + 4, line2, VESA_COLOR_YELLOW, VESA_COLOR_BLACK);

    mkrn_vesa_flip();
}

/* ── Syscall: draw test pattern ── */
uint32_t m4k_syscall_draw_test_pattern_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    mkrn_vesa_draw_test_pattern();
    return 0;
}

/* ── Syscall: flip (update display + cursor) ── */
uint32_t m4k_syscall_flip_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    mkrn_vesa_flip();
    return 0;
}

/* ── Syscall: return framebuffer info to userspace ── */
uint32_t m4k_syscall_get_framebuffer_info_impl(
    uint32_t buf_ptr, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (!fb_info.initialized)
        return (uint32_t)-1;

    struct m4k_framebuffer_info *ufb = (struct m4k_framebuffer_info *)buf_ptr;
    if (!ufb)
        return (uint32_t)-1;

    ufb->phys_addr = fb_info.phys_addr;
    ufb->width     = fb_info.width;
    ufb->height    = fb_info.height;
    ufb->bpp       = fb_info.bpp;
    ufb->pitch     = fb_info.pitch;

    return 0;
}

/* ── Syscall: draw rectangle ── */
uint32_t m4k_syscall_draw_rect_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    int x = (int)arg1;
    int y = (int)arg2;
    int w = (int)arg3;
    int h = (int)arg4;
    uint32_t color = arg5;

    if (!fb_info.initialized || !back_buffer)
        return (uint32_t)-1;

    mkrn_vesa_draw_rect(x, y, w, h, color, 1);
    return 0;
}

/* ── Syscall: draw text string ── */
uint32_t m4k_syscall_draw_text_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    int x = (int)arg1;
    int y = (int)arg2;
    const char *str = (const char *)arg3;
    uint32_t fg = arg4;
    uint32_t bg = arg5;

    if (!fb_info.initialized || !back_buffer || !str)
        return (uint32_t)-1;

    mkrn_vesa_put_string(x, y, str, fg, bg);
    return 0;
}
