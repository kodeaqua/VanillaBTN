#include "enhance/interpolation/interpolation.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_vram_context original, after;
static u32 packets[2][6];
static hm_render_frame draw(int slot, u64 tick, u64 key, int x, int ui) {
    DRAWENV env;
    SetDefDrawEnv(&env, 0, slot * 240, 320, 240);
    env.isbg = 1;
    env.r0 = env.g0 = env.b0 = 0;
    PutDrawEnv(&env);
    hm_render_frame f = {.tick = tick,
                         .key = key,
                         .eligible = 1,
                         .arena = {packets[slot], NULL},
                         .arena_bytes = {sizeof(packets[slot]), 0}};
    u32 *p = packets[slot];
    p[0] = 0x600000ff;
    p[1] = (2u << 16) | (u32)x;
    p[2] = (4u << 16) | 4;
    p[3] = 0x6000ff00;
    p[4] = (12u << 16) | (u32)ui;
    p[5] = (2u << 16) | 2;
    hm_render_begin(&f);
    hm_render_group(1);
    hm_gpu_execute_words(p, 3);
    hm_render_packet(p, 3);
    hm_render_group(0);
    hm_gpu_execute_words(p + 3, 3);
    hm_render_packet(p + 3, 3);
    hm_render_end();
    return f;
}
static u16 pixel(hm_interpolated_view *v, int x, int y) {
    return v->pixels[(v->area.y + y) * v->stride + v->area.x + x];
}
int main(int argc, char **argv) {
    hm_vram_reset();
    ResetGraph(0);
    CHECK(hm_interpolation_enable(1));
    hm_interpolated_view view;
    hm_render_frame live = draw(0, 2, 1, 2, 14);
    CHECK(!hm_interpolation_render(33333333, &live, &view));
    live = draw(1, 4, 1, 6, 16);
    hm_vram_snapshot(&original);
    hm_gpu_environment environment = *hm_gpu_environment_state();
    u32 saved[2][6];
    memcpy(saved, packets, sizeof(saved));
    u64 ticks = hm_timing_ticks();
    CHECK(hm_interpolation_render(83333334, &live, &view));
    CHECK(pixel(&view, 4, 2) == 31 && pixel(&view, 7, 2) == 31);
    CHECK(pixel(&view, 2, 2) == 0 && pixel(&view, 8, 2) == 0);
    CHECK(pixel(&view, 16, 12) == (31 << 5) && pixel(&view, 15, 12) == 0);
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&original, &after, sizeof(original)));
    CHECK(!memcmp(&environment, hm_gpu_environment_state(), sizeof(environment)));
    CHECK(!memcmp(saved, packets, sizeof(saved)) && ticks == hm_timing_ticks());
    CHECK(hm_interpolation_render(66666666, &live, &view) && pixel(&view, 2, 2) == 31);
    CHECK(hm_interpolation_render(100000001, &live, &view) && pixel(&view, 6, 2) == 31);
    if (argc == 2 && !strcmp(argv[1], "--benchmark")) {
        clock_t start = clock();
        u32 checksum = 0;
        for (u32 i = 0; i < 10000; ++i) {
            CHECK(hm_interpolation_render(66666666 + (i % 33333) * 1000, &live, &view));
            checksum += pixel(&view, 4, 2);
        }
        printf("Private 320x240 replay, two rectangles: %.4f ms/render, checksum=%u\n",
               (double)(clock() - start) * 1000 / CLOCKS_PER_SEC / 10000, checksum);
    }
    live.key = 2;
    CHECK(!hm_interpolation_render(83333334, &live, &view));
    live = draw(0, 6, 2, 3, 14);
    CHECK(!hm_interpolation_render(116666667, &live, &view));
    live = draw(1, 10, 2, 7, 16);
    CHECK(!hm_interpolation_render(183333334, &live, &view));
    /* A camera jump is drawn at the current native position, not smeared. */
    live = draw(0, 12, 2, 24, 16);
    CHECK(hm_interpolation_render(216666667, &live, &view));
    CHECK(pixel(&view, 24, 2) == 31 && pixel(&view, 15, 2) == 0);
    /* A material change must not inherit the previous object's motion. */
    hm_render_invalidate();
    live = draw(0, 14, 2, 2, 14);
    packets[0][0] = 0x600000aa;
    /* Capture the changed material using the same key, then the next frame. */
    hm_render_begin(&live);
    hm_render_group(1);
    hm_render_packet(packets[0], 3);
    hm_render_end();
    live = draw(1, 16, 2, 6, 16);
    CHECK(hm_interpolation_render(283333334, &live, &view));
    CHECK(pixel(&view, 6, 2) == 31 && pixel(&view, 4, 2) == 0);
    hm_render_invalidate();
    CHECK(!hm_interpolation_render(183333334, &live, &view));
    CHECK(hm_interpolation_enable(0));
    hm_vram_snapshot(&original);
    hm_render_begin(&live);
    hm_render_packet(packets[0], 3);
    hm_render_end();
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&original, &after, sizeof(original)));
    CHECK(!hm_interpolation_render(183333334, &live, &view));
    return 0;
}
