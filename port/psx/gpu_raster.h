#ifndef HMBTN_PSX_GPU_RASTER_H
#define HMBTN_PSX_GPU_RASTER_H
#include "compat/types.h"
void hm_gpu_pixel(s32 x, s32 y, u16 color, int blend);
u16 hm_gpu_texel(u32 u, u32 v, u16 clut);
u32 hm_gpu_polygon(const u32 *words, u32 available);
u32 hm_gpu_line(const u32 *words, u32 available);
#endif
