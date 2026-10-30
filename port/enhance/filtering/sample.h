#ifndef HMBTN_TEXTURE_FILTER_H
#define HMBTN_TEXTURE_FILTER_H
#include "compat/types.h"
typedef struct {
    s32 min_u, min_v, max_u, max_v;
} hm_texture_bounds;
typedef struct {
    u32 opcode, light[3];
    int rectangle, dither, x, y;
    u16 background;
} hm_texture_shading;
enum { HM_TEXTURE_FILTER_NEAREST, HM_TEXTURE_FILTER_BILINEAR, HM_TEXTURE_FILTER_XBR };
/* Q8 texture coordinates; clamp within the primitive's atlas bounds before
 * applying the original texture-window/page/CLUT sampler. Zero means no covered
 * tap: the caller must leave destination color and mask completely untouched. */
int hm_texture_filter_sample(s32 u_q8, s32 v_q8, hm_texture_bounds bounds, u16 clut,
                             const hm_texture_shading *shading, int filter_mode, u16 *result);
int hm_texture_bilinear(s32 u_q8, s32 v_q8, hm_texture_bounds bounds, u16 clut,
                        const hm_texture_shading *shading, u16 *result);
#endif
