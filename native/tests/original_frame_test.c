#include "sdk_gpu.h"
#include "main.h"
#include "anim.h"
#include "frame_buffers.h"
#include "loading_screen.h"
#include <assert.h>
#include <stddef.h>
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
s32 frame_index;
u8 *region_base = (u8 *)storage;

typedef struct LoadingFixture {
    AnimFile file;
    AnimImage images[2];
    AnimPartList lists[1];
    AnimPart parts[1];
    AnimRange ranges[1];
    AnimFrame frames[1];
} LoadingFixture;

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

    assert(SetDefDrawEnv(&frame_records[0].drawEnv, 0, 0, 320, 240));
    assert(SetDefDrawEnv(&frame_records[1].drawEnv, 0, 240, 320, 240));
    assert(SetDefDispEnv(&frame_records[0].dispEnv, 0, 240, 320, 240));
    assert(SetDefDispEnv(&frame_records[1].dispEnv, 0, 0, 320, 240));

    LoadingFixture *fixture = (LoadingFixture *)loading_data.files;
    memset(&loading_data, 0, sizeof(loading_data));
    fixture->file.magic = 14;
    fixture->file.section[0] = (s32)offsetof(LoadingFixture, images);
    fixture->file.section[1] = (s32)offsetof(LoadingFixture, lists);
    fixture->file.section[2] = (s32)offsetof(LoadingFixture, parts);
    fixture->file.section[3] = (s32)offsetof(LoadingFixture, ranges);
    fixture->file.section[4] = (s32)offsetof(LoadingFixture, frames);
    fixture->images[0] = (AnimImage){64, 0, 1, 1};
    fixture->images[1] = (AnimImage){16, 1, 16, 1};
    fixture->lists[0].count = 1;
    fixture->parts[0].palette = 1;
    fixture->ranges[0].frames = 1;
    fixture->frames[0].duration = 4;

    /* Pack two raw VRAM blocks: 4bpp texel 0x4321 at (64,0) and CLUT 1..2 at (17,1). */
    static const u8 pack[] = {
        0x0D, 0, 2, 0,
        2, 0, 0, 4, 64, 0, 0, 0, 1, 0, 1, 0, 0x21, 0x43, 0, 0,
        4, 0, 0, 4, 17, 0, 1, 0, 2, 0, 1, 0, 0x1F, 0x00, 0xE0, 0x03,
    };
    loading_data.packStart = (s32)sizeof(LoadingFixture);
    memcpy(loading_data.files + loading_data.packStart, pack, sizeof(pack));

    current_frame = &frame_records[0];
    frame_index = 0;
    loading_screen_init();
    assert(loading_screen_is_active() == 1);
    assert(!sdk_gpu_display().enabled);
    assert(video.vram[0][64] == 0x4321 && video.vram[1][17] == 0x001F && video.vram[1][18] == 0x03E0);

    loading_screen_draw();
    assert(sdk_gpu_ok() && sdk_gpu_display().enabled);
    assert(current_frame == &frame_records[1] && frame_index == 1);
    assert(sdk_gpu_display().y == 0);

    loading_screen_draw();
    assert(sdk_gpu_ok());
    assert(current_frame == &frame_records[0] && frame_index == 0);
    assert(sdk_gpu_display().y == 240);
    assert(video.vram[240 + 120][160] == 0x001F && video.vram[240 + 120][161] == 0x03E0);

    loading_screen_stop();
    assert(loading_screen_is_active() == 0);
    loading_screen_draw();
    assert(current_frame == &frame_records[0] && frame_index == 0);

    sdk_gpu_bind(NULL);
    return 0;
}
