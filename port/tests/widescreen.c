#include "enhance/widescreen/widescreen.h"
#include "gfx/gpu_scene.h"
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
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_vram_context before, after;
static u32 xy(int x, int y) { return (u16)x | ((u32)(u16)y << 16); }
int main(void) {
    RECT native = {0, 0, 320, 240};
    hm_widescreen_scene(1);
    hm_widescreen_configure(0, 1920, 1080);
    CHECK(!hm_widescreen_margin() && hm_widescreen_angle(2156) == 2156 &&
          hm_widescreen_radius(3600) == 3600);
    hm_widescreen_configure(1, 1920, 1080);
    CHECK(hm_widescreen_margin() == 54);
    RECT wide = hm_widescreen_area(native);
    CHECK(wide.x == -54 && wide.w == 428 && wide.h == 240);
    CHECK(hm_widescreen_angle(2156) > 2156 && hm_widescreen_angle(-108) < -108);
    CHECK(hm_widescreen_radius(3600) == 4815);
    hm_widescreen_configure(1, 2560, 1080);
    CHECK(hm_widescreen_margin() == 125);
    hm_widescreen_configure(1, 1024, 768);
    CHECK(!hm_widescreen_margin());
    hm_widescreen_configure(1, 1080, 1920);
    CHECK(!hm_widescreen_margin());
    hm_widescreen_configure(1, 100000, 1);
    CHECK(hm_widescreen_margin() == 352);
    hm_widescreen_configure(1, 1920, 1080);
    hm_widescreen_scene(0);
    CHECK(!hm_widescreen_margin());
    hm_widescreen_scene(1);
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV d;
    SetDefDrawEnv(&d, 0, 0, 320, 240);
    d.dtd = 0;
    d.dfe = 1;
    PutDrawEnv(&d);
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, 320, 240);
    PutDispEnv(&display);
    hm_software_output *out = hm_software_output_create();
    CHECK(out);
    hm_render_frame frame = {.key = 7, .tick = 0, .eligible = 1, .world_view = 1};
    u32 world[] = {0x600000ff, xy(-40, 10), xy(20, 20)};
    u32 ui[] = {0x6000ff00, xy(-40, 50), xy(20, 20)};
    hm_render_begin(&frame);
    hm_render_group(1);
    hm_render_packet(world, 3);
    hm_render_group(0);
    hm_render_packet(ui, 3);
    hm_render_end();
    hm_vram_snapshot(&before);
    u64 ticks = hm_timing_ticks();
    CHECK(hm_software_output_prepare(out, 428, 240));
    hm_replay_sink previous = hm_software_output_begin(out);
    CHECK(hm_software_output_replay(out, &frame));
    hm_software_view view = {0};
    CHECK(hm_software_output_end(out, previous, &view));
    CHECK(view.pixels[15 * 428 + 20] == 31); /* Extra world visible. */
    CHECK(view.pixels[55 * 428 + 20] == 0);  /* UI remains clipped to native canvas. */
    CHECK(view.pixels[15 * 428 + 0] == 0);   /* Side clear never reads wrapped atlas. */
    hm_vram_snapshot(&after);
    CHECK(!memcmp(&before, &after, sizeof before));
    CHECK(!memcmp(&d, &hm_gpu_environment_state()->draw, sizeof d) && ticks == hm_timing_ticks());
    hm_gpu_scene *gpu = hm_gpu_scene_create();
    CHECK(gpu);
    previous = hm_gpu_scene_begin(gpu, 428, 240);
    CHECK(hm_software_output_replay(out, &frame) == 1);
    /* Cached software replay intentionally does not emit commands: replay directly. */
    hm_replay_begin(&native);
    hm_replay_group(1);
    hm_gpu_execute_words(world, 3);
    hm_replay_group(0);
    hm_gpu_execute_words(ui, 3);
    hm_gpu_scene_data data;
    CHECK(hm_gpu_scene_end(gpu, previous, &data));
    CHECK((s32)data.words[2] == -54 && data.words[4] == 428 && data.words[10] == 1);
    previous = hm_software_output_begin(out);
    hm_replay_begin(&native);
    hm_replay_group(0);
    u32 full[] = {0x28ff0000, 0, 320, 240u << 16, 320u | (240u << 16)};
    hm_gpu_execute_words(full, 5);
    CHECK(hm_software_output_end(out, previous, &view));
    CHECK(view.pixels[15 * 428 + 20] == (31 << 10) && view.pixels[15 * 428 + 400] == (31 << 10));
    hm_gpu_scene_destroy(gpu);
    hm_software_output_destroy(out);
    hm_widescreen_configure(0, 1920, 1080);
    CHECK(hm_widescreen_area(native).w == 320);
    puts("Widescreen world/UI, source bounds, OFF parity and native-state preservation passed");
    return 0;
}
