#include "sdk_gpu.h"
#include "sdk_work.h"
#include "main.h"
#include "anim.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* A failed synthetic assertion must return to CTest without CRT dialogs. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

/* Main normally owns this pointer. The fixture supplies a frame without
 * substituting an animation clock, part builder or drawing implementation. */
static FrameState frame;
FrameState *current_frame = &frame;
static Ps1Video video;
static u_long packets[128], ot[4];

typedef struct Fixture {
    AnimFile file;
    AnimImage images[2];
    AnimPartList lists[1];
    AnimPart parts[1];
    AnimRange ranges[1];
    AnimFrame frames[2];
} Fixture;

static void prepare(Fixture *fixture, AnimState *state) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->file.magic = 14;
    fixture->file.section[0] = (s32)offsetof(Fixture, images);
    fixture->file.section[1] = (s32)offsetof(Fixture, lists);
    fixture->file.section[2] = (s32)offsetof(Fixture, parts);
    fixture->file.section[3] = (s32)offsetof(Fixture, ranges);
    fixture->file.section[4] = (s32)offsetof(Fixture, frames);
    fixture->images[0] = (AnimImage){64, 0, 1, 1};
    fixture->images[1] = (AnimImage){16, 1, 16, 1};
    fixture->lists[0].count = 1;
    fixture->parts[0].palette = 1;
    fixture->ranges[0].frames = 2;
    fixture->frames[0].duration = 4;
    fixture->frames[1].duration = 2;
    fixture->frames[1].control = ANIM_CONTROL_HOLD;
    anim_bind_file(state, &fixture->file);
    video.vram[0][64] = 0x4321;
    video.vram[1][17] = 0x001F;
    /* Uniform texels isolate original packet generation from the backend's
     * still-incomplete subpixel interpolation. UV orientation is checked in
     * the actual packet fields below, without inventing a hardware oracle. */
    video.vram[1][18] = video.vram[1][19] = video.vram[1][20] = 0x001F;
}

int main(void) {
    Fixture fixture;
    AnimState state;
    sdk_gpu_bind(&video);
    assert(sdk_gpu_map(packets, sizeof(packets)));
    assert(sdk_gpu_map(ot, sizeof(ot)));
    assert(sdk_work_bind(packets, sizeof(packets)));
    prepare(&fixture, &state);
    assert(state.images == fixture.images && state.frames == (u8 *)fixture.frames);
    anim_tick(&state);
    assert(state.frame == 0 && state.ticks == 1);
    anim_tick(&state);
    assert(state.frame == 1);
    anim_tick(&state);
    assert(state.frame == 1 && (state.flags & ANIM_FLAG_ENDED));
    state.status = ANIM_STATUS_PAUSED;
    anim_tick(&state);
    assert(state.frame == 1);
    state.status = 0;
    anim_select_range(&state, 0);
    ClearOTagR(ot, 4);
    anim_draw_ft4(ot, (POLY_FT4 *)packets, &state, 0, 0, 10, 10, 4096, 0, 0, -1, -1);
    DrawOTag(ot + 3);
    assert(sdk_gpu_ok());
    assert(video.vram[10][10] == 0x001F && video.vram[10][13] == 0x001F);
    assert(((POLY_FT4 *)packets)->u0 == 0 && ((POLY_FT4 *)packets)->u1 == 3);
    fixture.parts[0].attr = 1; /* Original oriented-UV path. */
    ClearOTagR(ot, 4);
    frame.primCursor.ft4 = (POLY_FT4 *)packets;
    sprite_draw_frame(ot, (POLY_FT4 *)packets, &state, 0, 0, 20, 20,
                      4096, 4096, 0, ANIM_DRAW_CURSOR, -1, -1, -1, -1);
    DrawOTag(ot + 3);
    assert(sdk_gpu_ok() && frame.primCursor.ft4 == (POLY_FT4 *)packets + 1);
    assert(video.vram[20][20] == 0x001F && video.vram[20][23] == 0x001F);
    assert(((POLY_FT4 *)packets)->u0 == 3 && ((POLY_FT4 *)packets)->u1 == 0);
    ClearOTagR(ot, 4);
    SPRT *sprites = (SPRT *)packets;
    DR_MODE *modes = (DR_MODE *)(packets + 16);
    anim_draw_sprt_parts(ot, &sprites, &modes, &state, 0, 0, 30, 30);
    DrawOTag(ot + 3);
    assert(sdk_gpu_ok());
    assert(video.vram[30][30] == 0x001F && video.vram[30][33] == 0x001F);
    sdk_gpu_bind(NULL);
    assert(sdk_work_bind(NULL, 0));
    return 0;
}
