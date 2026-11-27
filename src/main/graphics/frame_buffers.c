/* The frame records: linking primitives into the OT, the debug hook, and laying out
 * both frames' OTs and primitive buffers. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "frame_buffers.h"
#include "main.h"

#ifdef __psyz
/* The native entries would overlap the primitive buffers at the PS1 offsets. */
static OT_TYPE frame_main_ots[2][FRAME_OT_DEPTH];
#else
/* An OT entry is a tag in its top byte and the next entry's address in the other 24 bits. */
#define OT_TAG_MASK 0xFF000000
#define OT_ADDRESS_MASK 0x00FFFFFF
#endif

/* Link a primitive into the ordering table at `index` and advance the primitive cursor. */
void *ot_insert_prim(s32 index, s32 advance) {
    FrameState *state;
#ifdef __psyz
    void *current;
#else
    u_long *current;
#endif
    OT_TYPE *entry;
    s32 limit;

    state = current_frame;
    debug_hook_run(0x80);
    limit = state->otDepth;
    if (index >= limit) {
        index = limit - 1;
    }
    entry = &state->ot[index];
#ifdef __psyz
    current = state->primCursor.bytes;
    addPrim(entry, current);
#else
    current = state->primCursor.tag;
    *current = (*current & OT_TAG_MASK) | (*entry & OT_ADDRESS_MASK);
    *entry = (u_long)current & OT_ADDRESS_MASK;
#endif
    state->primCursor.bytes += advance;
    return current;
}

void debug_hook_run(s32 unused0) {
    /* Empty hook. */
}

/* Lay out and clear both frame buffers' main ordering tables and primitive buffers. */
void frame_buffers_layout(void) {
    s32 i;
    s32 j;
    FrameState *fb;
    u8 *region;

    region = region_base;
    for (i = 0; i < 2; i++) {
        fb = &frame_records[i];
        fb->otDepth = FRAME_OT_DEPTH;
        fb->otDepthBits = 11;
        fb->primSize = FRAME_PRIM_SIZE;
#ifdef __psyz
        fb->ot = frame_main_ots[i];
#else
        fb->ot = (OT_TYPE *)&region[i * 0x2000 + REGION_OT];
#endif
        fb->primStart = &region[i * FRAME_PRIM_SIZE + REGION_PRIM];
        ClearOTagR(fb->ot, FRAME_OT_DEPTH);
        for (j = 0; j < FRAME_PRIM_SIZE; j++) {
            fb->primStart[j] = 0;
        }
        fb->primCursor.bytes = fb->primStart;
    }
}
