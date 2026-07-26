#include "m4sh.h"

#define FB_W 800
#define FB_H 600
#define GREEN 0x0000FF00
#define RED   0x00FF0000
#define WHITE 0x00FFFFFF

static void delay(uint32_t loops)
{
    for (volatile uint32_t i = 0; i < loops; i++);
}

void musr_cmd_gfx_test(int ac, char **av)
{
    (void)ac;
    (void)av;

    struct m4k_framebuffer_info fb;
    int ret = m4k_get_framebuffer_info(&fb);
    if (ret < 0) {
        c_red();
        out_puts("gfx_test: framebuffer not available\n");
        c_rst();
        return;
    }

    c_grn();
    out_puts("gfx_test: framebuffer info:\n");
    c_wht();
    out_puts("  phys_addr=0x");
    print_u32(fb.phys_addr);
    out_puts("\n  resolution=");
    print_u32(fb.width);
    out_puts("x");
    print_u32(fb.height);
    out_puts("x");
    print_u32(fb.bpp);
    out_puts("\n  pitch=");
    print_u32(fb.pitch);
    out_puts("\n");

    out_puts("Drawing test pattern...\n");
    ret = m4k_draw_test_pattern();
    if (ret < 0) {
        c_red();
        out_puts("gfx_test: draw test pattern failed\n");
        c_rst();
        return;
    }
    c_grn();
    out_puts("gfx_test: test pattern drawn.\n");
    out_puts("Entering mouse interactive mode.\n");
    out_puts("Move mouse, left-click to draw dots.\n");
    c_rst();

    volatile uint32_t *lfb = (volatile uint32_t *)fb.phys_addr;
    int last_buttons = 0;

    for (;;) {
        struct m4k_mouse_event ev;
        int got = m4k_get_mouse_event(&ev);
        if (got) {
            out_puts("  dx=");
            print_u32((uint32_t)(int16_t)ev.dx);
            out_puts(" dy=");
            print_u32((uint32_t)(int16_t)ev.dy);
            out_puts(" btns=");
            print_u32(ev.buttons);
            out_puts("\n");

            if ((ev.buttons & 1) && !(last_buttons & 1)) {
                static int px = 400, py = 300;
                px += ev.dx;
                py += ev.dy;
                if (px < 0) px = 0;
                if (px >= FB_W) px = FB_W - 1;
                if (py < 0) py = 0;
                if (py >= FB_H) py = FB_H - 1;
                lfb[py * FB_W + px] = GREEN;
            }
            last_buttons = ev.buttons;
        }

        m4k_flip();
        delay(2000000);
    }
}
