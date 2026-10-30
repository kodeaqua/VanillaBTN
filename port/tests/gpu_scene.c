#include "gfx/gpu_scene.h"
#include "gfx/frame_replay.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include "enhance/dithering/dithering.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_vram_context before, after;
static void setup(int width, int height) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, width, height);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, width, height);
    PutDispEnv(&display);
}
int main(void) {
    hm_gpu_scene *s = hm_gpu_scene_create();
    CHECK(s);
    setup(32, 32);
    u32 rectangle[] = {0x600000ff, 0x00020002, 0x00040004};
    hm_frame_replay *a = hm_frame_replay_acquire(), *b = hm_frame_replay_acquire();
    CHECK(a && b);
    hm_frame_replay_release(a);
    hm_render_frame live = {.key = 9, .tick = 2, .eligible = 0}; /* Not a farm or enhancement. */
    hm_render_begin(&live);
    hm_gpu_execute_words(rectangle, 3);
    hm_render_packet(rectangle, 3);
    hm_render_end();
    CHECK(hm_frame_replay_serial(b, &live));
    u64 displayed_serial = hm_frame_replay_serial(b, &live);
    ++live.key; /* Cutscene state changes before the displayed buffer flips. */
    CHECK(hm_frame_replay_serial(b, &live) == displayed_serial);
    live.tick += 120; /* Blocking CD/audio wait; the display buffer is unchanged. */
    CHECK(hm_frame_replay_serial(b, &live) == displayed_serial);
    DISPENV other;
    SetDefDispEnv(&other, 0, 240, 32, 32);
    PutDispEnv(&other);
    CHECK(!hm_frame_replay_serial(b, &live));
    other.disp.y = 0;
    PutDispEnv(&other);
    hm_vram_snapshot(&before);
    hm_gpu_environment env = *hm_gpu_environment_state();
    u64 ticks = hm_timing_ticks();
    hm_replay_sink previous = hm_gpu_scene_begin(s, 32, 32);
    hm_gpu_scene_data data;
    CHECK(hm_frame_replay_draw(b, &live) && hm_gpu_scene_end(s, previous, &data));
    CHECK(data.width == 32 && data.height == 32 && data.bytes <= HM_SCENE_MAX_WORDS * sizeof(u32));
    CHECK(data.words[10] == 1 && data.vram_pixels[0] == 0);
    CHECK(!(data.words[data.words[8] + 4] & 64));
#ifdef HMBTN_ENH_DITHERING
    hm_dither_set_enabled(0);
    previous = hm_gpu_scene_begin(s, 32, 32);
    CHECK(hm_frame_replay_draw(b, &live) && hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[data.words[8] + 4] & 64);
    hm_dither_set_enabled(1);
#endif
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&before, &after, sizeof(before)));
    CHECK(!memcmp(&env, hm_gpu_environment_state(), sizeof(env)) && ticks == hm_timing_ticks());
    /* Replay restores the live environment before scene_end. The output must
     * keep its own captured backdrop instead of reading the next frame's clear. */
    hm_gpu_environment splash_env = env;
    splash_env.draw.isbg = 1;
    splash_env.draw.r0 = splash_env.draw.g0 = splash_env.draw.b0 = 255;
    hm_gpu_environment *live_env = hm_gpu_environment_bind(&splash_env);
    previous = hm_gpu_scene_begin(s, 32, 32);
    RECT splash_area = splash_env.draw.clip;
    hm_replay_begin(&splash_area);
    hm_gpu_environment_bind(live_env);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    CHECK(data.words[14] == 0x7fff);
    hm_render_invalidate();
    CHECK(!hm_frame_replay_serial(b, &live));
    hm_frame_replay_release(b);
    /* GPU geometry consumes only output: it must not run the CPU raster a second
     * time, while embedded polygon tpages still affect subsequent sprites. */
    setup(32, 32);
    RECT area = {0, 0, 32, 32};
    u32 texture[] = {0x25000000, 0,          0, 0x00000010, (282u << 16) | 8, 0x00100000,
                     0x0800,     0x65000000, 0, 0,          0x00040004};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(texture, 11);
    CHECK(hm_gpu_scene_end(s, previous, &data) && hm_gpu_environment_state()->draw.tpage == 282);
    CHECK(hm_vram_pixel(0, 0) == 0);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 feedback[] = {0x65000000, 0, 0, 0x00040004};
    hm_gpu_execute_words(feedback, 4);
    CHECK(!hm_gpu_scene_end(s, previous, &data) && hm_gpu_scene_fallback(s)[0]);
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(rectangle, 3);
    u32 fill[] = {0x020000ff, 0, 0x00040004};
    hm_gpu_execute_words(fill, 3);
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    for (int i = 0; i < 257; ++i)
        hm_gpu_execute_words(rectangle, 3);
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    setup(1024, 512);
    area = (RECT){0, 0, 1024, 512};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    for (int i = 0; i <= HM_SCENE_PRIMITIVES; ++i) {
        rectangle[1] = (u32)((i % 64) * 16) | ((u32)(((i / 64) % 32) * 16) << 16);
        hm_gpu_execute_words(rectangle, 3);
    }
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    previous = hm_gpu_scene_begin(s, 0, 0);
    hm_replay_begin(&area);
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    /* A clip extending past the viewport is safe when its geometry stays inside. */
    setup(32, 32);
    area = (RECT){0, 0, 32, 32};
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 dialog[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, 0x00020002, 0x00040004};
    hm_gpu_execute_words(dialog, 4);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    /* Page overlaps the framebuffer, but the sampled rows do not. */
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 safe_uv[] = {0xe1000600, 0x65808080, 0x00020002, (getClut(896, 490) << 16) | 0xf000,
                     0x00040004};
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    u32 unsafe_clip[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, 0x00280002, 0x00040004};
    hm_gpu_execute_words(unsafe_clip, 4);
    /* Independent textured draw sampling v=240 (tile y=15) stays on GPU */
    safe_uv[3] = ((u32)getClut(896, 490) << 16) | 0xf000;
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(hm_gpu_scene_end(s, previous, &data));
    /* Dependent textured draw sampling v=40 (tile y=2) falls back to software */
    setup(32, 32);
    previous = hm_gpu_scene_begin(s, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(unsafe_clip, 4);
    safe_uv[3] = ((u32)getClut(896, 490) << 16) | 0x2800;
    hm_gpu_execute_words(safe_uv, 5);
    CHECK(!hm_gpu_scene_end(s, previous, &data));
    /* Multisample locations can cross a 16-pixel bin boundary even when the
     * output pixel centre lies in the neighboring bin. */
    const u32 triangles[2][4] = {{0x200000ff, 0x000c0002, 0x000c000a, 0x00100002},
                                 {0x200000ff, 0x0002000c, 0x00020010, 0x000a000c}};
    for (unsigned samples = 1; samples <= 4; samples *= 2) {
        for (int axis = 0; axis < 2; ++axis) {
            setup(32, 32);
            hm_gpu_scene_multisamples(s, samples);
            previous = hm_gpu_scene_begin(s, 65, 97);
            hm_replay_begin(&area);
            hm_gpu_execute_words(triangles[axis], 4);
            CHECK(hm_gpu_scene_end(s, previous, &data));
            int bin = axis ? 1 : 2;
            CHECK(data.words[data.words[9] + bin * HM_SCENE_BIN_WORDS] ==
                  (samples == 1 ? 0u : 1u));
        }
    }
    hm_gpu_scene_destroy(s);
    return 0;
}
