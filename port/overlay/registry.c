#include "overlay/registry.h"
#include "psx/error.h"
#include <stdlib.h>
#include <string.h>

static const struct {
    s32 member, region;
} hm_overlays[] = {{8, 0},  {10, 0}, {11, 0}, {15, 1}, {17, 0}, {19, 0}, {21, 0},
                   {23, 0}, {25, 0}, {27, 0}, {28, 0}, {30, 1}, {31, 0}};
static u8 hm_loaded[13];
static s32 hm_active[2] = {-1, -1};
static struct {
    hm_overlay_span *spans;
    u8 *initial;
    size_t count;
} hm_state[13];

void hm_overlay_capture(s32 member, const hm_overlay_span *spans, size_t count) {
    for (u32 i = 0; i < 13; ++i) {
        if (hm_overlays[i].member != member)
            continue;
        if (hm_state[i].spans)
            return; /* Never replace pristine state with live state. */
        if (hm_loaded[i] || !spans || !count || count > SIZE_MAX / sizeof(*spans))
            hm_runtime_error("hm_overlay_capture", "invalid or late state capture");
        size_t total = 0;
        for (size_t j = 0; j < count; ++j) {
            if (!spans[j].address || !spans[j].bytes || spans[j].bytes > SIZE_MAX - total)
                hm_runtime_error("hm_overlay_capture", "invalid object span");
            total += spans[j].bytes;
        }
        hm_state[i].spans = malloc(count * sizeof(*spans));
        hm_state[i].initial = malloc(total);
        if (!hm_state[i].spans || !hm_state[i].initial)
            hm_runtime_error("hm_overlay_capture", "cannot allocate pristine overlay state");
        memcpy(hm_state[i].spans, spans, count * sizeof(*spans));
        hm_state[i].count = count;
        size_t offset = 0;
        for (size_t j = 0; j < count; ++j) {
            memcpy(hm_state[i].initial + offset, spans[j].address, spans[j].bytes);
            offset += spans[j].bytes;
        }
        return;
    }
    hm_runtime_error("hm_overlay_capture", "unknown overlay member");
}

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
        /* Restore native data/BSS from relocated C objects. Overlays without
         * a complete audited inventory still reject a second activation. */
        if (hm_loaded[i] && !hm_state[i].spans)
            hm_runtime_error("hm_overlay_select", "native overlay reentry is not implemented");
        size_t offset = 0;
        for (size_t j = 0; j < hm_state[i].count; ++j) {
            hm_overlay_span *span = &hm_state[i].spans[j];
            memcpy(span->address, hm_state[i].initial + offset, span->bytes);
            offset += span->bytes;
        }
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
