//! mgl.zig — M4KK1 GL compatibility layer (software, OpenGL 1.x subset).
//!
//! A fixed-pipeline immediate-mode rasterizer for the flat framebuffer.
//! Semantics follow OpenGL 1.x: state machine (viewport / clear color /
//! color), matrix stack (MODELVIEW / PROJECTION, 4x4 row-major),
//! glBegin..glEnd vertex batches rasterized with a top-left-fill
//! triangle rule and per-vertex color interpolation (Gouraud, u8 chans).
//!
//! Why not real Vulkan/OpenGL: M4KK1 has no GPU and no MMU — the
//! display is a VESA linear framebuffer composited in software.  This
//! layer gives GUI code a GL-shaped API today; if a hw driver ever
//! lands, the C callers keep their calls and only this file changes.
//!
//! Freestanding i386, C ABI out.  No fpu assumptions: all raster math
//! is integer (fixed-point 16.16), matching the soft-float i386 build.

const std = @import("std");

// ─── constants (GL-compatible values) ──────────────────────────

pub const GL_MODELVIEW: i32 = 0x1700;
pub const GL_PROJECTION: i32 = 0x1701;

pub const GL_TRIANGLES: i32 = 0x0004;
pub const GL_QUADS: i32 = 0x0007;

pub const GL_RGB: i32 = 0x1907;
pub const GL_RGBA: i32 = 0x1908;

/// Max vertices buffered per glBegin..glEnd batch.
const MAX_VERTS: usize = 64;

// ─── vertex / state ────────────────────────────────────────────

const Vertex = struct {
    x: i32, // screen-space after transform (16.16 fixed for interp)
    y: i32,
    r: u8,
    g: u8,
    b: u8,
};

/// Global state — single context, statically allocated (no heap in
/// freestanding).  fb defaults to null; mgl_set_framebuffer binds it.
var fb: ?[*]u32 = null;
var fb_w: i32 = 0;
var fb_h: i32 = 0;

var viewport_x: i32 = 0;
var viewport_y: i32 = 0;
var viewport_w: i32 = 0;
var viewport_h: i32 = 0;

var clear_r: u8 = 0;
var clear_g: u8 = 0;
var clear_b: u8 = 0;

var cur_r: u8 = 255;
var cur_g: u8 = 255;
var cur_b: u8 = 255;

var matrix_mode: i32 = GL_MODELVIEW;

/// 4x4 matrices, row-major, 16.16 fixed-point.
const M = [16]i32;

var mat_mv: M = ident();
var mat_pj: M = ident();

fn ident() M {
    var m: M = [_]i32{0} ** 16;
    m[0] = ONE;
    m[5] = ONE;
    m[10] = ONE;
    m[15] = ONE;
    return m;
}

const ONE: i32 = 1 << 16; // 1.0 in 16.16
const FRAC_BITS: u5 = 16;

fn fx(v: f32) i32 {
    // soft-float-safe float->fixed (f32 mul by 65536 then trunc)
    return @intFromFloat(v * 65536.0);
}

// ─── context binding ───────────────────────────────────────────

/// Bind the target framebuffer.  Must be called before drawing.
pub export fn mgl_set_framebuffer(p: [*]u32, w: i32, h: i32) void {
    fb = p;
    fb_w = w;
    fb_h = h;
    viewport_x = 0;
    viewport_y = 0;
    viewport_w = w;
    viewport_h = h;
}

// ─── GL-style state calls ──────────────────────────────────────

export fn mgl_viewport(x: i32, y: i32, w: i32, h: i32) void {
    if (w <= 0 or h <= 0) return;
    viewport_x = x;
    viewport_y = y;
    viewport_w = w;
    viewport_h = h;
}

export fn mgl_clear_color(r: f32, g: f32, b: f32, a: f32) void {
    _ = a;
    clear_r = clamp8(r);
    clear_g = clamp8(g);
    clear_b = clamp8(b);
}

export fn mgl_clear() void {
    const p = fb orelse return;
    const c = pack_rgb(clear_r, clear_g, clear_b);
    if (fb_w <= 0 or fb_h <= 0) return;
    @memset(p[0..@intCast(fb_w * fb_h)], c);
}

pub export fn mgl_color3f(r: f32, g: f32, b: f32) void {
    cur_r = clamp8(r);
    cur_g = clamp8(g);
    cur_b = clamp8(b);
}

pub export fn mgl_matrix_mode(mode: i32) void {
    if (mode == GL_MODELVIEW or mode == GL_PROJECTION)
        matrix_mode = mode;
}

pub export fn mgl_load_identity() void {
    if (matrix_mode == GL_PROJECTION) mat_pj = ident() else mat_mv = ident();
}

/// 2D translation (z=0): post-multiplies the current matrix.
export fn mgl_translate_f(x: f32, y: f32) void {
    mul_translate(if (matrix_mode == GL_PROJECTION) &mat_pj else &mat_mv, fx(x), fx(y));
}

/// 2D scale: post-multiplies the current matrix.
export fn mgl_scale_f(x: f32, y: f32) void {
    mul_scale(if (matrix_mode == GL_PROJECTION) &mat_pj else &mat_mv, fx(x), fx(y));
}

fn active_mat() *M {
    return if (matrix_mode == GL_PROJECTION) &mat_pj else &mat_mv;
}

fn mul_translate(m: *M, tx: i32, ty: i32) void {
    // right column: m' = m * T; only affects the translation column
    m[12] = m[12] + (m[0] *% tx >> FRAC_BITS) +% (m[4] *% ty >> FRAC_BITS);
    m[13] = m[13] + (m[1] *% tx >> FRAC_BITS) +% (m[5] *% ty >> FRAC_BITS);
    m[14] = m[14] + (m[2] *% tx >> FRAC_BITS) +% (m[6] *% ty >> FRAC_BITS);
    m[15] = m[15] + (m[3] *% tx >> FRAC_BITS) +% (m[7] *% ty >> FRAC_BITS);
}

fn mul_scale(m: *M, sx: i32, sy: i32) void {
    for (0..4) |c| {
        m[c * 4 + 0] = (m[c * 4 + 0] *% sx) >> FRAC_BITS;
        m[c * 4 + 1] = (m[c * 4 + 1] *% sy) >> FRAC_BITS;
    }
}

/// Transform (x,y,1) by MODELVIEW then PROJECTION, then map to
/// viewport pixels.  Returns screen-space 16.16 fixed coords.
fn transform(x: f32, y: f32) struct { sx: i32, sy: i32 } {
    const vx = fx(x);
    const vy = fx(y);
    // MODELVIEW — every product runs in i64: matrix cells are up to
    // 65536 (1.0) and coords up to ~800 in 16.16 (~5.2e7), so the
    // i32 product (65536 * 32768 = 2^31) overflows exactly at unit
    // scale times ±0.5 NDC.  That silent wrap turned every vertex
    // into the first one's value.
    const mx0: i64 = (@as(i64, mat_mv[0]) * vx >> FRAC_BITS) + (@as(i64, mat_mv[4]) * vy >> FRAC_BITS) + mat_mv[12];
    const my0: i64 = (@as(i64, mat_mv[1]) * vx >> FRAC_BITS) + (@as(i64, mat_mv[5]) * vy >> FRAC_BITS) + mat_mv[13];
    // PROJECTION (uses the ORIGINAL modelview-transformed vector —
    // not the half-updated one)
    const mx: i64 = (@as(i64, mat_pj[0]) * mx0 >> FRAC_BITS) + (@as(i64, mat_pj[4]) * my0 >> FRAC_BITS) + mat_pj[12];
    const my: i64 = (@as(i64, mat_pj[1]) * mx0 >> FRAC_BITS) + (@as(i64, mat_pj[5]) * my0 >> FRAC_BITS) + mat_pj[13];
    // viewport map (NDC-ish [-1,1] → pixels; identity proj keeps as-is).
    // 64-bit: mx can be ±1.0 in 16.16 (±65536) and half_w is up to
    // 800<<16 ≈ 5.2e7 — the product ≈ 3.4e12 overflows i32.
    const half_w: i64 = @as(i64, viewport_w) * ONE >> 1;
    const half_h: i64 = @as(i64, viewport_h) * ONE >> 1;
    const cx: i64 = @as(i64, viewport_x) * ONE + half_w;
    const cy: i64 = @as(i64, viewport_y) * ONE + half_h;
    const sx: i64 = cx + ((mx * half_w) >> FRAC_BITS);
    const sy: i64 = cy - ((my * half_h) >> FRAC_BITS);
    return .{ .sx = @intCast(sx), .sy = @intCast(sy) };
}

// ─── immediate mode ────────────────────────────────────────────

var vert_buf: [MAX_VERTS]Vertex = undefined;
var vert_count: usize = 0;
var prim_mode: i32 = 0;
var in_begin: bool = false;

pub export fn mgl_begin(mode: i32) void {
    if (mode != GL_TRIANGLES and mode != GL_QUADS) return;
    prim_mode = mode;
    vert_count = 0;
    in_begin = true;
}

pub export fn mgl_vertex2f(x: f32, y: f32) void {
    if (!in_begin or vert_count >= MAX_VERTS) return;
    const t = transform(x, y);
    vert_buf[vert_count] = .{
        .x = t.sx,
        .y = t.sy,
        .r = cur_r,
        .g = cur_g,
        .b = cur_b,
    };
    vert_count += 1;
}

pub export fn mgl_end() void {
    if (!in_begin) return;
    in_begin = false;
    if (fb == null) return;

    switch (prim_mode) {
        GL_TRIANGLES => {
            var i: usize = 0;
            while (i + 3 <= vert_count) : (i += 3)
                raster_tri(&vert_buf[i], &vert_buf[i + 1], &vert_buf[i + 2]);
        },
        GL_QUADS => {
            var i: usize = 0;
            while (i + 4 <= vert_count) : (i += 4) {
                raster_tri(&vert_buf[i], &vert_buf[i + 1], &vert_buf[i + 2]);
                raster_tri(&vert_buf[i], &vert_buf[i + 2], &vert_buf[i + 3]);
            }
        },
        else => {},
    }
}

// ─── rasterizer ────────────────────────────────────────────────

/// Area (2x) in fixed-point; sign gives winding.  64-bit multiply:
/// fixed coords are ~65536*800 = 5e7 per axis, and the cross product
/// of two such deltas is ~2.5e15 — far past i32 range.
fn edge(ax: i32, ay: i32, bx: i32, by: i32, cx: i32, cy: i32) i32 {
    const ax64: i64 = ax;
    const ay64: i64 = ay;
    const w: i64 = (((@as(i64, bx) - ax64) * (@as(i64, cy) - ay64)) >> FRAC_BITS) -
        (((@as(i64, by) - ay64) * (@as(i64, cx) - ax64)) >> FRAC_BITS);
    return @intCast(w);
}

fn raster_tri(a: *const Vertex, b: *const Vertex, c: *const Vertex) void {
    const p = fb orelse return;
    if (fb_w <= 0 or fb_h <= 0) return;

    // integer pixel bounds from fixed coords, conservative
    var min_x = @min(@min(a.x, b.x), c.x) >> FRAC_BITS;
    var max_x = (@max(@max(a.x, b.x), c.x) +% (ONE - 1)) >> FRAC_BITS;
    var min_y = @min(@min(a.y, b.y), c.y) >> FRAC_BITS;
    var max_y = (@max(@max(a.y, b.y), c.y) +% (ONE - 1)) >> FRAC_BITS;

    if (min_x < viewport_x) min_x = viewport_x;
    if (min_y < viewport_y) min_y = viewport_y;
    if (max_x > viewport_x + viewport_w) max_x = viewport_x + viewport_w;
    if (max_y > viewport_y + viewport_h) max_y = viewport_y + viewport_h;
    if (min_x >= max_x or min_y >= max_y) return;

    const area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (area == 0) return;
    const inv_area: i32 = if (area > 0) 1 else -1;

    var py = min_y;
    while (py < max_y) : (py += 1) {
        var px = min_x;
        while (px < max_x) : (px += 1) {
            const fxp: i32 = px *% ONE; // pixel center-ish (top-left rule)
            const fyp: i32 = py *% ONE;
            var w0 = edge(b.x, b.y, c.x, c.y, fxp, fyp);
            var w1 = edge(c.x, c.y, a.x, a.y, fxp, fyp);
            var w2 = edge(a.x, a.y, b.x, b.y, fxp, fyp);
            if (inv_area < 0) {
                w0 = -%w0;
                w1 = -%w1;
                w2 = -%w2;
            }
            if (w0 >= 0 and w1 >= 0 and w2 >= 0) {
                // Gouraud interp: weights normalized by area.
                // |w| <= |area| (both are
                // 2x-triangle-area cross products of the same scale),
                // so w*255 needs at most 8 extra bits — fits i32.
                // i64 @divTrunc would emit __udivdi3, unavailable in
                // i386 freestanding.
                const a32: i32 = area *% inv_area; // |area|, > 0
                const w0n: u32 = @intCast(@divTrunc(w0 *% 255, a32));
                const w1n: u32 = @intCast(@divTrunc(w1 *% 255, a32));
                const w2n: u32 = @intCast(@divTrunc(w2 *% 255, a32));
                const r = (@as(u32, a.r) * w0n + @as(u32, b.r) * w1n + @as(u32, c.r) * w2n) / 255;
                const g = (@as(u32, a.g) * w0n + @as(u32, b.g) * w1n + @as(u32, c.g) * w2n) / 255;
                const bch = (@as(u32, a.b) * w0n + @as(u32, b.b) * w1n + @as(u32, c.b) * w2n) / 255;
                const idx = @as(usize, @intCast(py)) * @as(usize, @intCast(fb_w)) +
                    @as(usize, @intCast(px));
                p[idx] = pack_rgb(@intCast(r & 0xFF), @intCast(g & 0xFF), @intCast(bch & 0xFF));
            }
        }
    }
}

// ─── helpers ───────────────────────────────────────────────────

fn clamp8(v: f32) u8 {
    if (v <= 0.0) return 0;
    if (v >= 1.0) return 255;
    return @intFromFloat(v * 255.0);
}

fn pack_rgb(r: u8, g: u8, b: u8) u32 {
    return (@as(u32, r) << 16) | (@as(u32, g) << 8) | @as(u32, b);
}

// ─── self-test (native zig test) ───────────────────────────────

test "mgl clear + tri fill" {
    var buf: [16 * 16]u32 = undefined;
    @memset(&buf, 0);
    mgl_set_framebuffer(&buf, 16, 16);
    mgl_clear_color(0, 0, 0, 0);
    mgl_clear();
    // full-screen quad via NDC-ish coords: translate not used; the
    // identity projection maps [-1,1] → viewport
    mgl_matrix_mode(GL_MODELVIEW);
    mgl_load_identity();
    mgl_matrix_mode(GL_PROJECTION);
    mgl_load_identity();
    mgl_begin(GL_QUADS);
    mgl_color3f(1, 0, 0);
    mgl_vertex2f(-0.5, -0.5);
    mgl_vertex2f(0.5, -0.5);
    mgl_vertex2f(0.5, 0.5);
    mgl_vertex2f(-0.5, 0.5);
    mgl_end();
    // Sample (7,7): strictly inside tri 1 ((8,8) sits on the shared
    // quad diagonal).  Gouraud weights are computed with integer
    // truncating division, so a flat-color quad can land 1-3 counts
    // below 255 — assert a tight range, not exact equality.
    const px = buf[7 * 16 + 7];
    const r = (px >> 16) & 0xFF;
    try std.testing.expect(r >= 250);
    try std.testing.expectEqual(@as(u32, 0), px & 0x00FFFF);
    try std.testing.expectEqual(@as(u32, 0), buf[0]);
}
