#include "enhance/dithering/dithering.h"
#include "psx/error.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"

typedef struct {
    s32 x, y;
    u32 color[3], u, v;
} hm_vertex;
static s32 hm_poly_signed11(u32 value) {
    value &= 2047;
    return value < 1024 ? (s32)value : (s32)value - 2048;
}
static s64 hm_edge(const hm_vertex *a, const hm_vertex *b, s32 x, s32 y) {
    return (s64)(b->x - a->x) * (y - a->y) - (s64)(b->y - a->y) * (x - a->x);
}
static int hm_top_left(const hm_vertex *a, const hm_vertex *b) {
    return b->y < a->y || (b->y == a->y && b->x > a->x);
}
static u32 hm_interpolate(u32 a, u32 b, u32 c, const s64 weight[3], s64 area) {
    return (u32)((weight[0] * a + weight[1] * b + weight[2] * c + area / 2) / area);
}
static void hm_triangle(hm_vertex a, hm_vertex b, hm_vertex c, u32 opcode, u16 clut) {
    const DRAWENV *draw = &hm_gpu_environment_state()->draw;
    s64 area = hm_edge(&a, &b, c.x, c.y);
    if (!area)
        return;
    if (area < 0) {
        hm_vertex temporary = b;
        b = c;
        c = temporary;
        area = -area;
    }
    s32 minx = a.x, maxx = a.x, miny = a.y, maxy = a.y;
    const hm_vertex vertices[2] = {b, c};
    for (u32 i = 0; i < 2; ++i) {
        if (vertices[i].x < minx)
            minx = vertices[i].x;
        if (vertices[i].x > maxx)
            maxx = vertices[i].x;
        if (vertices[i].y < miny)
            miny = vertices[i].y;
        if (vertices[i].y > maxy)
            maxy = vertices[i].y;
    }
    if (maxx - minx > 2046 || maxy - miny > 1022)
        return;
    minx /= 2;
    maxx /= 2;
    miny /= 2;
    maxy /= 2;
    if (minx < 0)
        minx = 0;
    if (miny < 0)
        miny = 0;
    if (maxx > 1024)
        maxx = 1024;
    if (maxy > 512)
        maxy = 512;
    if (minx < draw->clip.x)
        minx = draw->clip.x;
    if (miny < draw->clip.y)
        miny = draw->clip.y;
    if (maxx > draw->clip.x + draw->clip.w)
        maxx = draw->clip.x + draw->clip.w;
    if (maxy > draw->clip.y + draw->clip.h)
        maxy = draw->clip.y + draw->clip.h;
    int edge0 = hm_top_left(&b, &c), edge1 = hm_top_left(&c, &a), edge2 = hm_top_left(&a, &b);
    static const s8 dither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    int textured = (opcode & 4) != 0, raw = textured && (opcode & 1);
    int dithering = hm_dither_enabled() && draw->dtd && ((opcode & 16) || (textured && !raw));
    for (s32 y = miny; y < maxy; ++y)
        for (s32 x = minx; x < maxx; ++x) {
            s32 px = x * 2 + 1, py = y * 2 + 1;
            s64 weight[3] = {hm_edge(&b, &c, px, py), hm_edge(&c, &a, px, py),
                             hm_edge(&a, &b, px, py)};
            if (weight[0] < 0 || (!weight[0] && !edge0) || weight[1] < 0 ||
                (!weight[1] && !edge1) || weight[2] < 0 || (!weight[2] && !edge2))
                continue;
            u16 texel = 0;
            if (textured) {
                u32 u = hm_interpolate(a.u, b.u, c.u, weight, area);
                u32 v = hm_interpolate(a.v, b.v, c.v, weight, area);
                texel = hm_gpu_texel(u, v, clut);
                if (!texel)
                    continue;
            }
            u16 color = raw ? texel : texel & 0x8000;
            if (!raw)
                for (u32 channel = 0; channel < 3; ++channel) {
                    s32 value = (s32)hm_interpolate(a.color[channel], b.color[channel],
                                                    c.color[channel], weight, area);
                    if (textured)
                        value = value * ((texel >> (channel * 5)) & 31) / 16;
                    if (dithering)
                        value += dither[y & 3][x & 3];
                    if (value < 0)
                        value = 0;
                    if (value > 255)
                        value = 255;
                    color |= (u16)((value >> 3) << (channel * 5));
                }
            hm_gpu_pixel(x, y, color, (opcode & 2) && (!textured || (texel & 0x8000)));
        }
}

u32 hm_gpu_polygon(const u32 *words, u32 available) {
    u32 opcode = words[0] >> 24, count = opcode & 8 ? 4 : 3;
    int textured = (opcode & 4) != 0, gouraud = (opcode & 16) != 0;
    u32 length = 1 + count + (textured ? count : 0) + (gouraud ? count - 1 : 0);
    if (available < length)
        return available;
    hm_vertex vertices[4];
    u32 at = 1, rgb = words[0];
    u16 clut = 0, page = 0;
    const DRAWENV *draw = &hm_gpu_environment_state()->draw;
    for (u32 i = 0; i < count; ++i) {
        if (i && gouraud)
            rgb = words[at++];
        u32 xy = words[at++];
        hm_vertex *vertex = vertices + i;
        vertex->x = (hm_poly_signed11(xy) + draw->ofs[0]) * 2;
        vertex->y = (hm_poly_signed11(xy >> 16) + draw->ofs[1]) * 2;
        for (u32 channel = 0; channel < 3; ++channel)
            vertex->color[channel] = (rgb >> (channel * 8)) & 255;
        vertex->u = vertex->v = 0;
        if (textured) {
            u32 uv = words[at++];
            vertex->u = uv & 255;
            vertex->v = (uv >> 8) & 255;
            if (!i)
                clut = (u16)(uv >> 16);
            if (i == 1)
                page = (u16)(uv >> 16);
        }
    }
    if (textured)
        hm_gpu_environment_command(0xE1000000u | (page & 511) | ((u32)draw->dtd << 9) |
                                   ((u32)draw->dfe << 10));
    hm_triangle(vertices[0], vertices[1], vertices[2], opcode, clut);
    if (count == 4)
        hm_triangle(vertices[1], vertices[2], vertices[3], opcode, clut);
    return length;
}
