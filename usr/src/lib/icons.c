/*
 * ==== ICON LIBRARY ====
 * M4KK1 4P1 - icons.c
 * Description: 32x32 ARGB icon bitmap set (procedurally initialized)
 *
 * Each icon is a uint32_t[32*32] array in 0xAARRGGBB (alpha 0=transp).
 * Icons are painted once at startup by draw code, then blitted by
 * gui_draw_icon() anywhere (Dock, Launchpad, file manager).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"
#include "icons.h"

uint32_t icon_folder[ICON_SIZE * ICON_SIZE];
uint32_t icon_file[ICON_SIZE * ICON_SIZE];
uint32_t icon_text[ICON_SIZE * ICON_SIZE];
uint32_t icon_code[ICON_SIZE * ICON_SIZE];
uint32_t icon_terminal[ICON_SIZE * ICON_SIZE];
uint32_t icon_gear[ICON_SIZE * ICON_SIZE];
uint32_t icon_launchpad[ICON_SIZE * ICON_SIZE];
uint32_t icon_window[ICON_SIZE * ICON_SIZE];
uint32_t icon_error[ICON_SIZE * ICON_SIZE];

static void ipx(uint32_t *ic, int x, int y, uint32_t c)
{
    if (x < 0 || x >= ICON_SIZE || y < 0 || y >= ICON_SIZE)
        return;
    ic[y * ICON_SIZE + x] = c;
}

static void irect(uint32_t *ic, int x0, int y0, int x1, int y1, uint32_t c)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            ipx(ic, x, y, c);
}

/* Bresenham-ish line for icon art */
static void iline(uint32_t *ic, int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    int err = dx - dy;
    for (;;) {
        ipx(ic, x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

static void icircle_fill(uint32_t *ic, int cx, int cy, int r, uint32_t c)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                ipx(ic, cx + x, cy + y, c);
}

static void icircle_ring(uint32_t *ic, int cx, int cy, int r, uint32_t c)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r * r && d >= (r - 1) * (r - 1))
                ipx(ic, cx + x, cy + y, c);
        }
}

void icons_init(void)
{
    static int done = 0;
    if (done)
        return;
    done = 1;

    /* folder: tan back tab + front face */
    irect(&icon_folder[0], 2, 8, 29, 27, 0xFFB08040);
    irect(&icon_folder[0], 2, 8, 12, 13, 0xFF8F6430);
    irect(&icon_folder[0], 4, 12, 27, 25, 0xFFE8C080);
    irect(&icon_folder[0], 4, 12, 27, 14, 0xFFF0D0A0);

    /* file: white sheet with folded corner */
    irect(&icon_file[0], 7, 3, 24, 28, 0xFFE0E0E0);
    irect(&icon_file[0], 8, 4, 23, 27, 0xFFF8F8F8);
    iline(&icon_file[0], 18, 3, 24, 9, 0xFFB0B0B0);
    irect(&icon_file[0], 19, 4, 23, 8, 0xFFC8C8C8);
    for (int i = 0; i < 4; i++)
        irect(&icon_file[0], 11, 10 + i * 4, 20, 11 + i * 4, 0xFF909090);

    /* text (.txt): sheet + "T" glyph */
    irect(&icon_text[0], 7, 3, 24, 28, 0xFFE0E0E0);
    irect(&icon_text[0], 8, 4, 23, 27, 0xFFF8F8F8);
    irect(&icon_text[0], 11, 8, 20, 10, 0xFF2060C0);
    irect(&icon_text[0], 14, 8, 16, 20, 0xFF2060C0);
    for (int i = 0; i < 3; i++)
        irect(&icon_text[0], 11, 16 + i * 3, 20, 17 + i * 3, 0xFF909090);

    /* code (.c): sheet + green brackets */
    irect(&icon_code[0], 7, 3, 24, 28, 0xFF202830);
    irect(&icon_code[0], 8, 4, 23, 27, 0xFF28323C);
    iline(&icon_code[0], 12, 10, 9, 15, 0xFF40E080);
    iline(&icon_code[0], 9, 15, 12, 20, 0xFF40E080);
    iline(&icon_code[0], 19, 10, 22, 15, 0xFF40E080);
    iline(&icon_code[0], 22, 15, 19, 20, 0xFF40E080);
    ipx(&icon_code[0], 15, 14, 0xFF40E080);
    ipx(&icon_code[0], 16, 15, 0xFF40E080);

    /* terminal: dark screen + green ">_" prompt */
    irect(&icon_terminal[0], 2, 4, 29, 27, 0xFF505050);
    irect(&icon_terminal[0], 3, 5, 28, 26, 0xFF101018);
    irect(&icon_terminal[0], 3, 5, 28, 7, 0xFF606060);
    iline(&icon_terminal[0], 7, 12, 11, 16, 0xFF30D848);
    iline(&icon_terminal[0], 11, 16, 7, 20, 0xFF30D848);
    irect(&icon_terminal[0], 14, 19, 15, 20, 0xFF30D848);

    /* gear */
    icircle_fill(&icon_gear[0], 15, 15, 10, 0xFF808090);
    icircle_fill(&icon_gear[0], 15, 15, 6, 0x00000000);
    icircle_ring(&icon_gear[0], 15, 15, 5, 0xFFB0B0C0);
    /* gear teeth at 8 compass points, radius 11 (integer table —
     * the standalone GUI ELFs link without libm) */
    static const int gt8[8][2] = {
        { 11, 0 }, { 8, 8 }, { 0, 11 }, { -8, 8 },
        { -11, 0 }, { -8, -8 }, { 0, -11 }, { 8, -8 },
    };
    for (int a = 0; a < 8; a++) {
        int bx = 15 + gt8[a][0];
        int by = 15 + gt8[a][1];
        irect(&icon_gear[0], bx - 2, by - 2, bx + 2, by + 2, 0xFF808090);
    }

    /* launchpad: 3x3 rounded grid on gradient */
    irect(&icon_launchpad[0], 2, 2, 29, 29, 0xFF203050);
    irect(&icon_launchpad[0], 2, 2, 29, 15, 0xFF2A4070);
    for (int gy = 0; gy < 3; gy++)
        for (int gx = 0; gx < 3; gx++) {
            int x0 = 5 + gx * 9, y0 = 5 + gy * 9;
            uint32_t cols[9] = {
                0xFFE05050, 0xFFE0A030, 0xFF40B050,
                0xFF4090E0, 0xFF9060D0, 0xFF50B0B0,
                0xFFD06090, 0xFFA0A0A0, 0xFFF0D060
            };
            irect(&icon_launchpad[0], x0, y0, x0 + 6, y0 + 6,
                  cols[gy * 3 + gx]);
        }

    /* window: app window with title bar */
    irect(&icon_window[0], 3, 5, 28, 27, 0xFFC0C0C0);
    irect(&icon_window[0], 3, 5, 28, 9, 0xFF3060C0);
    ipx(&icon_window[0], 6, 7, 0xFFE05050);
    ipx(&icon_window[0], 9, 7, 0xFFE0C040);
    ipx(&icon_window[0], 12, 7, 0xFF40C060);
    irect(&icon_window[0], 5, 11, 26, 25, 0xFFF0F0F0);

    /* error / warning */
    icircle_fill(&icon_error[0], 15, 15, 13, 0xFFD03030);
    irect(&icon_error[0], 13, 7, 17, 18, 0xFFF0F0F0);
    irect(&icon_error[0], 13, 21, 17, 23, 0xFFF0F0F0);
}
