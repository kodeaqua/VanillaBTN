#ifndef HMBTN_COMPAT_ASSET_H
#define HMBTN_COMPAT_ASSET_H

#include "compat/types.h"
#include <stddef.h>

/* Read fixed-width asset words without alignment, aliasing or pointer-width
 * assumptions. The caller owns bounds validation of the containing asset. */
static inline s32 hm_asset_s32le(const void *address) {
    const u8 *p = address;
    u32 bits = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
    return bits <= INT32_MAX ? (s32)bits : -1 - (s32)(UINT32_MAX - bits);
}

#endif
