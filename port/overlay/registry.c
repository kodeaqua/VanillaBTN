#include "overlay/registry.h"
#include "psx/error.h"

static const struct {
    s32 member, region;
} hm_overlays[] = {{8, 0},  {10, 0}, {11, 0}, {15, 1}, {17, 0}, {19, 0}, {21, 0},
                   {23, 0}, {25, 0}, {27, 0}, {28, 0}, {30, 1}, {31, 0}};
static u8 hm_loaded[13];
static s32 hm_active[2] = {-1, -1};

void hm_overlay_reset(void) {
    for (u32 i = 0; i < 13; ++i)
        hm_loaded[i] = 0;
    hm_active[0] = hm_active[1] = -1;
}

s32 hm_overlay_select(s32 member, s32 region) {
    if (region < 0 || region > 1)
        hm_runtime_error("hm_overlay_select", "invalid region");
    for (u32 i = 0; i < 13; ++i) {
        if (hm_overlays[i].member != member)
            continue;
        if (hm_overlays[i].region != region)
            hm_runtime_error("hm_overlay_select", "overlay region mismatch");
        /* Compiled C initializers supply the first activation's state. Reentry
         * must restore native data/BSS, not read PS1 code into an asset buffer.
         * Fail explicitly until that reset lifecycle is implemented. */
        if (hm_loaded[i])
            hm_runtime_error("hm_overlay_select", "native overlay reentry is not implemented");
        hm_loaded[i] = 1;
        hm_active[region] = member;
        return 0;
    }
    hm_runtime_error("hm_overlay_select", "unknown overlay member");
}

s32 hm_overlay_active(s32 region) {
    if (region < 0 || region > 1)
        hm_runtime_error("hm_overlay_active", "invalid region");
    return hm_active[region];
}
