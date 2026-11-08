#include "sdk_gpu.h"
#include "main.h"
#include "screen_fade.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

/* Main-loop state and controls are fixture boundaries. Fade state, counters,
 * packet storage and builders all come from the actual screen_fade.c. */
FrameState frame_records[2];
FrameState *current_frame = frame_records;
s32 frame_index;
static u_long ot[2];
u_long *active_ot = ot;
InputRecord *input_pad1;
u8 main_get_speed_mode(void) { return MAIN_SPEED_FAST_2; }
void main_set_clear_screen(s32 value) { (void)value; }
void main_set_put_env(s32 value) { (void)value; }
static Ps1Video video;

static void draw_frame(void) {
    sdk_gpu_reset_packets();
    ClearOTagR(ot, 2); /* Native SDK maps a module-owned OT. */
    screen_clear_rect(0, 0, 320, 240, 255, 0, 0);
    screen_fade_frame();
    DrawOTag(ot + 1);
    CHECK(sdk_gpu_ok());
}

int main(void) {
    sdk_gpu_bind(&video);
    DRAWENV draw;
    CHECK(SetDefDrawEnv(&draw, 0, 0, 320, 240));
    CHECK(PutDrawEnv(&draw));
    fade_reset();
    fade_start_out(3, 0, 2);
    CHECK(fade_get_mode() == SCREEN_FADE_OUT && !fade_is_settled());
    draw_frame();
    CHECK(video.vram[10][10] == 24);
    draw_frame();
    CHECK(video.vram[10][10] == 16);
    draw_frame();
    CHECK(video.vram[10][10] == 8);
    draw_frame();
    CHECK(video.vram[10][10] == 0);
    draw_frame();
    CHECK(fade_is_settled());
    fade_start_in(4, 0, 2);
    draw_frame();
    CHECK(video.vram[10][10] == 16);
    draw_frame();
    CHECK(video.vram[10][10] == 24);
    draw_frame();
    CHECK(video.vram[10][10] == 31 && fade_is_settled());
    sdk_gpu_bind(NULL);
    return 0;
}
