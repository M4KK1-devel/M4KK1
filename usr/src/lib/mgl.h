/*
 * mgl.h — M4KK1 GL compatibility layer (software, OpenGL 1.x subset).
 *
 * Fixed-pipeline immediate-mode API over the flat framebuffer.
 * Implementation is Zig (usr/src/sprach/mgl.zig), C ABI out.
 *
 * M4KK1 4P1 / 2026-08 / SPDX-License-Identifier: 4P1-Custom
 */
#ifndef _M4KK1_MGL_H_
#define _M4KK1_MGL_H_

#include <stdint.h>

/* matrix modes (GL values) */
#define MGL_MODELVIEW   0x1700
#define MGL_PROJECTION  0x1701

/* primitives */
#define MGL_TRIANGLES   0x0004
#define MGL_QUADS       0x0007

/* Bind the target framebuffer (must precede all drawing). */
void mgl_set_framebuffer(uint32_t *fb, int w, int h);

/* State */
void mgl_viewport(int x, int y, int w, int h);
void mgl_clear_color(float r, float g, float b, float a);
void mgl_clear(void);
void mgl_color3f(float r, float g, float b);

/* Matrices (2D affine subset) */
void mgl_matrix_mode(int mode);
void mgl_load_identity(void);
void mgl_translate_f(float x, float y);
void mgl_scale_f(float x, float y);

/* Immediate mode */
void mgl_begin(int mode);
void mgl_vertex2f(float x, float y);
void mgl_end(void);

#endif /* _M4KK1_MGL_H_ */
