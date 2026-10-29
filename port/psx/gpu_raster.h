#ifndef HMBTN_PSX_GPU_RASTER_H
#define HMBTN_PSX_GPU_RASTER_H
#include "compat/types.h"
#include "psx/error.h"
#include "psx/vram.h"

static inline s32 hm_gpu_clamp31(s32 value) {
    return value < 0 ? 0 : value > 31 ? 31 : value;
}

static inline void hm_gpu_fast_pixel(u16 *vram_row, u64 *dirty_row, s32 x, u16 color, int blend,
                                     u32 blend_mode, u16 set_mask, u16 check_mask) {
    u16 background = vram_row[x];
    if (background & check_mask)
        return;
    if (blend) {
        u16 result = color & 0x8000;
        for (u32 shift = 0; shift < 15; shift += 5) {
            s32 back = (background >> shift) & 31, front = (color >> shift) & 31, mixed;
            switch (blend_mode) {
            case 0:
                mixed = back / 2 + front / 2;
                break;
            case 1:
                mixed = back + front;
                break;
            case 2:
                mixed = back - front;
                break;
            default:
                mixed = back + front / 4;
                break;
            }
            result |= (u16)(hm_gpu_clamp31(mixed) << shift);
        }
        color = result;
    }
    u16 final_val = color | set_mask;
    if (background != final_val) {
        vram_row[x] = final_val;
        *dirty_row |= UINT64_C(1) << ((u32)x >> 4);
    }
}

static inline u16 hm_gpu_fast_texel(const u16 (*vram_pixels)[HM_VRAM_WIDTH], u32 page_x, u32 page_y,
                                    u32 depth, u32 mask_x, u32 mask_y, u32 tw_x, u32 tw_y,
                                    u32 clut_x, u32 clut_y, u32 u, u32 v) {
    if (depth == 3u)
        hm_runtime_error("DrawOTag", "reserved texture depth");
    u = ((u & ~mask_x) | tw_x) & 255u;
    v = ((v & ~mask_y) | tw_y) & 255u;
    u32 ty = (page_y + v) & 511u;
    if (depth == 0) {
        u32 index = (vram_pixels[ty][(page_x + (u >> 2)) & 1023u] >> ((u & 3u) * 4u)) & 15u;
        return vram_pixels[clut_y][(clut_x + index) & 1023u];
    }
    if (depth == 1) {
        u32 index = (vram_pixels[ty][(page_x + (u >> 1)) & 1023u] >> ((u & 1u) * 8u)) & 255u;
        return vram_pixels[clut_y][(clut_x + index) & 1023u];
    }
    return vram_pixels[ty][(page_x + u) & 1023u];
}

void hm_gpu_pixel(s32 x, s32 y, u16 color, int blend);
u16 hm_gpu_texel(u32 u, u32 v, u16 clut);
u32 hm_gpu_polygon(const u32 *words, u32 available);
u32 hm_gpu_line(const u32 *words, u32 available);
void hm_gpu_execute_words(const u32 *words, u32 count);
#endif
