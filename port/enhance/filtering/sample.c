#include "enhance/filtering/sample.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
static int hm_filter_clamp(int n, int limit) { return n < 0 ? 0 : n > limit ? limit : n; }
static u16 hm_filter_shade(u16 texel, const hm_texture_shading *s) {
    u16 color = texel;
    if (!(s->opcode & 1)) {
        color = texel & 0x8000;
        static const s8 dith[4][4] = {
            {-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
        for (int c = 0; c < 3; ++c) {
            int value = (int)s->light[c], tex = (texel >> (c * 5)) & 31;
            if (s->rectangle)
                value = hm_filter_clamp(tex * value / 128, 31);
            else {
                value = tex * value / 16;
                if (s->dither)
                    value += dith[s->y & 3][s->x & 3];
                value = hm_filter_clamp(value, 255) >> 3;
            }
            color |= (u16)(value << (c * 5));
        }
    }
    if ((s->opcode & 2) && (texel & 0x8000)) {
        int mode = (hm_gpu_environment_state()->draw.tpage >> 5) & 3;
        u16 mixed = color & 0x8000;
        for (int c = 0; c < 3; ++c) {
            int back = (s->background >> (c * 5)) & 31, front = (color >> (c * 5)) & 31;
            int value = mode == 0   ? back / 2 + front / 2
                        : mode == 1 ? back + front
                        : mode == 2 ? back - front
                                    : back + front / 4;
            mixed |= (u16)(hm_filter_clamp(value, 31) << (c * 5));
        }
        color = mixed;
    }
    return color;
}
int hm_texture_filter_sample(s32 uq, s32 vq, hm_texture_bounds b, u16 clut,
                             const hm_texture_shading *s, int filter_mode, u16 *result) {
    if (filter_mode != HM_TEXTURE_FILTER_BILINEAR && filter_mode != HM_TEXTURE_FILTER_XBR)
        return 0;
    if (b.min_u < 0 || b.min_v < 0 || b.max_u > 2047 || b.max_v > 2047 || b.min_u > b.max_u ||
        b.min_v > b.max_v)
        return 0;
    if (uq < b.min_u * 256)
        uq = b.min_u * 256;
    if (uq > b.max_u * 256)
        uq = b.max_u * 256;
    if (vq < b.min_v * 256)
        vq = b.min_v * 256;
    if (vq > b.max_v * 256)
        vq = b.max_v * 256;
    u32 u = (u32)(uq / 256), v = (u32)(vq / 256), fx = (u32)uq & 255, fy = (u32)vq & 255;
    u32 u1 = u < (u32)b.max_u ? u + 1 : u, v1 = v < (u32)b.max_v ? v + 1 : v;
    u16 taps[4] = {hm_gpu_texel(u, v, clut), hm_gpu_texel(u1, v, clut), hm_gpu_texel(u, v1, clut),
                   hm_gpu_texel(u1, v1, clut)};
    u32 weights[4];
    if (filter_mode == HM_TEXTURE_FILTER_XBR) {
        float x = (float)fx / 255.0f, y = (float)fy / 255.0f;
        float tx = (x - 0.32f) / (0.68f - 0.32f), ty = (y - 0.32f) / (0.68f - 0.32f);
        tx = tx < 0.0f ? 0.0f : tx > 1.0f ? 1.0f : tx;
        ty = ty < 0.0f ? 0.0f : ty > 1.0f ? 1.0f : ty;
        x = tx * tx * (3.0f - 2.0f * tx);
        y = ty * ty * (3.0f - 2.0f * ty);
        if (taps[0] == taps[3] && taps[1] != taps[2]) {
            float target = x + y - 1.0f > 0.0f ? 1.0f : 0.0f;
            x += (target - x) * 0.35f;
            y += (target - y) * 0.35f;
        } else if (taps[1] == taps[2] && taps[0] != taps[3]) {
            float dx = x - y;
            float target_x = dx > 0.0f ? 1.0f : 0.0f;
            float target_y = dx > 0.0f ? 0.0f : 1.0f;
            x += (target_x - x) * 0.35f;
            y += (target_y - y) * 0.35f;
        }
        u32 sx = (u32)(x * 255.0f), sy = (u32)(y * 255.0f);
        weights[0] = (256 - sx) * (256 - sy);
        weights[1] = sx * (256 - sy);
        weights[2] = (256 - sx) * sy;
        weights[3] = sx * sy;
    } else {
        weights[0] = (256 - fx) * (256 - fy);
        weights[1] = fx * (256 - fy);
        weights[2] = (256 - fx) * fy;
        weights[3] = fx * fy;
    }
    u32 sums[3] = {0}, strongest = 0;
    u16 mask = 0;
    int covered = 0;
    for (int i = 0; i < 4; ++i) {
        u16 value = s->background;
        if (taps[i] && weights[i]) {
            covered = 1;
            value = hm_filter_shade(taps[i], s);
            if (weights[i] > strongest) {
                strongest = weights[i];
                mask = taps[i] & 0x8000;
            }
        }
        for (int c = 0; c < 3; ++c)
            sums[c] += ((value >> (c * 5)) & 31) * weights[i];
    }
    if (!covered)
        return 0;
    *result = mask;
    for (int c = 0; c < 3; ++c)
        *result |= (u16)(((sums[c] + 32768) >> 16) << (c * 5));
    return 1;
}
int hm_texture_bilinear(s32 uq, s32 vq, hm_texture_bounds b, u16 clut, const hm_texture_shading *s,
                        u16 *result) {
    return hm_texture_filter_sample(uq, vq, b, clut, s, HM_TEXTURE_FILTER_BILINEAR, result);
}
