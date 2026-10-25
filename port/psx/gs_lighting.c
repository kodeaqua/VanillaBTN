#include "psx/error.h"
#include "psx/integer_math.h"
#include <inline_c.h>
#include <libgs.h>

void GsSetFlatLight(s32 index, const GsF_LIGHT *light) {
    if (index < 0 || index >= 3 || !light)
        hm_runtime_error("GsSetFlatLight", "invalid light slot or descriptor");
    s64 x = light->vx, y = light->vy, z = light->vz;
    u64 squared = (u64)(x * x) + (u64)(y * y) + (u64)(z * z);
    u32 length = hm_integer_sqrt(squared);
    if (!length)
        hm_runtime_error("GsSetFlatLight", "zero light direction");
    /* Light directions point toward surfaces; opposite normals are brightest.
     * Rows map normals to intensities; color columns map the three lights. */
    hm_gte.matrix[1][index][0] = (s16)(-x * 4096 / length);
    hm_gte.matrix[1][index][1] = (s16)(-y * 4096 / length);
    hm_gte.matrix[1][index][2] = (s16)(-z * 4096 / length);
    hm_gte.matrix[2][0][index] = (s16)(light->r * 16);
    hm_gte.matrix[2][1][index] = (s16)(light->g * 16);
    hm_gte.matrix[2][2][index] = (s16)(light->b * 16);
}
void GsSetAmbient(s32 red, s32 green, s32 blue) {
    if (red < 0 || red > 4096 || green < 0 || green > 4096 || blue < 0 || blue > 4096)
        hm_runtime_error("GsSetAmbient", "invalid ambient color");
    gte_SetBackColor(red / 16, green / 16, blue / 16);
}
void GsSetLightMode(s32 mode) {
    /* This game requests only the default normal-lighting mode; fog/depth
     * cueing modes need their own native GS sorting path before acceptance. */
    if (mode != 0)
        hm_runtime_error("GsSetLightMode", "unsupported lighting mode");
}
