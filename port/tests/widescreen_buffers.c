#include "enhance/widescreen/widescreen.h"
#include "gfx/game_frame.h"
#include "main.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
FrameState frame_records[2];
FrameState *current_frame;
static int world;
static void *cursor;
hm_render_frame hm_game_render_frame(void) { return (hm_render_frame){.world_view = world}; }
void GsSetWorkBase(void *base) { cursor = base; }
void hm_game_set_work_base(void *base);
static u8 work[2][24000], primitive[2][96000];
int main(void) {
    for (int i = 0; i < 2; ++i) {
        frame_records[i].drawEnv.clip.w = 320;
        frame_records[i].drawEnv.clip.h = 240;
        frame_records[i].workBase = work[i];
        frame_records[i].workSize = 24000;
        frame_records[i].primStart = frame_records[i].primBuffer = primitive[i];
        frame_records[i].primSize = 96000;
    }
    current_frame = frame_records;
    world = 1;
    hm_widescreen_configure(0, 1920, 1080);
    hm_game_set_work_base(work[0]);
    CHECK(cursor == work[0]);
    hm_widescreen_configure(1, 1920, 1080);
    hm_game_set_work_base(work[0]);
    CHECK(cursor == current_frame->workBase && cursor != work[0] &&
          current_frame->workSize == 262144);
    /* Only primStart moves; primBuffer/primSize keep the original region for overlay backups. */
    CHECK(current_frame->primSize == 96000 && current_frame->primBuffer == primitive[0]);
    CHECK(current_frame->primStart != primitive[0] && hm_game_prim_capacity() == 1048576);
    u8 *wide_start = current_frame->primStart;
    CHECK(!hm_widescreen_prim_full(wide_start + 96000));
    CHECK(!hm_widescreen_prim_full(wide_start + 1048576 - 52 - 8192));
    CHECK(hm_widescreen_prim_full(wide_start + 1048576 - 52 - 8192 + 1));
    u8 *next = (u8 *)cursor + 128;
    hm_game_set_work_base(next);
    CHECK(cursor == next); /* Allocation must advance. */
    hm_widescreen_configure(0, 1920, 1080);
    hm_game_set_work_base(current_frame->workBase);
    CHECK(cursor == work[0] && current_frame->workSize == 24000);
    CHECK(current_frame->primStart == primitive[0] && hm_game_prim_capacity() == 96000);
    current_frame = &frame_records[1];
    hm_widescreen_configure(1, 1920, 1080);
    hm_game_set_work_base(work[1]);
    CHECK(cursor != work[1]);
    world = 0;
    hm_game_set_work_base(current_frame->workBase);
    CHECK(cursor != work[1] && hm_widescreen_margin());
    hm_widescreen_configure(0, 1920, 1080);
    hm_game_set_work_base(current_frame->workBase);
    CHECK(cursor == work[1] && !hm_widescreen_margin());
    CHECK(current_frame->primStart == primitive[1] && current_frame->primBuffer == primitive[1]);
    puts("Wide GS arenas preserve allocation cursor, OFF layout and overlay backup primitives");
    return 0;
}
