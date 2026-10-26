#include "compat/fixed_math.h"
#include <libgte.h>

#include "psx/trig_table.inc"

static s32 hm_sine(u32 phase) {
    u32 index = phase & 1023;
    switch ((phase >> 10) & 3) {
    case 0:
        return hm_sine_quadrant[index];
    case 1:
        return hm_sine_quadrant[1024 - index];
    case 2:
        return -hm_sine_quadrant[index];
    default:
        return -hm_sine_quadrant[1024 - index];
    }
}

s32 rsin(s32 angle) { return hm_sine((u32)angle); }
s32 rcos(s32 angle) { return hm_sine((u32)angle + 1024); }
s32 hm_psx_csin(s32 angle) { return rsin(angle); }
s32 hm_psx_ccos(s32 angle) { return rcos(angle); }
