#include <libgte.h>
#include <stdint.h>

/* Numerical SDK-function results, generated/checked by the optional reference
 * microtest tool. No proprietary SDK source or binaries are included. */
static const unsigned short atan_octant[1025] = {
#include "sdk_atan_table.inc"
};

long ratan2(long y, long x) {
    if (x == 0 && y == 0) return 0;
    int negative_x = x < 0, negative_y = y < 0;
    uint32_t ax = (uint32_t)(x < 0 ? -(int64_t)x : x);
    uint32_t ay = (uint32_t)(y < 0 ? -(int64_t)y : y);
    uint32_t small = ax < ay ? ax : ay, large = ax < ay ? ay : ax;
    /* Match SDK normalization: for large operands, shift the denominator
     * before dividing instead of overflowing a 32-bit shifted numerator. */
    unsigned ratio = (small & 0x7FE00000u) ? small / (large >> 10)
        : (unsigned)(((uint64_t)small << 10) / large);
    if (ratio > 1024) ratio = 1024;
    long angle = ax > ay ? atan_octant[ratio] : 1024 - atan_octant[ratio];
    if (negative_x) angle = 2048 - angle;
    return negative_y ? -angle : angle;
}

int csqrt(int value) {
    if (value <= 0) return 0;
    unsigned leading = 0;
    for (uint32_t bits = (uint32_t)value; !(bits & 0x80000000u); bits <<= 1) ++leading;
    int exponent = 8 - (int)leading;
    int scale = exponent >= 0 ? exponent / 2 : -((-exponent + 1) / 2) + 1;
    int normalized = scale >= 0 ? value >> (scale * 2) : value << (-scale * 2);
    /* SDK fixed-point hyperbolic CORDIC: six shift stages, repeating stage 4.
     * Preserve intermediate truncation; a generic sqrt changes tilt levels. */
    int x = normalized + 0x5D50AD, y = normalized - 0x5D50AD;
    for (int shift = 1; shift <= 6; ++shift) {
        int repetitions = shift == 4 ? 2 : 1;
        while (repetitions-- > 0) {
            int old_x = x;
            if (y < 0) { x += y >> shift; y += old_x >> shift; }
            else { x -= y >> shift; y -= old_x >> shift; }
        }
    }
    return x >> (6 - scale);
}
