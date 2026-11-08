#include "sdk_gpu.h"
#include "sdk_work.h"
#include "main.h"
#include "anim.h"
#include "bg_tile.h"
#include "camera.h"
#include "clut.h"
#include "farm_state.h"
#include "frame_buffers.h"
#include "loading_screen.h"
#include "math_util.h"
#include "ui_icons.h"
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
FarmLoadArea farm_area;

extern u8 camera_mode;
extern u8 camera_ready;
extern s8 camera_dirty;
extern s32 camera_view_x;
extern s32 camera_view_y;
extern s32 camera_view_z;
extern s32 camera_focus_index;
extern u8 ui_icon_cycle;
extern u8 ui_icon_tick;

typedef struct StageAnimFixture {
    AnimFile file;
    AnimImage images[2];
    AnimPartList lists[1];
    AnimPart parts[1];
    AnimRange ranges[44];
    AnimFrame frames[4];
} StageAnimFixture;

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

    /* Original CLUT fade and scroll routines read two 16-entry palettes via
     * StoreImage and queue DR_LOAD (GP0 0xA0) packets through ot_insert_prim. */
    for (unsigned i = 0; i < 16; ++i) {
        video.vram[2][i] = (i == 0) ? 0 : (uint16_t)((4 << 10) | (8 << 5) | 16);
        video.vram[3][i] = (i == 0) ? 0 : (uint16_t)((20 << 10) | (24 << 5) | 0);
    }
    ClutFade fade;
    u_long saved_clut[16];
    clut_fade_setup(&fade, saved_clut, 4, 0, 2, 0, 3, 0, 4);
    assert(fade.span == 4 && fade.position == 0 && fade.direction == -2);
    assert((saved_clut[0] & 0xFFFFu) == 0 && (saved_clut[0] >> 16) == ((4 << 10) | (8 << 5) | 16));
    assert((saved_clut[8] & 0xFFFFu) == 0 && (saved_clut[8] >> 16) == ((20 << 10) | (24 << 5) | 0));

    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    current_frame->primCursor.bytes = current_frame->primStart;
    clut_fade_step(&fade, 2);
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok());
    /* Entry 0 stays transparent 0x0000; non-zero blended entries set STP (0x8000). */
    assert(video.vram[4][0] == 0x0000);
    assert(video.vram[4][1] == (0x8000 | (12 << 10) | (16 << 5) | 8));

    /* Bounce mode (mode 1): steps 0 -> 2 -> 4, flips direction, then 4 -> 2 -> 0. */
    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 2 && fade.direction == 0);
    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 4 && fade.direction == 0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    current_frame->primCursor.bytes = current_frame->primStart;
    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 4 && fade.direction == 1);
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok() && video.vram[4][15] == (0x8000 | (20 << 10) | (24 << 5) | 0));

    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 2 && fade.direction == 1);
    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 0 && fade.direction == 1);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    current_frame->primCursor.bytes = current_frame->primStart;
    clut_scroll_step(&fade, 2, 1);
    assert(fade.position == 0 && fade.direction == 0);
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok() && video.vram[4][15] == (0x8000 | (4 << 10) | (8 << 5) | 16));

    /* Wrap mode (mode 0): wraps position >= span back by span. */
    clut_scroll_step(&fade, 3, 0);
    assert(fade.position == 3);
    clut_scroll_step(&fade, 2, 0);
    assert(fade.position == 1);
    assert(math_int_pow(3, 4) == 81 && math_int_pow(5, 0) == 1);
    s32 polar[3] = {0};
    vec_from_polar(polar, 800, 600, 512);
    assert(polar[0] == ((2896 * 800) >> 12) && polar[1] == -600 && polar[2] == ((2896 * 800) >> 12));

    camera_init();
    assert(camera_ready == 1 && camera_mode == 2 && camera_dirty == 0);
    assert(camera_focus_index == -1);
    assert(camera_view_x == 800 && camera_view_y == 600 && camera_view_z == 0);

    /* Stage an AnimFile in farm_area.stageData to exercise ui_icons.c and bg_tile.c. */
    StageAnimFixture *stage = (StageAnimFixture *)farm_area.stageData;
    memset(stage, 0, sizeof(*stage));
    stage->file.magic = 14;
    stage->file.section[0] = (s32)offsetof(StageAnimFixture, images);
    stage->file.section[1] = (s32)offsetof(StageAnimFixture, lists);
    stage->file.section[2] = (s32)offsetof(StageAnimFixture, parts);
    stage->file.section[3] = (s32)offsetof(StageAnimFixture, ranges);
    stage->file.section[4] = (s32)offsetof(StageAnimFixture, frames);
    stage->images[0] = (AnimImage){64, 0, 1, 1};
    stage->images[1] = (AnimImage){16, 1, 16, 1};
    stage->lists[0].count = 1;
    stage->parts[0].palette = 1;
    for (unsigned i = 0; i < 44; ++i) {
        stage->ranges[i].frames = 4;
    }
    for (unsigned i = 0; i < 4; ++i) {
        stage->frames[i].duration = 1;
    }

    ui_blink_tick(-1);
    assert(ui_icon_cycle == 0);
    for (unsigned i = 0; i < 121; ++i) {
        ui_blink_tick(0);
    }
    assert(ui_icon_cycle == 1);
    ui_blink_tick_60(-1);
    assert(ui_icon_tick == 0);
    for (unsigned i = 0; i < 61; ++i) {
        ui_blink_tick_60(0);
    }
    assert(ui_icon_tick == 1);

    assert(PutDrawEnv(&current_frame->drawEnv));
    assert(sdk_work_bind(current_frame->primStart, (size_t)current_frame->primSize));
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    ui_draw_icon_2a(&current_frame->ot[0], 20, 30);
    assert(ui_icon_anim.source == &stage->file);
    assert((u8 *)gs_work_alloc(0) ==
           current_frame->primStart + sizeof(SPRT) + sizeof(DR_MODE));
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok() && video.vram[30][20] == 0x001F);

    assert(sdk_work_bind(current_frame->primStart, (size_t)current_frame->primSize));
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    bg_tile_mode_sprite(&current_frame->ot[0]);
    /* 11 rows (y = 0..240 step 24) * 15 cols (x = -12..324 step 24) = 165 tiles. */
    assert((u8 *)gs_work_alloc(0) ==
           current_frame->primStart + 165 * (sizeof(SPRT) + sizeof(DR_MODE)));
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok() && video.vram[0][12] == 0x001F);
    assert(sdk_work_bind(NULL, 0));

    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    current_frame->primCursor.bytes = current_frame->primStart;
    bg_tile_mode_cursor(&current_frame->ot[0]);
    assert(current_frame->primCursor.bytes ==
           current_frame->primStart + 165 * (sizeof(SPRT) + sizeof(DR_MODE)));
    DrawOTag(current_frame->ot + FRAME_OT_DEPTH - 1);
    assert(sdk_gpu_ok());

    sdk_gpu_bind(NULL);
    return 0;
}
