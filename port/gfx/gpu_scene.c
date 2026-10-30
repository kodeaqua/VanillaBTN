#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
#include "gfx/gpu_scene.h"
#include "enhance/pgxp/pgxp.h"
#include "enhance/dithering/dithering.h"
#include <math.h>
#include "psx/gpu_environment.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>
struct hm_gpu_scene {
    u32 primitives[HM_SCENE_PRIMITIVES][HM_SCENE_STRIDE];
    u32 bins[HM_SCENE_BINS][HM_SCENE_BIN_WORDS];
    u32 packed[HM_SCENE_MAX_WORDS];
    u32 width, height, count, tiles_x, tiles_y;
    RECT area, native;
    u16 background;
    const char *failure;
    int begun, texture_linear;
    unsigned multisamples;
    u64 revision;
    const u32 *motion_previous, *motion_current;
    u32 motion_count, motion_alpha;
};
_Static_assert(sizeof(((hm_gpu_scene *)0)->primitives[0]) == 160, "GPU descriptor ABI");
static int hm_scene_signed(u32 n) { return (int)((n & 2047) ^ 1024) - 1024; }
static s64 hm_scene_floor(s64 n, s64 d) { return n >= 0 ? n / d : -((-n + d - 1) / d); }
static s64 hm_scene_coordinate(hm_gpu_scene *s, u32 word, int shift, u32 fallback, int offset) {
    if (s->motion_previous && s->motion_current && word < s->motion_count) {
        int a = hm_scene_signed(s->motion_previous[word] >> shift),
            b = hm_scene_signed(s->motion_current[word] >> shift);
        return (s64)(a + offset) * 65536 + (s64)(b - a) * s->motion_alpha;
    }
    return (s64)(hm_scene_signed(fallback >> shift) + offset) * 65536;
}
static u32 hm_scene_scaled(s64 q, int origin, u32 size, int logical) {
    return (u32)hm_scene_floor((q - (s64)origin * 65536) * size * 256, (s64)logical * 65536);
}
static int hm_scene_overlap(int x, int y, int w, int h, const RECT *r) {
    for (int sy = 0; sy < 2; ++sy)
        for (int sx = 0; sx < 2; ++sx) {
            int left = x - sx * 1024, top = y - sy * 512;
            if (left < r->x + r->w && left + w > r->x && top < r->y + r->h && top + h > r->y)
                return 1;
        }
    return 0;
}
static int hm_scene_material(hm_gpu_scene *s, u32 *p, u32 op, u32 page, u32 clut) {
    const hm_gpu_environment *env = hm_gpu_environment_state();
    const DRAWENV *d = &env->draw;
    if (op & 4) {
        u32 depth = (page >> 7) & 3;
        if (depth > 2) {
            s->failure = "reserved texture depth";
            return 0;
        }
        if (depth < 2 && hm_scene_overlap((clut & 63) * 16, (clut >> 6) & 511, depth ? 256 : 16, 1,
                                          &s->native)) {
            s->failure = "framebuffer texture feedback";
            return 0;
        }
    }
    const hm_vram_context *vram = hm_vram_state();
    p[1] = op;
    p[2] = page;
    p[3] = clut;
    int dtd = hm_dither_enabled() ? (int)d->dtd : 0;
    p[4] = (u32)dtd | (d->dfe << 1) | (vram->set_mask ? 4u : 0) | (vram->check_mask ? 8u : 0);
    p[5] = d->tw.x;
    p[6] = d->tw.y;
    p[7] = d->tw.w;
    p[8] = d->tw.h;
    p[9] = d->clip.x;
    p[10] = d->clip.y;
    p[11] = d->clip.w;
    p[12] = d->clip.h;
    p[13] = env->display.disp.x;
    p[14] = env->display.disp.y;
    p[15] = env->display.disp.w;
    p[16] = env->display.disp.h;
    p[17] = env->display_enabled;
    return 1;
}
/* Conservative sampled UV footprint, including texture-window remapping and
 * 8-bit wrapping. A page can share unused rows with the native framebuffer. */
static int hm_scene_texture_bounds(hm_gpu_scene *s, const u32 *p, u32 u, u32 v, u32 width,
                                   u32 height) {
    if (!(p[1] & 4) || !width || !height)
        return 1;
    u32 depth = (p[2] >> 7) & 3, mx = (256 - p[7]) & 248, my = (256 - p[8]) & 248;
    int x0 = 1024, x1 = -1, y0 = 512, y1 = -1;
    if (width > 256)
        width = 256;
    if (height > 256)
        height = 256;
    u32 u_base = u & 255, v_base = v & 255;
    if (!mx && u_base + width <= 256) {
        x0 = (int)(u_base >> (2 - depth));
        x1 = (int)((u_base + width - 1) >> (2 - depth));
    } else {
        for (u32 i = 0; i < width; ++i) {
            u32 mapped = ((((u + i) & 255) & ~mx) | (p[5] & mx)) & 255;
            int x = (int)(mapped >> (2 - depth));
            if (x < x0)
                x0 = x;
            if (x > x1)
                x1 = x;
        }
    }
    if (!my && v_base + height <= 256) {
        y0 = (int)v_base;
        y1 = (int)(v_base + height - 1);
    } else {
        for (u32 i = 0; i < height; ++i) {
            int y = (int)(((((v + i) & 255) & ~my) | (p[6] & my)) & 255);
            if (y < y0)
                y0 = y;
            if (y > y1)
                y1 = y;
        }
    }
    if (hm_scene_overlap((p[2] & 15) * 64 + x0, (p[2] & 16) * 16 + y0, x1 - x0 + 1, y1 - y0 + 1,
                         &s->native)) {
        s->failure = "framebuffer texture feedback";
        return 0;
    }
    return 1;
}
static void hm_scene_add(hm_gpu_scene *s, u32 *p, int x0, int y0, int x1, int y1) {
    if (s->failure)
        return;
    if (p[4] & 32) {
        --x0;
        --y0;
        ++x1;
        ++y1;
    }
    if (x0 < (int)p[9])
        x0 = (int)p[9];
    if (y0 < (int)p[10])
        y0 = (int)p[10];
    if (x1 > (int)p[9] + (int)p[11])
        x1 = (int)p[9] + (int)p[11];
    if (y1 > (int)p[10] + (int)p[12])
        y1 = (int)p[10] + (int)p[12];
    if (x0 >= x1 || y0 >= y1)
        return;
    if (x0 < s->area.x || y0 < s->area.y || x1 > s->area.x + s->area.w ||
        y1 > s->area.y + s->area.h) {
        s->failure = "offscreen draw dependencies";
        return;
    }
    if (x0 < s->area.x)
        x0 = s->area.x;
    if (y0 < s->area.y)
        y0 = s->area.y;
    if (x1 > s->area.x + s->area.w)
        x1 = s->area.x + s->area.w;
    if (y1 > s->area.y + s->area.h)
        y1 = s->area.y + s->area.h;
    if (x0 >= (int)p[9] + (int)p[11] || y0 >= (int)p[10] + (int)p[12] || x1 <= (int)p[9] ||
        y1 <= (int)p[10])
        return;
    if (x0 >= x1 || y0 >= y1)
        return;
    if (s->count == HM_SCENE_PRIMITIVES) {
        s->failure = "primitive capacity";
        return;
    }
    memcpy(s->primitives[s->count], p, HM_SCENE_STRIDE * sizeof(*p));
    u32 bx0 = (u32)(x0 - s->area.x) / 16, by0 = (u32)(y0 - s->area.y) / 16;
    u32 bx1 = (u32)(x1 - 1 - s->area.x) / 16, by1 = (u32)(y1 - 1 - s->area.y) / 16;
    for (u32 y = by0; y <= by1; ++y)
        for (u32 x = bx0; x <= bx1; ++x) {
            u32 *bin = s->bins[y * s->tiles_x + x];
            if (bin[0] == HM_SCENE_BIN_WORDS - 1) {
                s->failure = "tile capacity";
                return;
            }
            bin[++bin[0]] = s->count;
        }
    ++s->count;
}
static void hm_scene_begin_capture(void *context, const RECT *area) {
    hm_gpu_scene *s = context;
    s->begun = 0;
    int expanded = 0;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT expected = hm_widescreen_area(hm_gpu_environment_state()->draw.clip);
    expanded = area->x == expected.x && area->y == expected.y && area->w == expected.w &&
               area->h == expected.h;
#endif
    if (!expanded && (area->x < 0 || area->x + area->w > 1024)) {
        s->failure = "unsupported render dimensions";
        return;
    }
    if (!s->width || !s->height || s->width > 4096 || s->height > 4096 || area->x < -352 ||
        area->y < 0 || area->w <= 0 || area->w > 1024 || area->h <= 0 || area->x + area->w > 1376 ||
        area->y + area->h > 512) {
        s->failure = "unsupported render dimensions";
        return;
    }
    s->area = *area;
    s->native = hm_gpu_environment_state()->draw.clip;
    s->background = hm_gpu_background(&hm_gpu_environment_state()->draw);
    s->tiles_x = ((u32)area->w + 15) / 16;
    s->tiles_y = ((u32)area->h + 15) / 16;
    for (u32 i = 0; i < s->tiles_x * s->tiles_y; ++i)
        s->bins[i][0] = 0;
    memcpy(s->packed + HM_SCENE_HEADER, hm_vram_state()->pixels, 1024 * 512 * sizeof(u16));
    s->begun = 1;
}
static void hm_scene_triangle(hm_gpu_scene *s, u32 *material, const u32 xy[3], const u32 rgb[3],
                              const u32 uv[3], const u32 indices[3], const u32 *w) {
    u32 p[HM_SCENE_STRIDE];
    memcpy(p, material, sizeof(p));
    p[0] = 0;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    int x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;

    float pgxp_x[3], pgxp_y[3], pgxp_z[3];
    int has_pgxp = hm_pgxp_enabled();
    if (has_pgxp) {
        for (int i = 0; i < 3; ++i) {
            const void *ptr = &w[indices[i]];
            if (!hm_pgxp_lookup_vertex(ptr, xy[i], &pgxp_x[i], &pgxp_y[i], &pgxp_z[i])) {
                has_pgxp = 0;
                break;
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        int x = hm_scene_signed(xy[i]) + d->ofs[0], y = hm_scene_signed(xy[i] >> 16) + d->ofs[1];
        p[18 + i * 2] = (u32)x;
        p[19 + i * 2] = (u32)y;
        p[24 + i] = rgb[i] & 0xffffff;
        p[27 + i] = uv[i] & 65535;
        if (has_pgxp && !s->motion_previous) {
            s64 qx = (s64)roundf(pgxp_x[i] * 65536.0f) + (s64)d->ofs[0] * 65536;
            s64 qy = (s64)roundf(pgxp_y[i] * 65536.0f) + (s64)d->ofs[1] * 65536;
            p[34 + i * 2] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
            p[35 + i * 2] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
            float z_val = pgxp_z[i] > 1.0f ? pgxp_z[i] : 1.0f;
            memcpy(&p[30 + i], &z_val, sizeof(float));
        } else {
            s64 qx = hm_scene_coordinate(s, indices[i], 0, xy[i], d->ofs[0]);
            s64 qy = hm_scene_coordinate(s, indices[i], 16, xy[i], d->ofs[1]);
            p[34 + i * 2] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
            p[35 + i * 2] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
        }
        if (x < x0)
            x0 = x;
        if (x > x1)
            x1 = x;
        if (y < y0)
            y0 = y;
        if (y > y1)
            y1 = y;
    }
    if (x1 - x0 > 1023 || y1 - y0 > 511)
        return;

    s64 area = (s64)((s32)p[20] - (s32)p[18]) * ((s32)p[23] - (s32)p[19]) -
               (s64)((s32)p[21] - (s32)p[19]) * ((s32)p[22] - (s32)p[18]);
    if (s->motion_previous || has_pgxp) {
        p[4] |= 32;
        /* Interpolated vertices do not carry interpolated depth. Keep affine
         * texture mapping for those frames instead of using unset PGXP Z. */
        if (has_pgxp && !s->motion_previous) {
            p[33] = 1;
        }
        area = ((s64)(s32)p[36] - (s32)p[34]) * ((s64)(s32)p[39] - (s32)p[35]) -
               ((s64)(s32)p[37] - (s32)p[35]) * ((s64)(s32)p[38] - (s32)p[34]);
    }
    if (!area)
        return;
    if (area < 0) {
        u32 tmp;
        for (int j = 0; j < 2; ++j) {
            tmp = p[20 + j];
            p[20 + j] = p[22 + j];
            p[22 + j] = tmp;
        }
        for (int j = 0; j < 2; ++j) {
            tmp = p[36 + j];
            p[36 + j] = p[38 + j];
            p[38 + j] = tmp;
        }
        tmp = p[25];
        p[25] = p[26];
        p[26] = tmp;
        tmp = p[28];
        p[28] = p[29];
        p[29] = tmp;
        if (has_pgxp) {
            tmp = p[31];
            p[31] = p[32];
            p[32] = tmp;
        }
    }
    if (p[1] & 4) {
        u32 u0 = 255, u1 = 0, v0 = 255, v1 = 0;
        for (int i = 0; i < 3; ++i) {
            u32 u = p[27 + i] & 255, v = (p[27 + i] >> 8) & 255;
            if (u < u0)
                u0 = u;
            if (u > u1)
                u1 = u;
            if (v < v0)
                v0 = v;
            if (v > v1)
                v1 = v;
        }
        if (!hm_scene_texture_bounds(s, p, u0, v0, u1 - u0 + 1, v1 - v0 + 1))
            return;
    }
    hm_scene_add(s, p, x0, y0, x1, y1);
}
static void hm_scene_polygon(hm_gpu_scene *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, n = op & 8 ? 4 : 3;
    int tex = (op & 4) != 0, shaded = (op & 16) != 0;
    if (1 + n + (tex ? n : 0) + (shaded ? n - 1 : 0) > available) {
        s->failure = "truncated polygon";
        return;
    }
    u32 xy[4], rgb[4], uv[4] = {0}, indices[4], at = 1, color = w[0];
    for (u32 i = 0; i < n; ++i) {
        if (i && shaded)
            color = w[at++];
        rgb[i] = color;
        indices[i] = at;
        xy[i] = w[at++];
        if (tex)
            uv[i] = w[at++];
    }
    u32 p[HM_SCENE_STRIDE] = {0};
    u32 page = tex ? uv[1] >> 16 : hm_gpu_environment_state()->draw.tpage;
    if (!hm_scene_material(s, p, op, page, tex ? uv[0] >> 16 : 0))
        return;
    hm_scene_triangle(s, p, xy, rgb, uv, indices, w);
    if (n == 4)
        hm_scene_triangle(s, p, xy + 1, rgb + 1, uv + 1, indices + 1, w);
}
static void hm_scene_rectangle(hm_gpu_scene *s, const u32 *w, u32 available) {
    u32 op = w[0] >> 24, size = (op >> 3) & 3;
    int tex = (op & 4) != 0;
    u32 length = 2 + (u32)tex + (size == 0);
    if (length > available) {
        s->failure = "truncated rectangle";
        return;
    }
    u32 width = size == 1 ? 1 : size == 2 ? 8 : 16, height = width;
    if (!size) {
        width = w[length - 1] & 1023;
        height = (w[length - 1] >> 16) & 511;
    }
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    u32 p[HM_SCENE_STRIDE] = {0};
    if (!hm_scene_material(s, p, op, d->tpage, tex ? w[2] >> 16 : 0))
        return;
    int x = hm_scene_signed(w[1]) + d->ofs[0], y = hm_scene_signed(w[1] >> 16) + d->ofs[1];
    p[0] = 1;
    p[24] = w[0] & 0xffffff;
    p[27] = tex ? w[2] & 65535 : 0;
    p[30] = (u32)x;
    p[31] = (u32)y;
    p[32] = width;
    p[33] = height;
    if (s->motion_previous) {
        s64 qx = hm_scene_coordinate(s, 1, 0, w[1], d->ofs[0]);
        s64 qy = hm_scene_coordinate(s, 1, 16, w[1], d->ofs[1]);
        p[4] |= 32;
        p[35] = hm_scene_scaled(qx, s->area.x, s->width, s->area.w);
        p[36] = hm_scene_scaled(qy, s->area.y, s->height, s->area.h);
        p[37] = hm_scene_scaled(qx + (s64)width * 65536, s->area.x, s->width, s->area.w);
        p[38] = hm_scene_scaled(qy + (s64)height * 65536, s->area.y, s->height, s->area.h);
    }
    if (!hm_scene_texture_bounds(s, p, p[27] & 255, (p[27] >> 8) & 255, width, height))
        return;
    hm_scene_add(s, p, x, y, x + (int)width, y + (int)height);
}
static void hm_scene_line_segment(hm_gpu_scene *s, u32 xy0, u32 xy1, u32 c0, u32 c1, u32 op,
                                  int skip) {
    int x = hm_scene_signed(xy0), y = hm_scene_signed(xy0 >> 16), x1 = hm_scene_signed(xy1),
        y1 = hm_scene_signed(xy1 >> 16);
    int dx = abs(x1 - x), dy = abs(y1 - y);
    if (dx > 1023 || dy > 511)
        return;
    const DRAWENV *d = &hm_gpu_environment_state()->draw;
    static const s8 dith[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    int sx = x < x1 ? 1 : -1, sy = y < y1 ? 1 : -1, error = dx - dy, steps = dx > dy ? dx : dy;
    /* Axis-aligned flat, undithered lines are one rectangle rather than N cells. */
    if (!skip && (c0 & 0xffffff) == (c1 & 0xffffff) && (!dx || !dy) && dx < 1023 && dy < 511) {
        u32 p[3] = {0x60000000u | ((op & 2) << 24) | (c0 & 0xffffff),
                    (u16)(x < x1 ? x : x1) | ((u32)(u16)(y < y1 ? y : y1) << 16),
                    (u32)(dx + 1) | ((u32)(dy + 1) << 16)};
        u32 before = s->count;
        hm_scene_rectangle(s, p, 3);
        if (s->count == before + 1)
            s->primitives[before][34] = 1;
        return;
    }
    for (int at = 0;; ++at) {
        if (!skip || at) {
            u32 rgb = 0;
            for (int c = 0; c < 3; ++c) {
                int a = (c0 >> (c * 8)) & 255, b = (c1 >> (c * 8)) & 255;
                int value = steps ? (a * (steps - at) + b * at + steps / 2) / steps : a;
                if (d->dtd)
                    value += dith[(y + d->ofs[1]) & 3][(x + d->ofs[0]) & 3];
                if (value < 0)
                    value = 0;
                if (value > 255)
                    value = 255;
                rgb |= (u32)value << (c * 8);
            }
            u32 p[3] = {0x60000000u | ((op & 2) << 24) | rgb, (u16)x | ((u32)(u16)y << 16),
                        0x10001};
            hm_scene_rectangle(s, p, 3);
        }
        if ((x == x1 && y == y1) || s->failure)
            break;
        int e = 2 * error;
        if (e > -dy) {
            error -= dy;
            x += sx;
        }
        if (e < dx) {
            error += dx;
            y += sy;
        }
    }
}
static void hm_scene_line(hm_gpu_scene *s, const u32 *w, u32 available) {
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    u32 op = w[0] >> 24;
    int shaded = (op & 16) != 0, poly = (op & 8) != 0;
    if (available < (shaded ? 4u : 3u)) {
        s->failure = "truncated line";
        return;
    }
    u32 at = 2, xy = w[1], color = w[0];
    int skip = 0;
    while (at < available) {
        if (poly && skip && (w[at] & 0xf000f000u) == 0x50005000u)
            return;
        u32 next = shaded ? w[at++] : color;
        if (at >= available)
            break;
        u32 point = w[at++];
        hm_scene_line_segment(s, xy, point, color, next, op, skip);
        if (!poly)
            return;
        xy = point;
        color = next;
        skip = 1;
    }
    s->failure = "unterminated line";
}
static void hm_scene_command(void *context, const u32 *w, u32 available, int after) {
    hm_gpu_scene *s = context;
    if (after || !s->begun || s->failure || !available)
        return;
    u32 op = w[0] >> 24;
    if (op >= 0x20 && op <= 0x3f)
        hm_scene_polygon(s, w, available);
    else if (op >= 0x60 && op <= 0x7f)
        hm_scene_rectangle(s, w, available);
    else if (op >= 0x40 && op <= 0x5f)
        hm_scene_line(s, w, available);
    else if (op != 0 && op != 1 && !(op >= 0xe1 && op <= 0xe6))
        s->failure = "inline VRAM transfer";
}
hm_gpu_scene *hm_gpu_scene_create(void) { return calloc(1, sizeof(hm_gpu_scene)); }
void hm_gpu_scene_destroy(hm_gpu_scene *s) { free(s); }
void hm_gpu_scene_texture_filter(hm_gpu_scene *s, int linear) { s->texture_linear = !!linear; }
void hm_gpu_scene_multisamples(hm_gpu_scene *s, unsigned samples) {
    s->multisamples = samples == 2 || samples == 4 ? samples : 1;
}
static u32 hm_scene_consume(void *context, const u32 *w, u32 available) {
    if (!available)
        return 0;
    u32 op = w[0] >> 24, length = 0;
    if (op >= 0x20 && op <= 0x3f) {
        u32 n = op & 8 ? 4 : 3;
        length = 1 + n + ((op & 4) ? n : 0) + ((op & 16) ? n - 1 : 0);
    } else if (op >= 0x60 && op <= 0x7f)
        length = 2 + ((op & 4) ? 1 : 0) + (((op >> 3) & 3) == 0);
    else if (op >= 0x40 && op <= 0x5f) {
        u32 at = 2;
        int shaded = (op & 16) != 0, poly = (op & 8) != 0, skip = 0;
        while (at < available) {
            if (poly && skip && (w[at] & 0xf000f000u) == 0x50005000u) {
                length = at + 1;
                break;
            }
            if (shaded)
                ++at;
            if (at >= available)
                break;
            ++at;
            if (!poly) {
                length = at;
                break;
            }
            skip = 1;
        }
    }
    if (!length || length > available)
        return 0;
    hm_scene_command(context, w, length, 0);
    hm_gpu_scene *s = context;
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    if (op >= 0x20 && op <= 0x3f && (op & 4)) {
        const DRAWENV *draw = &hm_gpu_environment_state()->draw;
        u32 page = w[(op & 16) ? 5 : 4] >> 16;
        hm_gpu_environment_command(0xe1000000u | (page & 511) | ((u32)draw->dtd << 9) |
                                   ((u32)draw->dfe << 10));
    }
    return length;
}
static void hm_scene_motion(void *context, const u32 *previous, const u32 *current, u32 count,
                            u32 alpha) {
    hm_gpu_scene *s = context;
    if (!previous || !current || !count || alpha > 65536) {
        previous = current = NULL;
        count = 0;
        alpha = 0;
    }
    s->motion_previous = previous;
    s->motion_current = current;
    s->motion_count = count;
    s->motion_alpha = alpha;
}
hm_replay_sink hm_gpu_scene_begin(hm_gpu_scene *s, u32 width, u32 height) {
    s->width = width;
    s->height = height;
    s->count = 0;
    s->failure = NULL;
    s->begun = 0;
    s->motion_previous = s->motion_current = NULL;
    s->motion_count = 0;
    return hm_replay_bind((hm_replay_sink){s, hm_scene_begin_capture, hm_scene_command,
                                           hm_scene_consume, hm_scene_motion});
}
int hm_gpu_scene_end(hm_gpu_scene *s, hm_replay_sink previous, hm_gpu_scene_data *data) {
    hm_replay_bind(previous);
    if (!s->begun || s->failure)
        return 0;
    u32 primitives = HM_SCENE_HEADER + 1024 * 512 / 2,
        bins = primitives + s->count * HM_SCENE_STRIDE;
    u32 total = bins + s->tiles_x * s->tiles_y * HM_SCENE_BIN_WORDS;
    memset(s->packed, 0, HM_SCENE_HEADER * sizeof(u32));
    s->packed[0] = s->width;
    s->packed[1] = s->height;
    s->packed[2] = s->area.x;
    s->packed[3] = s->area.y;
    s->packed[4] = s->area.w;
    s->packed[5] = s->area.h;
    s->packed[6] = s->tiles_x;
    s->packed[7] = s->tiles_y;
    s->packed[8] = primitives;
    s->packed[9] = bins;
    s->packed[10] = s->count;
    s->packed[11] = (u32)s->texture_linear;
    s->packed[12] = s->native.x;
    s->packed[13] = s->native.w;
    s->packed[14] = s->background;
    s->packed[15] = s->multisamples == 2 || s->multisamples == 4 ? s->multisamples : 1;
    memcpy(s->packed + primitives, s->primitives, s->count * HM_SCENE_STRIDE * sizeof(u32));
    memcpy(s->packed + bins, s->bins, s->tiles_x * s->tiles_y * HM_SCENE_BIN_WORDS * sizeof(u32));
    *data = (hm_gpu_scene_data){s->packed, (size_t)total * sizeof(u32), ++s->revision, s->width,
                                s->height};
    return 1;
}
const char *hm_gpu_scene_fallback(const hm_gpu_scene *s) {
    return s->failure ? s->failure : "missing frame history";
}
