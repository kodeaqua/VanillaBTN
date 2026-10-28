#include "compat/fixed_math.h"
#include "psx/error.h"
#include "psx/integer_math.h"
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

/* Mathematical CORDIC constants, generated from atan(2^-i) in Q32 turn units.
 * No SDK lookup table or implementation is copied. */
static const s64 hm_atan_q32[40] = {2199023255552LL,
                                    1298159229407LL,
                                    685911684590LL,
                                    348179481054LL,
                                    174765388006LL,
                                    87467890064LL,
                                    43744617923LL,
                                    21873643805LL,
                                    10936988781LL,
                                    5468515251LL,
                                    2734260233LL,
                                    1367130443LL,
                                    683565262LL,
                                    341782636LL,
                                    170891319LL,
                                    85445659LL,
                                    42722830LL,
                                    21361415LL,
                                    10680707LL,
                                    5340354LL,
                                    2670177LL,
                                    1335088LL,
                                    667544LL,
                                    333772LL,
                                    166886LL,
                                    83443LL,
                                    41722LL,
                                    20861LL,
                                    10430LL,
                                    5215LL,
                                    2608LL,
                                    1304LL,
                                    652LL,
                                    326LL,
                                    163LL,
                                    81LL,
                                    41LL,
                                    20LL,
                                    10LL,
                                    5LL};
static s64 hm_atan_shift(s64 n, unsigned bits) {
    if (n >= 0)
        return n >> bits;
    return -(((-n) + ((INT64_C(1) << bits) - 1)) >> bits);
}
s32 ratan2(s32 y, s32 x) {
    if (!y)
        return x < 0 ? 2048 : 0;
    if (!x)
        return y < 0 ? -1024 : 1024;
    s64 ax = x < 0 ? -(s64)x : x, ay = y < 0 ? -(s64)y : y;
    s64 vx = ax * (INT64_C(1) << 20), vy = ay * (INT64_C(1) << 20), angle = 0;
    for (unsigned i = 0; i < 40 && vy; ++i) {
        s64 dx = hm_atan_shift(vx, i), dy = hm_atan_shift(vy, i);
        if (vy > 0) {
            vx += dy;
            vy -= dx;
            angle += hm_atan_q32[i];
        } else {
            vx -= dy;
            vy += dx;
            angle -= hm_atan_q32[i];
        }
    }
    s32 result = (s32)((angle + (INT64_C(1) << 31)) >> 32);
    if (x < 0)
        result = 2048 - result;
    return y < 0 ? -result : result;
}
s32 hm_psx_csqrt(s32 value) {
    if (value <= 0)
        return 0;
    return (s32)hm_integer_sqrt((u64)(u32)value << 12);
}
