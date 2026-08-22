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

/* ── Cursor overlay (save/restore software sprite) ── */
#define CURSOR_W 16
#define CURSOR_H 16

static const uint16_t cursor_bitmap[CURSOR_H] = {
    0xC000, 0xE000, 0xF000, 0xF800,
    0xFC00, 0xFE00, 0xFF00, 0xFF80,
    0xFFC0, 0xFFE0, 0xE7E0, 0xC3E0,
    0x81E0, 0x00E0, 0x0060, 0x0020,
};

static int32_t cursor_x = 400;
static int32_t cursor_y = 300;
static int32_t cursor_old_x = -1;
static int32_t cursor_old_y = -1;
static uint32_t cursor_bg[CURSOR_W * CURSOR_H];
static bool    cursor_enabled = false;

void mkrn_vesa_set_cursor_pos(int32_t x, int32_t y)
{
    if (x < 0) x = 0;
    if (x >= (int32_t)fb_info.width) x = (int32_t)fb_info.width - 1;
    if (y < 0) y = 0;
    if (y >= (int32_t)fb_info.height) y = (int32_t)fb_info.height - 1;
    cursor_x = x;
    cursor_y = y;
}

void mkrn_vesa_cursor_enable(int enable)
{
    cursor_enabled = !!enable;
}

/* XOR the cursor bitmap onto back_buffer at (sx, sy) */
static void cursor_xor_on_backbuf(int sx, int sy)
{
    if (!back_buffer) return;
    int w = (int)fb_info.width;
    int h = (int)fb_info.height;
    for (int row = 0; row < CURSOR_H; row++) {
        int py = sy + row;
        if (py < 0 || py >= h) continue;
        uint16_t bits = cursor_bitmap[row];
        for (int col = 0; col < CURSOR_W; col++) {
            int px = sx + col;
            if (px < 0 || px >= w) continue;
            if (bits & (0x8000 >> col))
                back_buffer[py * w + px] ^= 0x00FFFFFF;
        }
    }
}

/* Save a CURSOR_W x CURSOR_H rect from back_buffer into cursor_bg */
static void cursor_save_bg(int sx, int sy)
{
    if (!back_buffer) return;
    int w = (int)fb_info.width;
    int h = (int)fb_info.height;
    for (int row = 0; row < CURSOR_H; row++) {
        int py = sy + row;
        if (py < 0 || py >= h) {
            mkrn_memset(&cursor_bg[row * CURSOR_W], 0, CURSOR_W * 4);
            continue;
        }
        for (int col = 0; col < CURSOR_W; col++) {
            int px = sx + col;
            if (px < 0 || px >= w)
                cursor_bg[row * CURSOR_W + col] = 0;
            else
                cursor_bg[row * CURSOR_W + col] = back_buffer[py * w + px];
        }
    }
}

/* Restore cursor_bg pixels onto back_buffer (erase cursor) */
static void cursor_restore_bg(int sx, int sy)
{
    if (!back_buffer) return;
    int w = (int)fb_info.width;
    int h = (int)fb_info.height;
    for (int row = 0; row < CURSOR_H; row++) {
        int py = sy + row;
        if (py < 0 || py >= h) continue;
        for (int col = 0; col < CURSOR_W; col++) {
            int px = sx + col;
            if (px < 0 || px >= w) continue;
            back_buffer[py * w + px] = cursor_bg[row * CURSOR_W + col];
        }
    }
}

/* ── Full cursor cycle: restore old, save new, draw new ── */
void mkrn_vesa_update_cursor(void)
{
    if (!cursor_enabled || !back_buffer || !fb_info.initialized)
        return;

    /* Single source of truth: the mouse driver's accumulated absolute
     * position (already clamped to the screen and carrying the 2x
     * ballistics).  Syncing here (not only in mkrn_vesa_flip) means
     * mid-frame cursor moves stay aligned with hit-testing no matter
     * when the WM asks for an update. */
    if (mkrn_mouse_is_initialized()) {
        int32_t mx, my;
        mkrn_mouse_get_position(&mx, &my);
        cursor_x = mx;
        cursor_y = my;
    }

    int nx = cursor_x;
    int ny = cursor_y;

    if (nx >= (int)fb_info.width)  nx = (int)fb_info.width - 1;
    if (ny >= (int)fb_info.height) ny = (int)fb_info.height - 1;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;

    if (nx == cursor_old_x && ny == cursor_old_y)
        return;

    /* Restore old cursor position from saved background */
    if (cursor_old_x >= 0 && cursor_old_y >= 0)
        cursor_restore_bg(cursor_old_x, cursor_old_y);

    /* Save new position background and draw */
    cursor_save_bg(nx, ny);
    cursor_xor_on_backbuf(nx, ny);

    /* Flip both rects: old (restored) and new (with cursor) */
    if (cursor_old_x >= 0 && cursor_old_y >= 0)
        mkrn_vesa_flip_rect(cursor_old_x, cursor_old_y, CURSOR_W, CURSOR_H);
    mkrn_vesa_flip_rect(nx, ny, CURSOR_W, CURSOR_H);

    cursor_old_x = nx;
    cursor_old_y = ny;
}

/* ── Partial flip: copy a rect from back_buffer to LFB ── */
void mkrn_vesa_flip_rect(int x, int y, int w, int h)
{
    if (!fb_info.initialized || !back_buffer)
        return;
    if (w <= 0 || h <= 0) return;

    int scr_w = (int)fb_info.width;
    int scr_h = (int)fb_info.height;

    /* Clip to screen */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    if (w <= 0 || h <= 0) return;

    uint32_t *lfb = (uint32_t *)fb_info.phys_addr;
    uint32_t *src = back_buffer + y * scr_w + x;
    uint32_t *dst = lfb + y * scr_w + x;

    for (int row = 0; row < h; row++) {
        uint32_t pixels = (uint32_t)w;
        uint32_t *s = src + row * scr_w;
        uint32_t *d = dst + row * scr_w;
        __asm__ volatile("cld; rep movsl"
            : "+c"(pixels), "+D"(d), "+S"(s)
            : : "memory");
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
    /* The kernel linker heap (0x29c000..0x69c000, 4MB) cannot hold a
     * 1.9MB frame buffer plus every other allocation: a first-fit
     * block near the heap tail silently ran past __heap_end and the
     * full-screen gradient fill overwrote the fixed user-ELF window
     * at 0x600000+ (killing init/MDM — the "MDM GPF" of commit
     * 8374944).  Large buffers belong to the buddy zone above the
     * ramdisk (0x3000000+), page-aligned, far from every fixed
     * window.  Pages: size rounded up to 4KB. */
    {
        size_t pages = (back_buffer_size + 0xFFF) >> 12;
        back_buffer = (uint32_t *)mkrn_memory_alloc_page(pages);
    }
    if (!back_buffer) {
        M4K_LOG_ERROR("vesa: failed to allocate back buffer\n");
        return;
    }
    /* zero the fresh pages so the first flip does not flash garbage */
    mkrn_memset(back_buffer, 0, back_buffer_size);

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
    mkrn_console_write("\n");

    cursor_enabled = true;

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

    /* Draw cursor onto fresh back_buffer (composite just wrote the scene) */
    if (cursor_enabled) {
        cursor_save_bg(cursor_x, cursor_y);
        cursor_xor_on_backbuf(cursor_x, cursor_y);
    }

    uint32_t *lfb = (uint32_t *)fb_info.phys_addr;
    uint32_t pixels = fb_info.width * fb_info.height;

    __asm__ volatile("cld; rep movsl"
        : "+c"(pixels), "+D"(lfb)
        : "S"(back_buffer)
        : "memory");

    /* Track cursor position for mid-frame cursor_update save/restore */
    cursor_old_x = cursor_x;
    cursor_old_y = cursor_y;
}

void mkrn_vesa_clear(uint32_t color)
{
    if (!back_buffer)
        return;

    /* rep stosl fill: 1024x600x4 = 2.4MB per clear — the per-pixel
     * C loop dominated boot and window-drag redraw paths. */
    uint32_t pixels = fb_info.width * fb_info.height;
    __asm__ volatile("cld; rep stosl"
        : "+c"(pixels), "+D"(back_buffer)
        : "a"(color)
        : "memory");
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
        if (!back_buffer)
            return;
        /* Clip once, then write through a row pointer — the old
         * per-pixel put_pixel re-ran full bounds checks for every
         * pixel of every filled rect (the hottest GUI primitive). */
        int x2 = x + w;
        int y2 = y + h;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x2 > (int)fb_info.width)  x2 = (int)fb_info.width;
        if (y2 > (int)fb_info.height) y2 = (int)fb_info.height;
        if (x >= x2 || y >= y2)
            return;
        for (int row = y; row < y2; row++) {
            uint32_t *prow =
                back_buffer + (uint32_t)row * fb_info.width
                + (uint32_t)x;
            uint32_t cnt = (uint32_t)(x2 - x);
            /* rep stosl — the same win flip_rect/gfx_blit already get
             * from rep movsl; store-fill skips the load of every pixel. */
            __asm__ volatile("cld; rep stosl"
                : "+c"(cnt), "+D"(prow)
                : "a"(color)
                : "memory");
        }
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
    /* Clip once per glyph instead of bounds-checking every pixel via
     * put_pixel — text rendering is the hottest per-pixel path after
     * rect fill.  Write through row pointers like draw_rect does. */
    int x0 = x > 0 ? x : 0;
    int x_end = x + FONT_8X16_WIDTH;
    if (x_end > (int)fb_info.width)
        x_end = (int)fb_info.width;
    int y0 = y > 0 ? y : 0;
    int y_end = y + FONT_8X16_HEIGHT;
    if (y_end > (int)fb_info.height)
        y_end = (int)fb_info.height;
    if (x0 >= x_end || y0 >= y_end)
        return;

    for (int row = y0; row < y_end; row++) {
        unsigned char bits = glyph[row - y];
        uint32_t *prow =
            back_buffer + (uint32_t)row * fb_info.width;
        for (int col = x0; col < x_end; col++)
            prow[col] =
                (bits & (0x80 >> (col - x))) ? fg : bg;
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

/* ── Syscall: blit a client pixel buffer into the back buffer ──
 * Copies a w*h run of 32-bit pixels from the client's memory (flat
 * address space, so any address is directly readable) into the
 * back buffer at (x, y), clipped to the screen.  This is the
 * primitive Copland uses to composite client-rendered window
 * surfaces (the Sprach model). */
uint32_t m4k_syscall_gfx_blit_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    int x = (int)arg1;
    int y = (int)arg2;
    int w = (int)arg3;
    int h = (int)arg4;
    const uint32_t *src = (const uint32_t *)arg5;

    if (!fb_info.initialized || !back_buffer || !src)
        return (uint32_t)-1;
    if (w <= 0 || h <= 0)
        return 0;

    int scr_w = (int)fb_info.width;
    int scr_h = (int)fb_info.height;

    /* The client's source buffer has one row of orig_w pixels; clipping
     * shrinks the copied width but the row stride stays orig_w. */
    int orig_w = w;

    /* Clip source/dest to the visible region */
    if (x < 0) { w += x; src += (uint32_t)(-x); x = 0; }
    if (y < 0) { h += y; src += (uint32_t)(-y) * (uint32_t)orig_w; y = 0; }
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    if (w <= 0 || h <= 0)
        return 0;

    uint32_t *dst = back_buffer + (uint32_t)y * (uint32_t)scr_w + (uint32_t)x;

    for (int row = 0; row < h; row++) {
        const uint32_t *s = src + (uint32_t)row * (uint32_t)orig_w;
        uint32_t *d = dst + (uint32_t)row * (uint32_t)scr_w;
        uint32_t cnt = (uint32_t)w;
        __asm__ volatile("cld; rep movsl"
            : "+c"(cnt), "+D"(d), "+S"(s)
            :
            : "memory");
    }
    return 0;
}

/* ── Syscall: fill a rect with a vertical gradient ──
 * The wallpaper repaint used to issue one m4k_draw_rect syscall per
 * scanline (600 syscalls + scheduler yields per full-screen fill, h
 * per damage region).  Do the whole gradient in one kernel-side pass:
 * compute the row color here, then rep stosl the row.  Gradient span
 * (top color at y=0, bottom at screen height-1) matches the
 * user-space gui_draw_gradient/copland_wallpaper_rect math. */
uint32_t m4k_syscall_fill_gradient_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    struct m4k_gradient_params {
        uint32_t top;
        uint32_t bottom;
    };
    const struct m4k_gradient_params *gp =
        (const struct m4k_gradient_params *)arg5;

    int x = (int)arg1;
    int y = (int)arg2;
    int w = (int)arg3;
    int h = (int)arg4;

    if (!fb_info.initialized || !back_buffer || !gp)
        return (uint32_t)-1;
    if (w <= 0 || h <= 0)
        return 0;

    int scr_w = (int)fb_info.width;
    int scr_h = (int)fb_info.height;

    /* Clip to screen */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    if (w <= 0 || h <= 0)
        return 0;

    /* Gradient coefficients: color(y) = c0 + (c1-c0)*y/(scr_h-1).
     * Precompute the per-channel steps in 16.16 fixed point so the
     * row loop needs no divides.  Signed steps: downward gradients
     * (e.g. 0xFF->0x00) would underflow unsigned subtraction. */
    uint32_t fh = (uint32_t)(scr_h > 1 ? scr_h - 1 : 1);
    uint32_t r0 = (gp->top    >> 16) & 0xFF;
    uint32_t g0 = (gp->top    >>  8) & 0xFF;
    uint32_t b0 =  gp->top          & 0xFF;
    uint32_t r1 = (gp->bottom >> 16) & 0xFF;
    uint32_t g1 = (gp->bottom >>  8) & 0xFF;
    uint32_t b1 =  gp->bottom       & 0xFF;
    int32_t rstep = ((int32_t)(r1 - r0) << 16) / (int32_t)fh;
    int32_t gstep = ((int32_t)(g1 - g0) << 16) / (int32_t)fh;
    int32_t bstep = ((int32_t)(b1 - b0) << 16) / (int32_t)fh;
    int32_t racc = (int32_t)(r0 << 16);
    int32_t gacc = (int32_t)(g0 << 16);
    int32_t bacc = (int32_t)(b0 << 16);
    /* Advance to the first destination row */
    for (int i = 0; i < y; i++) {
        racc += rstep; gacc += gstep; bacc += bstep;
    }

    uint32_t *dst = back_buffer + (uint32_t)y * (uint32_t)scr_w
                  + (uint32_t)x;
    for (int row = 0; row < h; row++) {
        uint32_t color = (((uint32_t)(racc >> 16) & 0xFFu) << 16)
                       | (((uint32_t)(gacc >> 16) & 0xFFu) <<  8)
                       | ( (uint32_t)(bacc >> 16) & 0xFFu);
        uint32_t *d = dst + (uint32_t)row * (uint32_t)scr_w;
        uint32_t cnt = (uint32_t)w;
        __asm__ volatile("cld; rep stosl"
            : "+c"(cnt), "+D"(d)
            : "a"(color)
            : "memory");
        racc += rstep; gacc += gstep; bacc += bstep;
    }
    return 0;
}

/* ── Syscall: flip a partial region of back_buffer to LFB ── */
uint32_t m4k_syscall_flip_rect_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg5;
    mkrn_vesa_flip_rect((int)arg1, (int)arg2, (int)arg3, (int)arg4);
    return 0;
}

/* ── Syscall: update cursor position (save/restore + draw + flip rects) ── */
uint32_t m4k_syscall_update_cursor_impl(
    uint32_t arg1, uint32_t arg2, uint32_t arg3,
    uint32_t arg4, uint32_t arg5)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    mkrn_vesa_update_cursor();
    return 0;
}
