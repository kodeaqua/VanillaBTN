/* The original farm uses a 2048-deep OT for world-sorted scenery and actors.
 * Actor sprites can be projected by game C and have no PGXP-tagged XY pointer.
 * They must reach the Hor+ sidecar, unlike sprites in the small 2D/HUD OTs. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
/* Mirror the real host: ONE global RAM mapping contains every OT/packet. */
static u_long arena[2048 + 4 + 5 + 5];
static u_long *world_ot = &arena[0];
static u_long *hud_ot = &arena[2048];
static u_long *world_sprite = &arena[2052];
static u_long *hud_sprite = &arena[2057];

static uint32_t pixel_at(int x) {
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 0;
    uint32_t result = 0;
    const int col = (width - 320) / 2 + x;
    if (col >= 0 && col < width && ps1_video_copy_panorama(&video, 0, 0,
                                                320, 240, frame, (size_t)width))
        result = frame[(size_t)36 * width + col];
    free(frame);
    return result;
}

static void make_sprite(u_long sprite[5]) {
    sprite[0] = 4u << 24;
    sprite[1] = 0x65808080u;     /* raw textured variable SPRT */
    sprite[2] = (35u << 16) | 330u;
    sprite[3] = 0u;
    sprite[4] = (4u << 16) | 4u;
}

int main(void) {
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(arena, sizeof(arena))) return 6;
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    DRAWENV env;
    if (!SetDefDrawEnv(&env, 0, 0, 320, 240)) return 2;
    env.tpage = 3u | (2u << 7);
    if (!PutDrawEnv(&env)) return 3;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) video.vram[y][192+x] = 0x001Fu;
    sdk_gpu_begin_frame();
    ClearOTagR(world_ot, 2048);
    make_sprite(world_sprite);
    addPrim(&world_ot[1024], world_sprite);
    DrawOTag(&world_ot[2047]);
    if (!sdk_gpu_ok() || pixel_at(332) != 0xFFFF0000u) return 4;
    sdk_gpu_begin_frame();
    ps1_video_panorama_clear_rows(0, 240);
    ClearOTagR(hud_ot, 4);
    make_sprite(hud_sprite);
    addPrim(&hud_ot[3], hud_sprite);
    DrawOTag(&hud_ot[3]);
    if (!sdk_gpu_ok() || pixel_at(332) != 0xFF000000u) return 5;
    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_panorama_world_ot_test: farm actor packets extend, HUD packets stay clipped");
    return 0;
}
