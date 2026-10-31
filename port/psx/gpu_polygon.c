#include "enhance/dithering/dithering.h"
#include "enhance/pgxp/pgxp.h"
#include <math.h>
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
static u32 hm_first_set_tile(u64 mask) {
#if defined(__GNUC__) || defined(__clang__)
    return (u32)__builtin_ctzll(mask);
#else
    u32 index = 0;
    while (!(mask & 1)) {
        mask >>= 1;
        ++index;
    }
    return index;
#endif
}
static inline u32 hm_interpolate(u32 a, u32 b, u32 c, const s64 weight[3], s64 area, u64 inv_area) {
    s64 num = weight[0] * a + weight[1] * b + weight[2] * c + area / 2;
    if (num <= 0)
        return 0;
    return (u32)(((u64)num * inv_area) >> 32);
}
static void hm_triangle(hm_vertex a, hm_vertex b, hm_vertex c, u32 opcode, u16 clut) {
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const DRAWENV *draw = &state->draw;
    s64 area = hm_edge(&a, &b, c.x, c.y);
    if (!area)
        return;
    if (area < 0) {
        hm_vertex temporary = b;
        b = c;
        c = temporary;
        area = -area;
    }
    u64 inv_area = ((UINT64_C(1) << 32) + (u64)(area / 2)) / (u64)area;
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
    if (minx >= maxx || miny >= maxy)
        return;
    int filter_writes = hm_vram_write_filter_active();
    if (filter_writes && !hm_vram_write_filter_intersects(minx, miny, maxx, maxy))
        return;
    u64 filter_x_tiles = UINT64_MAX;
    if (filter_writes) {
        u32 first_tile = (u32)minx >> 4;
        u32 last_tile = ((u32)maxx - 1) >> 4;
        filter_x_tiles <<= first_tile;
        if (last_tile < 63)
            filter_x_tiles &= (UINT64_C(1) << (last_tile + 1)) - 1;
    }
    int edge0 = hm_top_left(&b, &c), edge1 = hm_top_left(&c, &a), edge2 = hm_top_left(&a, &b);
    static const s8 dither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    int textured = (opcode & 4) != 0, raw = textured && (opcode & 1), semi = (opcode & 2) != 0;
    u32 page = draw->tpage, depth = (page >> 7) & 3u;
    hm_vram_context *vram = hm_vram_mutable_state();
    u16 (*vram_pixels)[HM_VRAM_WIDTH] = vram->pixels;
    u16 set_mask = vram->set_mask, check_mask = vram->check_mask;
    u32 blend_mode = (page >> 5) & 3u;
    int check_disp = !draw->dfe && state->display_enabled;
    s32 disp_x0 = state->display.disp.x, disp_y0 = state->display.disp.y;
    s32 disp_x1 = disp_x0 + state->display.disp.w, disp_y1 = disp_y0 + state->display.disp.h;
    u32 page_x = (page & 15u) * 64u, page_y = (page & 16u) * 16u;
    u32 mask_x = (256u - (u32)draw->tw.w) & 248u;
    u32 mask_y = (256u - (u32)draw->tw.h) & 248u;
    u32 tw_x = (u32)draw->tw.x & mask_x, tw_y = (u32)draw->tw.y & mask_y;
    u32 clut_x = (clut & 63u) * 16u, clut_y = (clut >> 6) & 511u;
    int dithering = hm_dither_enabled() && draw->dtd && ((opcode & 16) || (textured && !raw));
    int uniform_color = !(opcode & 16) ||
                        (a.color[0] == b.color[0] && a.color[0] == c.color[0] &&
                         a.color[1] == b.color[1] && a.color[1] == c.color[1] &&
                         a.color[2] == b.color[2] && a.color[2] == c.color[2]);
    u16 flat_untextured =
        (u16)(((a.color[0] >> 3) & 31) | (((a.color[1] >> 3) & 31) << 5) |
              (((a.color[2] >> 3) & 31) << 10));
    s32 px0 = minx * 2 + 1, py0 = miny * 2 + 1;
    s64 row0 = hm_edge(&b, &c, px0, py0), row1 = hm_edge(&c, &a, px0, py0),
        row2 = hm_edge(&a, &b, px0, py0);
    s64 dx0 = -(s64)(c.y - b.y) * 2, dy0 = (s64)(c.x - b.x) * 2;
    s64 dx1 = -(s64)(a.y - c.y) * 2, dy1 = (s64)(a.x - c.x) * 2;
    s64 dx2 = -(s64)(b.y - a.y) * 2, dy2 = (s64)(b.x - a.x) * 2;
    for (s32 y = miny; y < maxy; ++y, row0 += dy0, row1 += dy1, row2 += dy2) {
        u64 allowed_tiles = hm_vram_write_filter_row((u32)y);
        if (filter_writes && !allowed_tiles)
            continue;
        if (filter_writes) {
            allowed_tiles &= filter_x_tiles;
            if (!allowed_tiles)
                continue;
        }
        s64 weight[3] = {row0, row1, row2};
        u16 *vram_row = vram_pixels[y];
        u64 dirty_mask = 0;
        int row_disp = check_disp && y >= disp_y0 && y < disp_y1;
        s32 x = minx;
        while (x < maxx) {
            s32 span_end = maxx;
            if (filter_writes) {
                u64 remaining_tiles = allowed_tiles & (UINT64_MAX << ((u32)x >> 4));
                if (!remaining_tiles)
                    break;
                u32 tile_x = hm_first_set_tile(remaining_tiles);
                s32 span_start = (s32)(tile_x << 4);
                if (span_start > x) {
                    s32 skipped = span_start - x;
                    x = span_start;
                    weight[0] += dx0 * skipped;
                    weight[1] += dx1 * skipped;
                    weight[2] += dx2 * skipped;
                    continue;
                }
                span_end = (s32)((tile_x + 1) << 4);
                if (span_end > maxx)
                    span_end = maxx;
            }
            for (; x < span_end; ++x, weight[0] += dx0, weight[1] += dx1,
                                       weight[2] += dx2) {
            if (weight[0] < 0 || (!weight[0] && !edge0) || weight[1] < 0 ||
                (!weight[1] && !edge1) || weight[2] < 0 || (!weight[2] && !edge2))
                continue;
            if (row_disp && x >= disp_x0 && x < disp_x1)
                continue;
            if (!textured && uniform_color && !dithering) {
                hm_gpu_fast_pixel(vram_row, &dirty_mask, x, flat_untextured, semi, blend_mode,
                                  set_mask, check_mask);
                continue;
            }
            u16 texel = 0;
            if (textured) {
                u32 u = hm_interpolate(a.u, b.u, c.u, weight, area, inv_area);
                u32 v = hm_interpolate(a.v, b.v, c.v, weight, area, inv_area);
                texel = hm_gpu_fast_texel((const u16(*)[HM_VRAM_WIDTH])vram_pixels, page_x, page_y,
                                          depth, mask_x, mask_y, tw_x, tw_y, clut_x, clut_y, u, v);
                if (!texel)
                    continue;
            }
            u16 color = raw ? texel : texel & 0x8000;
            if (!raw)
                for (u32 channel = 0; channel < 3; ++channel) {
                    s32 value = uniform_color ? (s32)a.color[channel]
                                              : (s32)hm_interpolate(a.color[channel], b.color[channel],
                                                                    c.color[channel], weight, area, inv_area);
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
            hm_gpu_fast_pixel(vram_row, &dirty_mask, x, color, semi && (!textured || (texel & 0x8000)),
                              blend_mode, set_mask, check_mask);
            }
        }
        vram->dirty[(u32)y >> 4] |= dirty_mask;
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
#if defined(HMBTN_ENH_PGXP) && HMBTN_ENH_PGXP
        float fx, fy, fz;
        if (hm_pgxp_lookup_vertex(&words[at - 1], xy, &fx, &fy, &fz)) {
            vertex->x = (s32)roundf((fx + (float)draw->ofs[0]) * 2.0f);
            vertex->y = (s32)roundf((fy + (float)draw->ofs[1]) * 2.0f);
        } else {
            vertex->x = (hm_poly_signed11(xy) + draw->ofs[0]) * 2;
            vertex->y = (hm_poly_signed11(xy >> 16) + draw->ofs[1]) * 2;
        }
#else
        vertex->x = (hm_poly_signed11(xy) + draw->ofs[0]) * 2;
        vertex->y = (hm_poly_signed11(xy >> 16) + draw->ofs[1]) * 2;
#endif
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
