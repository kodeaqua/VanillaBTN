#ifndef HMBTN_PSX_INTEGER_MATH_H
#define HMBTN_PSX_INTEGER_MATH_H
#include "compat/types.h"
/* Restoring integer square root, with no host floating-point dependence. */
static inline u32 hm_integer_sqrt(u64 value) {
    u64 root = 0, bit = UINT64_C(1) << 62;
    while (bit > value)
        bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else
            root >>= 1;
        bit >>= 2;
    }
    return (u32)root;
}
#endif
