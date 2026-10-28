#ifndef HMBTN_COMPAT_PLAYER_STEPS_H
#define HMBTN_COMPAT_PLAYER_STEPS_H
#include "compat/types.h"
#include "psx/error.h"

/* The original probe indexes adjacent linker objects as one halfword span.
 * Select their real native objects for the three source-used base indices. */
static inline s16 *hm_player_probe_step(s32 base, u32 facing, s16 *small, s16 *large,
                                        s16 reach[8][2]) {
    u32 safe_facing = facing & 3;
    switch (base) {
    case 0:
        return small + safe_facing * 2;
    case 0x18:
        return large + 8 + safe_facing * 2;
    case 0x38:
        return reach[4 + safe_facing];
    default:
        return small + safe_facing * 2;
    }
}
#endif
