#include "enhance/scaling/scaling.h"
#include "gfx/software_raster.h"
#ifdef TEST_INTERPOLATION
#include "enhance/interpolation/interpolation.h"
#endif
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
static u16 pixels[4096 * 2160];
static hm_vram_context original, after;
static u32 xy(int x, int y) { return (u16)x | ((u32)(u16)y << 16); }
static void setup(void) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 16, 16);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    for (u32 y = 0; y < 16; ++y)
        for (u32 x = 0; x < 16; ++x)
            hm_vram_draw_pixel(640 + x, 256 + y, (u16)(1 + ((x + y * 3) % 31)));
}
static hm_software_surface raster(const u32 *words, u32 count, u32 width, u32 height) {
    hm_software_surface surface = {.pixels = pixels, .width = width, .height = height};
    hm_replay_sink previous = hm_replay_bind(hm_software_sink(&surface));
    RECT area = {0, 0, 16, 16};
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    hm_replay_bind(previous);
    return surface;
}
static int parity(const u32 *words, u32 count) {
    setup();
    hm_software_surface surface = raster(words, count, 16, 16);
    if (!surface.begun)
        return 0;
    for (u32 y = 0; y < 16; ++y)
        for (u32 x = 0; x < 16; ++x)
            if (pixels[y * 16 + x] != hm_vram_pixel(x, y))
                return 0;
    return 1;
}
static hm_render_frame capture(int slot, u64 tick, int x) {
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, slot * 240, 16, 16);
    draw.dtd = 0;
    draw.isbg = 1;
    PutDrawEnv(&draw);
    static u32 packet[2][3];
    packet[slot][0] = 0x600000ff;
    packet[slot][1] = xy(x, 2);
    packet[slot][2] = xy(4, 4);
    hm_render_frame frame = {.key = 1,
                             .tick = tick,
                             .eligible = 1,
                             .arena = {packet[slot], NULL},
                             .arena_bytes = {sizeof(packet[slot]), 0}};
    hm_render_begin(&frame);
    hm_render_group(1);
    hm_gpu_execute_words(packet[slot], 3);
    hm_render_packet(packet[slot], 3);
    hm_render_end();
    return frame;
}
int main(int argc, char **argv) {
    /* At 1x, the optional rasterizer must reproduce native supported commands. */
    u32 fixture[] = {0xe1000000 | 282, 0x200000ff, 0, 0,         0, 0x6000ff00,        0,
                     0x00020002,       0x25000000, 0, 0,         0, (282u << 16) | 12, 0,
                     0x000c00,         0x620000ff, 0, 0x00020002};
    fixture[2] = xy(1, 1);
    fixture[3] = xy(14, 3);
    fixture[4] = xy(4, 14);
    fixture[6] = xy(10, 10);
    fixture[9] = xy(2, 2);
    fixture[11] = xy(12, 2);
    fixture[13] = xy(2, 12);
    fixture[16] = xy(10, 10);
    setup();
    hm_software_surface surface = raster(fixture, sizeof(fixture) / sizeof(*fixture), 16, 16);
    CHECK(surface.begun);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            CHECK(pixels[y * 16 + x] == hm_vram_pixel((u32)x, (u32)y));
    u32 shaded[] = {0xe1000200, 0x300000ff, xy(1, 1), 0x0000ff00, xy(14, 3), 0x00ff0000, xy(4, 14)};
    CHECK(parity(shaded, sizeof(shaded) / sizeof(*shaded)));
    u32 quad[] = {0x2800ff00, xy(2, 1), xy(14, 3), xy(1, 13), xy(12, 14)};
    CHECK(parity(quad, sizeof(quad) / sizeof(*quad)));
    u32 lines[] = {0xe1000200, 0x400000ff, xy(1, 1), xy(12, 9), 0x500000ff, xy(1, 14), 0x00ff0000,
                   xy(14, 1),  0x4800ff00, xy(1, 2), xy(4, 12), xy(14, 14), 0x55555555};
    CHECK(parity(lines, sizeof(lines) / sizeof(*lines)));
    u32 images[] = {0x020000ff, xy(2, 2), xy(8, 8),   0xa0000000,
                    xy(4, 4),   xy(2, 2), 0x03e0001f, 0x7c0003e0};
    CHECK(parity(images, sizeof(images) / sizeof(*images)));
    u32 clipped[] = {0xe3000000 | 3 | (2 << 10),
                     0xe4000000 | 10 | (12 << 10),
                     0x200000ff,
                     xy(-8, -8),
                     xy(16, 0),
                     xy(5, 15)};
    CHECK(parity(clipped, sizeof(clipped) / sizeof(*clipped)));
    /* Higher-resolution diagonal coverage differs from enlarging native pixels. */
    u32 triangle[] = {0x200000ff, xy(1, 1), xy(14, 3), xy(4, 14)};
    setup();
    surface = raster(triangle, 4, 32, 32);
    int differences = 0;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            if (pixels[y * 32 + x] != hm_vram_pixel((u32)x / 2, (u32)y / 2))
                ++differences;
    CHECK(differences > 10);
    /* Tiny logical views at large output scale cannot overflow attributes. */
    setup();
    DRAWENV tiny;
    SetDefDrawEnv(&tiny, 0, 0, 1, 1);
    tiny.dtd = 0;
    tiny.dfe = 1;
    PutDrawEnv(&tiny);
    hm_software_surface extreme = {.pixels = pixels, .width = 4096, .height = 128};
    hm_replay_sink saved_sink = hm_replay_bind(hm_software_sink(&extreme));
    RECT tiny_area = {0, 0, 1, 1};
    hm_replay_begin(&tiny_area);
    u32 large[] = {0x300000ff, xy(0, 0), 0x0000ff00, xy(1023, 0), 0x00ff0000, xy(0, 511)};
    hm_gpu_execute_words(large, 6);
    hm_replay_bind(saved_sink);
    CHECK(extreme.begun && pixels[0] != 0);
    /* Clipping, negative coordinates, all blend modes, mask-set/check. */
    for (u32 mode = 0; mode < 4; ++mode) {
        setup();
        u32 blend[] = {0xe1000000 | (mode << 5),
                       0x600000ff,
                       xy(-1, -1),
                       xy(9, 9),
                       0x6200ff00,
                       xy(2, 2),
                       xy(8, 8),
                       0xe6000001,
                       0x600000ff,
                       xy(3, 3),
                       xy(2, 2),
                       0xe6000002,
                       0x60ff0000,
                       xy(3, 3),
                       xy(2, 2)};
        surface = raster(blend, sizeof(blend) / sizeof(*blend), 16, 16);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                CHECK(pixels[y * 16 + x] == hm_vram_pixel((u32)x, (u32)y));
    }
    CHECK(hm_scaling_enable(1) && hm_scaling_prepare(32, 32));
    CHECK(!hm_scaling_prepare(0, 32) && !hm_scaling_prepare(4097, 32));
    setup();
    hm_render_frame live = capture(0, 2, 2);
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, 16, 16);
    PutDispEnv(&display);
    hm_vram_snapshot(&original);
    hm_gpu_environment env = *hm_gpu_environment_state();
    u64 ticks = hm_timing_ticks();
    hm_replay_sink previous = hm_scaling_begin();
    hm_scaled_view view;
    CHECK(hm_scaling_replay(&live) && hm_scaling_end(previous, &view));
    CHECK(view.width == 32 && view.height == 32 && view.pixels[4 * 32 + 4] == 31);
    /* Repeating a scanout retains the high-resolution result, including after
     * switching from an intervening native-only loading presentation. */
    previous = hm_scaling_begin();
    CHECK(hm_scaling_replay(&live) && hm_scaling_end(previous, &view));
    live.bypass = 1;
    previous = hm_scaling_begin();
    CHECK(!hm_scaling_replay(&live) && !hm_scaling_end(previous, &view));
    live.bypass = 0;
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&original, &after, sizeof(original)));
    CHECK(!memcmp(&env, hm_gpu_environment_state(), sizeof(env)) && ticks == hm_timing_ticks());
#ifdef TEST_INTERPOLATION
    CHECK(hm_interpolation_enable(1));
    live = capture(0, 2, 2);
    live = capture(1, 4, 6);
    previous = hm_scaling_begin();
    hm_interpolated_view interpolated;
    CHECK(hm_interpolation_render(83333334, &live, &interpolated));
    CHECK(hm_scaling_end(previous, &view) && view.pixels[4 * 32 + 8] == 31 &&
          view.pixels[4 * 32 + 4] == 0);
    CHECK(hm_interpolation_enable(0)); /* Must retain the scaling subscription. */
    live = capture(0, 6, 2);
    PutDispEnv(&display);
    previous = hm_scaling_begin();
    CHECK(hm_scaling_replay(&live) && hm_scaling_end(previous, &view));
#endif
    hm_render_invalidate();
    previous = hm_scaling_begin();
    CHECK(!hm_scaling_replay(&live) && !hm_scaling_end(previous, &view));
    CHECK(hm_scaling_enable(0));
    hm_vram_snapshot(&original);
    hm_render_begin(&live);
    hm_render_packet(triangle, 4);
    hm_render_end();
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&original, &after, sizeof(original)));
    if (argc == 2 && !strcmp(argv[1], "--benchmark")) {
        const u32 sizes[][2] = {{320, 240}, {640, 480}, {1440, 1080}};
        for (int i = 0; i < 3; ++i) {
            setup();
            clock_t start = clock();
            for (int frame = 0; frame < 50; ++frame)
                surface = raster(triangle, 4, sizes[i][0], sizes[i][1]);
            printf("One flat triangle, %ux%u private raster: %.3f ms/render\n", surface.width,
                   surface.height, (double)(clock() - start) * 1000 / CLOCKS_PER_SEC / 50);
        }
    }
    return 0;
}
