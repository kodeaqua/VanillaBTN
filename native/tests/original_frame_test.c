#include "sdk_gpu.h"
#include "main.h"
#include "frame_buffers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

static Ps1Video video;
static u_long storage[(REGION_PRIM + FRAME_PRIM_SIZE * 2) / 4];
FrameState frame_records[2];
FrameState *current_frame;
u8 *region_base = (u8 *)storage;

int main(void) {
    sdk_gpu_bind(&video);
    assert(sdk_gpu_map(storage, sizeof(storage)));
    memset(storage, 0xAA, sizeof(storage));
    frame_buffers_layout();
    assert(sdk_gpu_ok());
    for (unsigned i = 0; i < 2; ++i) {
        FrameState *frame = frame_records + i;
        assert(frame->ot == (u_long *)(region_base + i * 0x2000 + REGION_OT));
        assert(frame->primStart == region_base + i * FRAME_PRIM_SIZE + REGION_PRIM);
        assert(frame->otDepth == 2048 && frame->otDepthBits == 11);
        assert(frame->primCursor.bytes == frame->primStart);
        for (unsigned j = 0; j < FRAME_PRIM_SIZE; ++j) assert(frame->primStart[j] == 0);
        current_frame = frame;
        /* Original cursor insertion preserves the tag's length byte and caps
         * depths above the table. Geometry bytes here are synthetic GP0 input. */
        u_long *packet = frame->primCursor.tag;
        packet[0] = 2ul << 24;
        packet[1] = i == 0 ? 0x680000FFul : 0x68FF0000ul;
        packet[2] = (u_long)(10 + i) | ((u_long)(20 + i) << 16);
        assert(ot_insert_prim(9999, 12) == packet);
        assert(frame->primCursor.bytes == frame->primStart + 12);
        DrawOTag(frame->ot + FRAME_OT_DEPTH - 1);
        assert(sdk_gpu_ok());
        assert(video.vram[20 + i][10 + i] == (i == 0 ? 0x001F : 0x7C00));
    }
    sdk_gpu_bind(NULL);
    return 0;
}
