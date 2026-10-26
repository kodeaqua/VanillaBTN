#include "enhance/filtering/sample.h"
#include "gfx/software_output.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/timing.h"
#include "psx/vram.h"
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
int main(void) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV d;
    SetDefDrawEnv(&d, 0, 0, 32, 32);
    d.dtd = 0;
    d.dfe = 1;
    d.tpage = 282;
    PutDrawEnv(&d);
    DISPENV disp;
    SetDefDispEnv(&disp, 0, 0, 32, 32);
    PutDispEnv(&disp);
    hm_vram_draw_pixel(640, 256, 31);
    hm_vram_draw_pixel(641, 256, 31 << 5);
    hm_vram_draw_pixel(640, 257, 31 << 10);
    hm_vram_draw_pixel(641, 257, 32767);
    hm_texture_bounds bounds = {0, 0, 1, 1};
    hm_texture_shading s = {
        .opcode = 0x65, .rectangle = 1, .light = {128, 128, 128}, .background = 31 << 10};
    u16 result = 0;
    hm_vram_snapshot(&before);
    u64 ticks = hm_timing_ticks();
    CHECK(hm_texture_bilinear(128, 0, bounds, 0, &s, &result) && result == (16 | (16 << 5)));
    CHECK(hm_texture_bilinear(128, 128, bounds, 0, &s, &result) && result == 0x4210);
    CHECK(hm_texture_bilinear(-128, 0, bounds, 0, &s, &result) && result == 31);
    CHECK(hm_texture_bilinear(384, 0, bounds, 0, &s, &result) && result == (31 << 5));
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&before, &after, sizeof(before)) && ticks == hm_timing_ticks());
    hm_vram_draw_pixel(641, 256, 0);
    bounds.max_v = 0;
    CHECK(hm_texture_bilinear(128, 0, bounds, 0, &s, &result) && result == (16 | (16 << 10)));
    result = 0x1234;
    CHECK(!hm_texture_bilinear(256, 0, bounds, 0, &s, &result) && result == 0x1234);
    hm_vram_draw_pixel(641, 256, 0x8000);
    CHECK(hm_texture_bilinear(256, 0, bounds, 0, &s, &result) && result == 0x8000);
    hm_vram_draw_pixel(641, 256, 0x8000 | (31 << 5));
    s.opcode = 0x67;
    CHECK(hm_texture_bilinear(128, 0, bounds, 0, &s, &result) &&
          result == (16 | (8 << 5) | (8 << 10)));
    hm_vram_draw_pixel(640, 256, 0x801f);
    s.background = 31 << 5;
    const u16 blends[] = {0x8000 | 15 | (15 << 5), 0x8000 | 31 | (31 << 5), 0x8000 | (31 << 5),
                          0x8000 | 7 | (31 << 5)};
    for (u32 mode = 0; mode < 4; ++mode) {
        d.tpage = 282 | (mode << 5);
        PutDrawEnv(&d);
        CHECK(hm_texture_bilinear(0, 0, bounds, 0, &s, &result) && result == blends[mode]);
    }
    s.opcode = 0x65;
    s.background = 0;
    hm_vram_draw_pixel(896, 252, 31);
    hm_vram_draw_pixel(897, 252, 31 << 5);
    u16 clut = (252 << 6) | 56;
    for (u32 depth = 0; depth < 2; ++depth) {
        d.tpage = 26 | (depth << 7);
        PutDrawEnv(&d);
        hm_vram_draw_pixel(640, 256, depth ? 0x0100 : 0x0010);
        CHECK(hm_texture_bilinear(128, 0, bounds, clut, &s, &result) && result == (16 | (16 << 5)));
    }
    d.tpage = 282;
    d.tw.w = 224;
    d.tw.x = 32;
    PutDrawEnv(&d);
    hm_vram_draw_pixel(703, 256, 31);
    hm_vram_draw_pixel(672, 256, 31 << 5);
    CHECK(
        hm_texture_bilinear(31 * 256 + 128, 0, (hm_texture_bounds){31, 0, 32, 0}, 0, &s, &result) &&
        result == (16 | (16 << 5)));
    /* Shared output supports filtering with no scaling module or game data. */
    d.tw.w = 256;
    d.tw.x = 0;
    PutDrawEnv(&d);
    hm_vram_draw_pixel(640, 256, 31);
    hm_vram_draw_pixel(641, 256, 31 << 5);
    hm_software_output *output = hm_software_output_create();
    CHECK(output);
    hm_software_output_filter(output, 1);
    CHECK(hm_software_output_prepare(output, 64, 64));
    hm_render_frame live = {.key = 1, .tick = 2};
    u32 sprite[] = {0x65000000, 0, 0, 0x00010002};
    hm_render_begin(&live);
    hm_gpu_execute_words(sprite, 4);
    hm_render_packet(sprite, 4);
    hm_render_end();
    hm_vram_snapshot(&before);
    hm_gpu_environment env = *hm_gpu_environment_state();
    ticks = hm_timing_ticks();
    hm_replay_sink previous = hm_software_output_begin(output);
    hm_software_view view;
    CHECK(hm_software_output_replay(output, &live) &&
          hm_software_output_end(output, previous, &view));
    CHECK(view.pixels[1] == (23 | (8 << 5)));
    hm_software_output_filter(output, 0);
    previous = hm_software_output_begin(output);
    CHECK(hm_software_output_replay(output, &live) &&
          hm_software_output_end(output, previous, &view) && view.pixels[1] == 31);
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&before, &after, sizeof(before)) && ticks == hm_timing_ticks());
    CHECK(!memcmp(&env, hm_gpu_environment_state(), sizeof(env)));
    hm_software_output_destroy(output);
    return 0;
}
