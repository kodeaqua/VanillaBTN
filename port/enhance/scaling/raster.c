#include "enhance/scaling/raster.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    s64 x, y;
    s32 lx, ly;
    u32 rgb[3], u, v;
} hm_scaled_vertex;
static int hm_scaled_signed11(u32 n) { return (int)((n & 2047) ^ 1024) - 1024; }
static int hm_scaled_clamp(int n, int max) { return n < 0 ? 0 : n > max ? max : n; }
static s64 hm_scaled_floor_div(s64 n, s64 d) { return n >= 0 ? n / d : -((-n + d - 1) / d); }
static int hm_scaled_logical_x(const hm_scaled_surface *s, int x) { return s->logical_x[x]; }
static int hm_scaled_logical_y(const hm_scaled_surface *s, int y) { return s->logical_y[y]; }
static void hm_scaled_put(hm_scaled_surface *s, int x, int y, u16 color, int blend) {
    const hm_gpu_environment *env = s->environment;
    const DRAWENV *d = &env->draw;
    int lx = hm_scaled_logical_x(s, x), ly = hm_scaled_logical_y(s, y);
    if (lx < d->clip.x || ly < d->clip.y || lx >= d->clip.x + d->clip.w ||
        ly >= d->clip.y + d->clip.h)
        return;
    const RECT *display = &env->display.disp;
    if (!d->dfe && env->display_enabled && lx >= display->x && ly >= display->y &&
        lx < display->x + display->w && ly < display->y + display->h)
        return;
    u16 *out = &s->pixels[(size_t)y * s->width + x];
    const hm_vram_context *vram = s->vram;
    if (vram->check_mask && (*out & 0x8000))
        return;
    if (blend) {
        u16 result = color & 0x8000;
        int mode = (d->tpage >> 5) & 3;
        for (int shift = 0; shift < 15; shift += 5) {
            int back = (*out >> shift) & 31, front = (color >> shift) & 31;
            int value = mode == 0   ? back / 2 + front / 2
                        : mode == 1 ? back + front
                        : mode == 2 ? back - front
                                    : back + front / 4;
            result |= (u16)(hm_scaled_clamp(value, 31) << shift);
        }
        color = result;
    }
    *out = color | vram->set_mask;
}
static void hm_scaled_begin(void *context, const RECT *area) {
    hm_scaled_surface *s = context;
    s->begun = 0;
    if (!s->pixels || !s->width || !s->height || s->width > 4096 || s->height > 4096 ||
        area->x < 0 || area->y < 0 || area->w <= 0 || area->h <= 0 || area->x + area->w > 1024 ||
        area->y + area->h > 512)
        return;
    s->area = *area;
    ++s->revision;
    s->environment = hm_gpu_environment_state();
    s->vram = hm_vram_state();
    for (u32 x = 0; x < s->width; ++x)
        s->logical_x[x] = area->x + (int)(((u64)(2 * x + 1) * (u32)area->w) / (2 * s->width));
    for (u32 y = 0; y < s->height; ++y)
        s->logical_y[y] = area->y + (int)(((u64)(2 * y + 1) * (u32)area->h) / (2 * s->height));
    for (u32 y = 0; y < s->height; ++y)
        for (u32 x = 0; x < s->width; ++x)
            s->pixels[(size_t)y * s->width + x] = hm_vram_pixel(
                (u32)hm_scaled_logical_x(s, (int)x), (u32)hm_scaled_logical_y(s, (int)y));
    s->begun = 1;
}
static s64 hm_scaled_edge(hm_scaled_vertex a, hm_scaled_vertex b, s64 x, s64 y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
static int hm_scaled_top_left(hm_scaled_vertex a, hm_scaled_vertex b) {
    return b.y < a.y || (b.y == a.y && b.x > a.x);
}
static u32 hm_scaled_interpolate(u32 a, u32 b, u32 c, const s64 w[3], s64 area) {
    if (area > INT64_MAX / (3 * 255))
        return (u32)(((double)w[0] * a + (double)w[1] * b + (double)w[2] * c) / (double)area + 0.5);
    return (u32)((w[0] * a + w[1] * b + w[2] * c + area / 2) / area);
}
static void hm_scaled_triangle(hm_scaled_surface *s, hm_scaled_vertex a, hm_scaled_vertex b,
                               hm_scaled_vertex c, u32 op, u16 clut) {
    int lx0 = a.lx, lx1 = a.lx, ly0 = a.ly, ly1 = a.ly;
    hm_scaled_vertex source[2] = {b, c};
    for (int i = 0; i < 2; ++i) {
        if (source[i].lx < lx0)
            lx0 = source[i].lx;
        if (source[i].lx > lx1)
            lx1 = source[i].lx;
        if (source[i].ly < ly0)
            ly0 = source[i].ly;
        if (source[i].ly > ly1)
            ly1 = source[i].ly;
    }
    if (lx1 - lx0 > 1023 || ly1 - ly0 > 511)
        return;
    s64 area = hm_scaled_edge(a, b, c.x, c.y);
    if (!area)
        return;
    if (area < 0) {
        hm_scaled_vertex tmp = b;
        b = c;
        c = tmp;
        area = -area;
    }
    s64 minx = a.x, maxx = a.x, miny = a.y, maxy = a.y;
    hm_scaled_vertex others[2] = {b, c};
    for (int i = 0; i < 2; ++i) {
        if (others[i].x < minx)
            minx = others[i].x;
        if (others[i].x > maxx)
            maxx = others[i].x;
        if (others[i].y < miny)
            miny = others[i].y;
        if (others[i].y > maxy)
            maxy = others[i].y;
    }
    int x0 = hm_scaled_clamp((int)hm_scaled_floor_div(minx, 256), (int)s->width),
        x1 = hm_scaled_clamp((int)hm_scaled_floor_div(maxx + 255, 256), (int)s->width);
    int y0 = hm_scaled_clamp((int)hm_scaled_floor_div(miny, 256), (int)s->height),
        y1 = hm_scaled_clamp((int)hm_scaled_floor_div(maxy + 255, 256), (int)s->height);
    int edges[3] = {hm_scaled_top_left(b, c), hm_scaled_top_left(c, a), hm_scaled_top_left(a, b)};
    int textured = (op & 4) != 0, raw = textured && (op & 1);
    int flat = !textured && a.rgb[0] == b.rgb[0] && a.rgb[0] == c.rgb[0] && a.rgb[1] == b.rgb[1] &&
               a.rgb[1] == c.rgb[1] && a.rgb[2] == b.rgb[2] && a.rgb[2] == c.rgb[2];
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    flat = flat && (!d->dtd || !(op & 16));
    u16 flat_color = (u16)((a.rgb[0] >> 3) | ((a.rgb[1] >> 3) << 5) | ((a.rgb[2] >> 3) << 10));
    static const s8 dither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            s64 px = (s64)x * 256 + 128, py = (s64)y * 256 + 128;
            s64 w[3] = {hm_scaled_edge(b, c, px, py), hm_scaled_edge(c, a, px, py),
                        hm_scaled_edge(a, b, px, py)};
            if (w[0] < 0 || (!w[0] && !edges[0]) || w[1] < 0 || (!w[1] && !edges[1]) || w[2] < 0 ||
                (!w[2] && !edges[2]))
                continue;
            if (flat) {
                hm_scaled_put(s, x, y, flat_color, (op & 2) != 0);
                continue;
            }
            u16 texel = 0;
            if (textured) {
                texel = hm_gpu_texel(hm_scaled_interpolate(a.u, b.u, c.u, w, area),
                                     hm_scaled_interpolate(a.v, b.v, c.v, w, area), clut);
                if (!texel)
                    continue;
            }
            u16 color = raw ? texel : texel & 0x8000;
            if (!raw)
                for (int channel = 0; channel < 3; ++channel) {
                    int value = (int)hm_scaled_interpolate(a.rgb[channel], b.rgb[channel],
                                                           c.rgb[channel], w, area);
                    if (textured)
                        value = value * ((texel >> (channel * 5)) & 31) / 16;
                    if (d->dtd && ((op & 16) || (textured && !raw)))
                        value +=
                            dither[hm_scaled_logical_y(s, y) & 3][hm_scaled_logical_x(s, x) & 3];
                    color |= (u16)((hm_scaled_clamp(value, 255) >> 3) << (channel * 5));
                }
            hm_scaled_put(s, x, y, color, (op & 2) && (!textured || (texel & 0x8000)));
        }
}
static hm_scaled_vertex hm_scaled_make_vertex(hm_scaled_surface *s, int x, int y, u32 rgb, u32 uv) {
    hm_scaled_vertex v = {
        .x = hm_scaled_floor_div((s64)(x - s->area.x) * s->width * 256, s->area.w),
        .y = hm_scaled_floor_div((s64)(y - s->area.y) * s->height * 256, s->area.h),
        .lx = x,
        .ly = y,
        .u = uv & 255,
        .v = (uv >> 8) & 255};
    for (int i = 0; i < 3; ++i)
        v.rgb[i] = (rgb >> (i * 8)) & 255;
    return v;
}
static void hm_scaled_polygon(hm_scaled_surface *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, n = op & 8 ? 4 : 3;
    int textured = (op & 4) != 0, shaded = (op & 16) != 0;
    if (1 + n + (textured ? n : 0) + (shaded ? n - 1 : 0) > available)
        return;
    hm_scaled_vertex v[4];
    u32 at = 1, rgb = w[0];
    u16 clut = 0;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    for (u32 i = 0; i < n; ++i) {
        if (i && shaded)
            rgb = w[at++];
        u32 xy = w[at++], uv = textured ? w[at++] : 0;
        int x = hm_scaled_signed11(xy) + d->ofs[0], y = hm_scaled_signed11(xy >> 16) + d->ofs[1];
        v[i] = hm_scaled_make_vertex(s, x, y, rgb, uv);
        if (textured && !i)
            clut = (u16)(uv >> 16);
        if (textured && i == 1)
            hm_gpu_environment_command(0xe1000000u | ((uv >> 16) & 511) | ((u32)d->dtd << 9) |
                                       ((u32)d->dfe << 10));
    }
    hm_scaled_triangle(s, v[0], v[1], v[2], op, clut);
    if (n == 4)
        hm_scaled_triangle(s, v[1], v[2], v[3], op, clut);
}
static void hm_scaled_rectangle(hm_scaled_surface *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, size = (op >> 3) & 3;
    int textured = (op & 4) != 0;
    u32 length = 2 + (u32)textured + (size == 0);
    if (length > available)
        return;
    int width = size == 1 ? 1 : size == 2 ? 8 : 16, height = width;
    if (!size) {
        width = w[length - 1] & 1023;
        height = (w[length - 1] >> 16) & 511;
    }
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int lx = hm_scaled_signed11(w[1]) + d->ofs[0], ly = hm_scaled_signed11(w[1] >> 16) + d->ofs[1];
    hm_scaled_vertex a = hm_scaled_make_vertex(s, lx, ly, w[0], 0),
                     b = hm_scaled_make_vertex(s, lx + width, ly + height, w[0], 0);
    int x0 = hm_scaled_clamp((int)hm_scaled_floor_div(a.x, 256), (int)s->width),
        x1 = hm_scaled_clamp((int)hm_scaled_floor_div(b.x + 255, 256), (int)s->width);
    int y0 = hm_scaled_clamp((int)hm_scaled_floor_div(a.y, 256), (int)s->height),
        y1 = hm_scaled_clamp((int)hm_scaled_floor_div(b.y + 255, 256), (int)s->height);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            int dx = hm_scaled_logical_x(s, x) - lx, dy = hm_scaled_logical_y(s, y) - ly;
            if (dx < 0 || dy < 0 || dx >= width || dy >= height)
                continue;
            u16 texel = 0, color = 0;
            if (textured) {
                texel = hm_gpu_texel((w[2] & 255) + (u32)dx, ((w[2] >> 8) & 255) + (u32)dy,
                                     (u16)(w[2] >> 16));
                if (!texel)
                    continue;
            }
            if (textured && (op & 1))
                color = texel;
            else {
                color = texel & 0x8000;
                for (int c = 0; c < 3; ++c) {
                    int value = (w[0] >> (c * 8)) & 255;
                    if (textured)
                        value = hm_scaled_clamp(((texel >> (c * 5)) & 31) * value / 128, 31);
                    else
                        value >>= 3;
                    color |= (u16)(value << (c * 5));
                }
            }
            hm_scaled_put(s, x, y, color, (op & 2) && (!textured || (texel & 0x8000)));
        }
}
static void hm_scaled_line_segment(hm_scaled_surface *s, u32 xy0, u32 xy1, u32 c0, u32 c1, u32 op,
                                   int skip) {
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int nx0 = hm_scaled_signed11(xy0), ny0 = hm_scaled_signed11(xy0 >> 16),
        nx1 = hm_scaled_signed11(xy1), ny1 = hm_scaled_signed11(xy1 >> 16);
    if (abs(nx1 - nx0) > 1023 || abs(ny1 - ny0) > 511)
        return;
    /* A native line has one logical-pixel thickness. Rasterize its covered
     * cells at output resolution to preserve UI borders and polyline joins. */
    int dx = abs(nx1 - nx0), dy = abs(ny1 - ny0), sx = nx0 < nx1 ? 1 : -1, sy = ny0 < ny1 ? 1 : -1,
        error = dx - dy;
    int steps = dx > dy ? dx : dy;
    for (int at = 0;; ++at) {
        if (!skip || at) {
            u32 packet[3] = {0x60000000u | (op & 2 ? 0x02000000u : 0),
                             (u16)nx0 | ((u32)(u16)ny0 << 16), 0x00010001};
            for (int c = 0; c < 3; ++c) {
                int a = (c0 >> (c * 8)) & 255, b = (c1 >> (c * 8)) & 255;
                int value = steps ? (a * (steps - at) + b * at + steps / 2) / steps : a;
                static const s8 dither[4][4] = {
                    {-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
                if (d->dtd)
                    value = hm_scaled_clamp(
                        value + dither[(ny0 + d->ofs[1]) & 3][(nx0 + d->ofs[0]) & 3], 255);
                packet[0] |= (u32)value << (c * 8);
            }
            hm_scaled_rectangle(s, packet, 3);
        }
        if (nx0 == nx1 && ny0 == ny1)
            break;
        int doubled = 2 * error;
        if (doubled > -dy) {
            error -= dy;
            nx0 += sx;
        }
        if (doubled < dx) {
            error += dx;
            ny0 += sy;
        }
    }
}
static void hm_scaled_line(hm_scaled_surface *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24;
    int shaded = (op & 16) != 0, poly = (op & 8) != 0;
    if (available < (shaded ? 4u : 3u))
        return;
    u32 at = 2, xy = w[1], color = w[0];
    int skip = 0;
    while (at < available) {
        if (poly && skip && (w[at] & 0xf000f000u) == 0x50005000u)
            return;
        u32 next = shaded ? w[at++] : color;
        if (at >= available)
            return;
        u32 point = w[at++];
        hm_scaled_line_segment(s, xy, point, color, next, op, skip);
        if (!poly)
            return;
        xy = point;
        color = next;
        skip = 1;
    }
}
static void hm_scaled_command(void *context, const u32 *w, u32 available, int after) {
    hm_scaled_surface *s = context;
    if (!s->begun || !available)
        return;
    u32 op = w[0] >> 24;
    if (!after) {
        if (op >= 0x20 && op <= 0x3f)
            hm_scaled_polygon(s, w, available);
        else if (op >= 0x60 && op <= 0x7f)
            hm_scaled_rectangle(s, w, available);
        else if (op >= 0x40 && op <= 0x5f)
            hm_scaled_line(s, w, available);
    } else if ((op == 2 || op == 0xa0) && available >= 3) {
        int rx = (s16)w[1], ry = (s16)(w[1] >> 16), rw = (s16)w[2], rh = (s16)(w[2] >> 16);
        if (op == 0xa0) {
            rx &= 1023;
            ry &= 511;
            rw = (((u32)rw - 1) & 1023) + 1;
            rh = (((u32)rh - 1) & 511) + 1;
        } else {
            rx &= 0x3f0;
            ry &= 511;
            rw = (((u32)rw & 1023) + 15) & ~15u;
            rh &= 511;
        }
        for (u32 y = 0; y < s->height; ++y)
            for (u32 x = 0; x < s->width; ++x) {
                int lx = hm_scaled_logical_x(s, (int)x), ly = hm_scaled_logical_y(s, (int)y);
                if ((((u32)(lx - rx)) & 1023) < (u32)rw && (((u32)(ly - ry)) & 511) < (u32)rh)
                    s->pixels[(size_t)y * s->width + x] = hm_vram_pixel((u32)lx, (u32)ly);
            }
    }
}
hm_replay_sink hm_scaled_sink(hm_scaled_surface *s) {
    return (hm_replay_sink){s, hm_scaled_begin, hm_scaled_command, NULL, NULL};
}
