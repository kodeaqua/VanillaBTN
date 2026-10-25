#ifndef HMBTN_COMPAT_PLAYER_STEPS_H
#define HMBTN_COMPAT_PLAYER_STEPS_H
#include "compat/types.h"
#include "psx/error.h"

/* The original probe indexes adjacent linker objects as one halfword span.
 * Select their real native objects for the three source-used base indices. */
static inline s16 *hm_player_probe_step(s32 base, u32 facing, s16 *small, s16 *large,
                                        s16 reach[8][2]) {
    if (facing >= 4)
        hm_runtime_error("player_probe_target", "invalid player facing");
    switch (base) {
    case 0:
        return small + facing * 2;
    case 0x18:
        return large + 8 + facing * 2;
    case 0x38:
        return reach[4 + facing];
    default:
        hm_runtime_error("player_probe_target", "unknown step table base");
    }
}
#endif
