#include "enhance/dithering/dithering.h"
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
#include "gfx/software_raster.h"
#ifdef HMBTN_ENH_FILTERING
#include "enhance/filtering/sample.h"
#endif
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

enum { HM_SW_THREADS = 8 };
typedef void (*hm_sw_row_fn)(const void *ctx, int y0, int y1);
static struct {
    pthread_t threads[HM_SW_THREADS - 1];
    pthread_mutex_t mutex;
    pthread_cond_t start_cv, done_cv;
    const void *ctx;
    hm_sw_row_fn fn;
    int y0, y1;
    atomic_int next_y, active;
    u32 generation;
    int initialized;
} hm_sw_pool;

static void hm_sw_work_chunk(const void *ctx, hm_sw_row_fn fn, int y0, int y1) {
    for (;;) {
        int start = atomic_fetch_add_explicit(&hm_sw_pool.next_y, 16, memory_order_relaxed);
        if (start >= y1)
            break;
        int end = start + 16 < y1 ? start + 16 : y1;
        if (start >= y0)
            fn(ctx, start, end);
    }
}

static void *hm_sw_worker(void *arg) {
    (void)arg;
    u32 seen = 0;
    for (;;) {
        pthread_mutex_lock(&hm_sw_pool.mutex);
        while (hm_sw_pool.generation == seen)
            pthread_cond_wait(&hm_sw_pool.start_cv, &hm_sw_pool.mutex);
        seen = hm_sw_pool.generation;
        const void *ctx = hm_sw_pool.ctx;
        hm_sw_row_fn fn = hm_sw_pool.fn;
        int y0 = hm_sw_pool.y0, y1 = hm_sw_pool.y1;
        pthread_mutex_unlock(&hm_sw_pool.mutex);
        hm_sw_work_chunk(ctx, fn, y0, y1);
        if (atomic_fetch_sub_explicit(&hm_sw_pool.active, 1, memory_order_acq_rel) == 1) {
            pthread_mutex_lock(&hm_sw_pool.mutex);
            pthread_cond_signal(&hm_sw_pool.done_cv);
            pthread_mutex_unlock(&hm_sw_pool.mutex);
        }
    }
    return NULL;
}

static void hm_sw_parallel_rows(int y0, int y1, int width_span, hm_sw_row_fn fn, const void *ctx) {
    if (y0 >= y1)
        return;
    if ((s64)(y1 - y0) * width_span < 65536 || (y1 - y0) < 32) {
        fn(ctx, y0, y1);
        return;
    }
    if (!hm_sw_pool.initialized) {
        pthread_mutex_init(&hm_sw_pool.mutex, NULL);
        pthread_cond_init(&hm_sw_pool.start_cv, NULL);
        pthread_cond_init(&hm_sw_pool.done_cv, NULL);
        for (int i = 0; i < HM_SW_THREADS - 1; ++i) {
            pthread_create(&hm_sw_pool.threads[i], NULL, hm_sw_worker, NULL);
            pthread_detach(hm_sw_pool.threads[i]);
        }
        hm_sw_pool.initialized = 1;
    }
    pthread_mutex_lock(&hm_sw_pool.mutex);
    hm_sw_pool.ctx = ctx;
    hm_sw_pool.fn = fn;
    hm_sw_pool.y0 = y0;
    hm_sw_pool.y1 = y1;
    atomic_store_explicit(&hm_sw_pool.next_y, y0, memory_order_relaxed);
    atomic_store_explicit(&hm_sw_pool.active, HM_SW_THREADS, memory_order_relaxed);
    ++hm_sw_pool.generation;
    pthread_cond_broadcast(&hm_sw_pool.start_cv);
    pthread_mutex_unlock(&hm_sw_pool.mutex);
    hm_sw_work_chunk(ctx, fn, y0, y1);
    if (atomic_fetch_sub_explicit(&hm_sw_pool.active, 1, memory_order_acq_rel) != 1) {
        pthread_mutex_lock(&hm_sw_pool.mutex);
        while (atomic_load_explicit(&hm_sw_pool.active, memory_order_acquire) > 0)
            pthread_cond_wait(&hm_sw_pool.done_cv, &hm_sw_pool.mutex);
        pthread_mutex_unlock(&hm_sw_pool.mutex);
    }
}

typedef struct {
    s64 x, y;
    s32 lx, ly;
    u32 rgb[3], u, v;
} hm_software_vertex;
static int hm_software_signed11(u32 n) { return (int)((n & 2047) ^ 1024) - 1024; }
static int hm_software_clamp(int n, int max) { return n < 0 ? 0 : n > max ? max : n; }
static s64 hm_software_floor_div(s64 n, s64 d) { return n >= 0 ? n / d : -((-n + d - 1) / d); }
static int hm_software_logical_x(const hm_software_surface *s, int x) { return s->logical_x[x]; }
static int hm_software_logical_y(const hm_software_surface *s, int y) { return s->logical_y[y]; }
static void hm_software_put(hm_software_surface *s, int x, int y, u16 color, int blend) {
    const hm_gpu_environment *env = s->environment;
    const DRAWENV *d = &env->draw;
    int lx = hm_software_logical_x(s, x), ly = hm_software_logical_y(s, y);
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
            result |= (u16)(hm_software_clamp(value, 31) << shift);
        }
        color = result;
    }
    *out = color | vram->set_mask;
}
typedef struct {
    hm_software_surface *s;
    RECT native;
    u16 background;
} hm_sw_begin_job;

static void hm_software_begin_rows(const void *ctx, int y0, int y1) {
    const hm_sw_begin_job *job = ctx;
    hm_software_surface *s = job->s;
    RECT native = job->native;
    u16 background = job->background;
    for (int y = y0; y < y1; ++y) {
        const u16 *vram_row = s->vram->pixels[(u32)s->logical_y[y] & 511];
        u16 *dst_row = &s->pixels[(size_t)y * s->width];
        for (u32 x = 0; x < s->width; ++x) {
            int lx = s->logical_x[x];
            dst_row[x] = (lx < native.x || lx >= native.x + native.w)
                             ? background
                             : vram_row[(u32)lx & 1023];
        }
    }
}

static void hm_software_begin(void *context, const RECT *area) {
    hm_software_surface *s = context;
    s->begun = 0;
    int expanded = 0;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT expected = hm_widescreen_area(hm_gpu_environment_state()->draw.clip);
    expanded = area->x == expected.x && area->y == expected.y && area->w == expected.w &&
               area->h == expected.h;
#endif
    if (!expanded && (area->x < 0 || area->x + area->w > 1024))
        return;
    if (!s->pixels || !s->width || !s->height || s->width > 4096 || s->height > 4096 ||
        area->x < -352 || area->y < 0 || area->w <= 0 || area->w > 1024 || area->h <= 0 ||
        area->x + area->w > 1376 || area->y + area->h > 512)
        return;
    s->area = *area;
    ++s->revision;
    s->environment = hm_gpu_environment_state();
    s->vram = hm_vram_state();
    for (u32 x = 0; x < s->width; ++x) {
        s->logical_x[x] = area->x + (int)(((u64)(2 * x + 1) * (u32)area->w) / (2 * s->width));
        s->q8_x[x] = (s32)(((s64)(2 * x + 1) * area->w * 128 + s->width / 2) / s->width) +
                     area->x * 256 - 128;
    }
    for (u32 y = 0; y < s->height; ++y) {
        s->logical_y[y] = area->y + (int)(((u64)(2 * y + 1) * (u32)area->h) / (2 * s->height));
        s->q8_y[y] = (s32)(((s64)(2 * y + 1) * area->h * 128 + s->height / 2) / s->height) +
                     area->y * 256 - 128;
    }
    hm_sw_begin_job job = {
        .s = s,
        .native = hm_gpu_environment_state()->draw.clip,
        .background = hm_gpu_background(&s->environment->draw)};
    hm_sw_parallel_rows(0, (int)s->height, (int)s->width, hm_software_begin_rows, &job);
    s->begun = 1;
}
static s64 hm_software_edge(hm_software_vertex a, hm_software_vertex b, s64 x, s64 y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
static int hm_software_top_left(hm_software_vertex a, hm_software_vertex b) {
    return b.y < a.y || (b.y == a.y && b.x > a.x);
}
static u32 hm_software_interpolate(u32 a, u32 b, u32 c, const s64 w[3], s64 area) {
    if (area > INT64_MAX / (3 * 255))
        return (u32)(((double)w[0] * a + (double)w[1] * b + (double)w[2] * c) / (double)area + 0.5);
    return (u32)((w[0] * a + w[1] * b + w[2] * c + area / 2) / area);
}
#ifdef HMBTN_ENH_FILTERING
static s32 hm_software_uv_q8(u32 a, u32 b, u32 c, const s64 w[3], s64 area) {
    if (area > INT64_MAX / (3 * 255))
        return (s32)(((double)w[0] * a + (double)w[1] * b + (double)w[2] * c) / (double)area * 256 +
                     0.5);
    s64 numerator = w[0] * a + w[1] * b + w[2] * c, whole = numerator / area,
        rest = numerator % area;
    return (s32)(whole * 256 + (rest * 256 + area / 2) / area);
}
#endif
typedef struct {
    hm_software_surface *s;
    hm_software_vertex a, b, c;
    s64 area;
    int x0, x1;
    int edges[3];
    int textured, raw, flat, uniform_rgb, dither_active;
    u32 op;
    u16 clut, flat_color;
    const DRAWENV *d;
#ifdef HMBTN_ENH_FILTERING
    hm_texture_bounds bounds;
#endif
} hm_sw_tri_job;

static void hm_software_triangle_rows(const void *ctx, int y0, int y1) {
    const hm_sw_tri_job *job = ctx;
    hm_software_surface *s = job->s;
    const hm_software_vertex a = job->a, b = job->b, c = job->c;
    s64 area = job->area;
    int x0 = job->x0, x1 = job->x1;
    const int *edges = job->edges;
    int textured = job->textured, raw = job->raw, flat = job->flat,
        uniform_rgb = job->uniform_rgb, dither_active = job->dither_active;
    u32 op = job->op;
    u16 clut = job->clut, flat_color = job->flat_color;
    const DRAWENV *d = job->d;
    static const s8 dither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    s64 px0 = (s64)x0 * 256 + 128, py0 = (s64)y0 * 256 + 128;
    s64 row0 = hm_software_edge(b, c, px0, py0), row1 = hm_software_edge(c, a, px0, py0),
        row2 = hm_software_edge(a, b, px0, py0);
    s64 dx0 = -(c.y - b.y) * 256, dy0 = (c.x - b.x) * 256;
    s64 dx1 = -(a.y - c.y) * 256, dy1 = (a.x - c.x) * 256;
    s64 dx2 = -(b.y - a.y) * 256, dy2 = (b.x - a.x) * 256;
    for (int y = y0; y < y1; ++y, row0 += dy0, row1 += dy1, row2 += dy2) {
        int ly = s->logical_y[y];
        if (ly < d->clip.y || ly >= d->clip.y + d->clip.h)
            continue;
        s64 w[3] = {row0, row1, row2};
        for (int x = x0; x < x1; ++x, w[0] += dx0, w[1] += dx1, w[2] += dx2) {
            if (w[0] < 0 || (!w[0] && !edges[0]) || w[1] < 0 || (!w[1] && !edges[1]) || w[2] < 0 ||
                (!w[2] && !edges[2]))
                continue;
            if (flat) {
                hm_software_put(s, x, y, flat_color, (op & 2) != 0);
                continue;
            }
#ifdef HMBTN_ENH_FILTERING
            if (textured && s->texture_linear) {
                hm_texture_shading shading = {.opcode = op,
                                              .rectangle = 0,
                                              .dither = d->dtd,
                                              .x = s->logical_x[x],
                                              .y = ly,
                                              .background = s->pixels[(size_t)y * s->width + x]};
                for (int channel = 0; channel < 3; ++channel)
                    shading.light[channel] =
                        uniform_rgb ? a.rgb[channel]
                                    : hm_software_interpolate(a.rgb[channel], b.rgb[channel],
                                                              c.rgb[channel], w, area);
                u16 filtered;
                if (hm_texture_bilinear(hm_software_uv_q8(a.u, b.u, c.u, w, area),
                                        hm_software_uv_q8(a.v, b.v, c.v, w, area), job->bounds,
                                        clut, &shading, &filtered))
                    hm_software_put(s, x, y, filtered, 0);
                continue;
            }
#endif
            u16 texel = 0;
            if (textured) {
                texel = hm_gpu_texel(hm_software_interpolate(a.u, b.u, c.u, w, area),
                                     hm_software_interpolate(a.v, b.v, c.v, w, area), clut);
                if (!texel)
                    continue;
            }
            u16 color = raw ? texel : texel & 0x8000;
            if (!raw)
                for (int channel = 0; channel < 3; ++channel) {
                    int value = uniform_rgb
                                    ? (int)a.rgb[channel]
                                    : (int)hm_software_interpolate(a.rgb[channel], b.rgb[channel],
                                                                   c.rgb[channel], w, area);
                    if (textured)
                        value = value * ((texel >> (channel * 5)) & 31) / 16;
                    if (dither_active)
                        value += dither[ly & 3][s->logical_x[x] & 3];
                    color |= (u16)((hm_software_clamp(value, 255) >> 3) << (channel * 5));
                }
            hm_software_put(s, x, y, color, (op & 2) && (!textured || (texel & 0x8000)));
        }
    }
}

static void hm_software_triangle(hm_software_surface *s, hm_software_vertex a, hm_software_vertex b,
                                 hm_software_vertex c, u32 op, u16 clut) {
    int lx0 = a.lx, lx1 = a.lx, ly0 = a.ly, ly1 = a.ly;
    hm_software_vertex source[2] = {b, c};
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
    s64 area = hm_software_edge(a, b, c.x, c.y);
    if (!area)
        return;
    if (area < 0) {
        hm_software_vertex tmp = b;
        b = c;
        c = tmp;
        area = -area;
    }
    s64 minx = a.x, maxx = a.x, miny = a.y, maxy = a.y;
    hm_software_vertex others[2] = {b, c};
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
    int x0 = hm_software_clamp((int)hm_software_floor_div(minx, 256), (int)s->width),
        x1 = hm_software_clamp((int)hm_software_floor_div(maxx + 255, 256), (int)s->width);
    int y0 = hm_software_clamp((int)hm_software_floor_div(miny, 256), (int)s->height),
        y1 = hm_software_clamp((int)hm_software_floor_div(maxy + 255, 256), (int)s->height);
    if (x0 >= x1 || y0 >= y1)
        return;
    int textured = (op & 4) != 0, raw = textured && (op & 1);
    int uniform_rgb = !(op & 16) ||
                      (a.rgb[0] == b.rgb[0] && a.rgb[0] == c.rgb[0] && a.rgb[1] == b.rgb[1] &&
                       a.rgb[1] == c.rgb[1] && a.rgb[2] == b.rgb[2] && a.rgb[2] == c.rgb[2]);
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int dither_active = hm_dither_enabled() && d->dtd && ((op & 16) || (textured && !raw));
    int flat = !textured && uniform_rgb && !dither_active;
    u16 flat_color = (u16)((a.rgb[0] >> 3) | ((a.rgb[1] >> 3) << 5) | ((a.rgb[2] >> 3) << 10));
    hm_sw_tri_job job = {.s = s,
                         .a = a,
                         .b = b,
                         .c = c,
                         .area = area,
                         .x0 = x0,
                         .x1 = x1,
                         .edges = {hm_software_top_left(b, c), hm_software_top_left(c, a),
                                   hm_software_top_left(a, b)},
                         .textured = textured,
                         .raw = raw,
                         .flat = flat,
                         .uniform_rgb = uniform_rgb,
                         .dither_active = dither_active,
                         .op = op,
                         .clut = clut,
                         .flat_color = flat_color,
                         .d = d};
#ifdef HMBTN_ENH_FILTERING
    job.bounds = (hm_texture_bounds){(s32)a.u, (s32)a.v, (s32)a.u, (s32)a.v};
    for (int i = 0; i < 2; ++i) {
        if (others[i].u < (u32)job.bounds.min_u)
            job.bounds.min_u = (s32)others[i].u;
        if (others[i].v < (u32)job.bounds.min_v)
            job.bounds.min_v = (s32)others[i].v;
        if (others[i].u > (u32)job.bounds.max_u)
            job.bounds.max_u = (s32)others[i].u;
        if (others[i].v > (u32)job.bounds.max_v)
            job.bounds.max_v = (s32)others[i].v;
    }
#endif
    hm_sw_parallel_rows(y0, y1, x1 - x0, hm_software_triangle_rows, &job);
}
static hm_software_vertex hm_software_make_vertex(hm_software_surface *s, int x, int y, u32 rgb,
                                                  u32 uv) {
    hm_software_vertex v = {
        .x = hm_software_floor_div((s64)(x - s->area.x) * s->width * 256, s->area.w),
        .y = hm_software_floor_div((s64)(y - s->area.y) * s->height * 256, s->area.h),
        .lx = x,
        .ly = y,
        .u = uv & 255,
        .v = (uv >> 8) & 255};
    for (int i = 0; i < 3; ++i)
        v.rgb[i] = (rgb >> (i * 8)) & 255;
    return v;
}
static void hm_software_polygon(hm_software_surface *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, n = op & 8 ? 4 : 3;
    int textured = (op & 4) != 0, shaded = (op & 16) != 0;
    if (1 + n + (textured ? n : 0) + (shaded ? n - 1 : 0) > available)
        return;
    hm_software_vertex v[4];
    u32 at = 1, rgb = w[0];
    u16 clut = 0;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    for (u32 i = 0; i < n; ++i) {
        if (i && shaded)
            rgb = w[at++];
        u32 xy = w[at++], uv = textured ? w[at++] : 0;
        int x = hm_software_signed11(xy) + d->ofs[0],
            y = hm_software_signed11(xy >> 16) + d->ofs[1];
        v[i] = hm_software_make_vertex(s, x, y, rgb, uv);
        if (textured && !i)
            clut = (u16)(uv >> 16);
        if (textured && i == 1)
            hm_gpu_environment_command(0xe1000000u | ((uv >> 16) & 511) | ((u32)d->dtd << 9) |
                                       ((u32)d->dfe << 10));
    }
    hm_software_triangle(s, v[0], v[1], v[2], op, clut);
    if (n == 4)
        hm_software_triangle(s, v[1], v[2], v[3], op, clut);
}
typedef struct {
    hm_software_surface *s;
    const u32 *w;
    u32 op;
    int textured, width, height, lx, ly, x0, x1;
    u16 solid_color, clut;
    u32 u0, v0;
    int light[3];
    const DRAWENV *d;
} hm_sw_rect_job;

static void hm_software_rectangle_rows(const void *ctx, int y0, int y1) {
    const hm_sw_rect_job *job = ctx;
    hm_software_surface *s = job->s;
    u32 op = job->op;
    int textured = job->textured, width = job->width, height = job->height;
    int lx = job->lx, ly = job->ly, x0 = job->x0, x1 = job->x1;
    u16 solid_color = job->solid_color, clut = job->clut;
    u32 u0 = job->u0, v0 = job->v0;
    const DRAWENV *d = job->d;
#ifdef HMBTN_ENH_FILTERING
    hm_texture_bounds bounds = {(s32)u0, (s32)v0, (s32)u0 + width - 1, (s32)v0 + height - 1};
#endif
    for (int y = y0; y < y1; ++y) {
        int ly_val = s->logical_y[y], dy = ly_val - ly;
        if (dy < 0 || dy >= height || ly_val < d->clip.y || ly_val >= d->clip.y + d->clip.h)
            continue;
        if (!textured) {
            for (int x = x0; x < x1; ++x) {
                int dx = s->logical_x[x] - lx;
                if (dx >= 0 && dx < width)
                    hm_software_put(s, x, y, solid_color, (op & 2) != 0);
            }
            continue;
        }
#ifdef HMBTN_ENH_FILTERING
        if (s->texture_linear) {
            s32 vq = s->q8_y[y] - ly * 256 + bounds.min_v * 256;
            for (int x = x0; x < x1; ++x) {
                int dx = s->logical_x[x] - lx;
                if (dx < 0 || dx >= width)
                    continue;
                s32 uq = s->q8_x[x] - lx * 256 + bounds.min_u * 256;
                hm_texture_shading shading = {.opcode = op,
                                              .rectangle = 1,
                                              .light = {(u32)job->light[0], (u32)job->light[1],
                                                        (u32)job->light[2]},
                                              .background = s->pixels[(size_t)y * s->width + x]};
                u16 filtered;
                if (hm_texture_bilinear(uq, vq, bounds, clut, &shading, &filtered))
                    hm_software_put(s, x, y, filtered, 0);
            }
            continue;
        }
#endif
        int prev_dx = -1;
        u16 cached_texel = 0, cached_color = 0;
        for (int x = x0; x < x1; ++x) {
            int dx = s->logical_x[x] - lx;
            if (dx < 0 || dx >= width)
                continue;
            if (dx != prev_dx) {
                prev_dx = dx;
                cached_texel = hm_gpu_texel(u0 + (u32)dx, v0 + (u32)dy, clut);
                if (cached_texel) {
                    if (op & 1)
                        cached_color = cached_texel;
                    else {
                        cached_color = cached_texel & 0x8000;
                        for (int c = 0; c < 3; ++c) {
                            int value =
                                hm_software_clamp(((cached_texel >> (c * 5)) & 31) * job->light[c] / 128, 31);
                            cached_color |= (u16)(value << (c * 5));
                        }
                    }
                }
            }
            if (!cached_texel)
                continue;
            hm_software_put(s, x, y, cached_color, (op & 2) && (cached_texel & 0x8000));
        }
    }
}

static void hm_software_rectangle(hm_software_surface *s, const u32 *w, u32 available) {
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
    int lx = hm_software_signed11(w[1]) + d->ofs[0],
        ly = hm_software_signed11(w[1] >> 16) + d->ofs[1];
    hm_software_vertex a = hm_software_make_vertex(s, lx, ly, w[0], 0),
                       b = hm_software_make_vertex(s, lx + width, ly + height, w[0], 0);
    int x0 = hm_software_clamp((int)hm_software_floor_div(a.x, 256), (int)s->width),
        x1 = hm_software_clamp((int)hm_software_floor_div(b.x + 255, 256), (int)s->width);
    int y0 = hm_software_clamp((int)hm_software_floor_div(a.y, 256), (int)s->height),
        y1 = hm_software_clamp((int)hm_software_floor_div(b.y + 255, 256), (int)s->height);
    if (x0 >= x1 || y0 >= y1)
        return;
    hm_sw_rect_job job = {
        .s = s,
        .w = w,
        .op = op,
        .textured = textured,
        .width = width,
        .height = height,
        .lx = lx,
        .ly = ly,
        .x0 = x0,
        .x1 = x1,
        .solid_color = (u16)(((w[0] >> 3) & 31) | (((w[0] >> 11) & 31) << 5) |
                             (((w[0] >> 19) & 31) << 10)),
        .clut = textured ? (u16)(w[2] >> 16) : 0,
        .u0 = textured ? (w[2] & 255) : 0,
        .v0 = textured ? ((w[2] >> 8) & 255) : 0,
        .light = {(int)(w[0] & 255), (int)((w[0] >> 8) & 255), (int)((w[0] >> 16) & 255)},
        .d = d};
    hm_sw_parallel_rows(y0, y1, x1 - x0, hm_software_rectangle_rows, &job);
}
static void hm_software_line_segment(hm_software_surface *s, u32 xy0, u32 xy1, u32 c0, u32 c1,
                                     u32 op, int skip) {
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int nx0 = hm_software_signed11(xy0), ny0 = hm_software_signed11(xy0 >> 16),
        nx1 = hm_software_signed11(xy1), ny1 = hm_software_signed11(xy1 >> 16);
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
                    value = hm_software_clamp(
                        value + dither[(ny0 + d->ofs[1]) & 3][(nx0 + d->ofs[0]) & 3], 255);
                packet[0] |= (u32)value << (c * 8);
            }
            hm_software_rectangle(s, packet, 3);
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
static void hm_software_line(hm_software_surface *s, const u32 *w, u32 available) {
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
        hm_software_line_segment(s, xy, point, color, next, op, skip);
        if (!poly)
            return;
        xy = point;
        color = next;
        skip = 1;
    }
}
static void hm_software_command(void *context, const u32 *w, u32 available, int after) {
    hm_software_surface *s = context;
    if (!s->begun || !available)
        return;
    u32 op = w[0] >> 24;
    if (!after) {
        if (op >= 0x20 && op <= 0x3f)
            hm_software_polygon(s, w, available);
        else if (op >= 0x60 && op <= 0x7f)
            hm_software_rectangle(s, w, available);
        else if (op >= 0x40 && op <= 0x5f)
            hm_software_line(s, w, available);
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
                int lx = hm_software_logical_x(s, (int)x), ly = hm_software_logical_y(s, (int)y);
                if ((((u32)(lx - rx)) & 1023) < (u32)rw && (((u32)(ly - ry)) & 511) < (u32)rh)
                    s->pixels[(size_t)y * s->width + x] = hm_vram_pixel((u32)lx, (u32)ly);
            }
    }
}
hm_replay_sink hm_software_sink(hm_software_surface *s) {
    return (hm_replay_sink){s, hm_software_begin, hm_software_command, NULL, NULL};
}
