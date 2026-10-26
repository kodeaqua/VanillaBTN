/* Check actual background dispatch with capturing render sinks, no game data. */
#include "../../src/main/ui/bg_tile.c"
#include <stdio.h>

FarmLoadArea farm_area;
AnimState ui_icon_anim;
static FrameState hm_frame;
FrameState *current_frame = &hm_frame;
static u_long hm_ot;
static int hm_tiles, hm_binds, hm_bad, hm_scroll, hm_mode;

void anim_bind_file(AnimState *state, AnimFile *file) {
    if (state != &ui_icon_anim || file != (AnimFile *)farm_area.stageData)
        hm_bad = 1;
    ++hm_binds;
}
static void hm_capture(u_long *ot, AnimState *state, u8 anim, u8 frame, s16 x, s16 y) {
    static const int frames[] = {2, 1, 3, 0};
    int index = hm_tiles++;
    if (ot != &hm_ot || state != &ui_icon_anim || anim != 0x1D || frame != frames[index % 4] ||
        x != -12 + index % 15 * 24 - hm_scroll || y != index / 15 * 24 - hm_scroll)
        hm_bad = 1;
}
void anim_draw_sprt_cursor(u_long *ot, AnimState *state, u8 anim, u8 frame, s16 x, s16 y) {
    if (hm_mode != 0)
        hm_bad = 1;
    hm_capture(ot, state, anim, frame, x, y);
}
void anim_draw_sprt(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *state, u8 anim,
                    u8 frame, s16 x, s16 y) {
    if (hm_mode != 1 || sprites != &hm_frame.primCursor.sprt || modes != &hm_frame.primCursor.mode)
        hm_bad = 1;
    hm_capture(ot, state, anim, frame, x, y);
}
int main(void) {
    for (int call = 1; call <= 4; ++call) {
        hm_tiles = 0;
        hm_scroll = call / 4;
        hm_mode = call % 2;
        if (hm_mode)
            bg_tile_mode_cursor(&hm_ot);
        else
            bg_tile_mode_sprite(&hm_ot);
        if (hm_bad || hm_tiles != 165 || hm_binds != call) {
            fprintf(stderr, "Background callback/coordinates mismatch at call %d\n", call);
            return 1;
        }
    }
    return 0;
}
