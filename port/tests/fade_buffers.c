/* Original fade packets through both native framebuffers and private outputs.
 * No assets, window, game loop, replay input or save data. */
#include "enhance/widescreen/widescreen.h"
#include "gfx/frame_replay.h"
#include "gfx/gpu_scene.h"
#include "gfx/software_output.h"
#include "main.h"
#include "psx/gpu_environment.h"
#include "psx/vram.h"
#include "screen_fade.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static u32 table[2][2];
int main(void) {
    hm_widescreen_configure(1, 1920, 1080);
    hm_widescreen_scene(1);
    hm_software_output *software = hm_software_output_create();
    hm_gpu_scene *gpu = hm_gpu_scene_create();
    hm_frame_replay *capture = hm_frame_replay_acquire();
    CHECK(software && gpu && capture && hm_software_output_prepare(software, 428, 240));
    hm_vram_reset();
    ResetGraph(0);
    int step = 0;
    u64 tick = 0;
    for (int phase = 0; phase < 4; ++phase) {
        if (phase & 1)
            fade_start_out(10, 0, 2);
        else
            fade_start_in(10, 0, 2);
        for (int frame = 0; frame < 16; ++frame, ++step, tick += 2) {
            int slot = step & 1;
            frame_index = slot;
            current_frame = &frame_records[slot];
            active_ot = table[slot];
            ClearOTagR(active_ot, 2);
            DRAWENV draw;
            SetDefDrawEnv(&draw, 0, slot * 240, 320, 240);
            draw.isbg = draw.dfe = 1;
            draw.dtd = 0;
            draw.r0 = draw.b0 = phase < 2 ? 255 : 0;
            draw.g0 = phase < 2 ? 255 : 120;
            DISPENV display;
            SetDefDispEnv(&display, 0, (1 - slot) * 240, 320, 240);
            PutDispEnv(&display);
            PutDrawEnv(&draw);
            screen_fade_frame();
            hm_render_frame live = {.tick = tick, .key = (u64)phase};
            hm_render_begin(&live);
            hm_render_group(0);
            DrawOTag(active_ot + 1);
            hm_render_end();
            if ((phase & 1) && frame == 5) {
                tick += 120; /* Blocking asset wait during fade-out/TV exit. */
                live.tick = tick;
            }
            if (!step)
                continue;
            hm_replay_sink prior = hm_software_output_begin(software);
            CHECK(hm_software_output_replay(software, &live));
            hm_software_view view = {0};
            CHECK(hm_software_output_end(software, prior, &view));
            u16 expected = hm_vram_pixel(0, (u32)display.disp.y + 100);
            CHECK(view.pixels[100 * 428] == expected);
            CHECK(view.pixels[100 * 428 + 427] == expected);
            CHECK(view.pixels[100 * 428 + 100] == expected);
            prior = hm_gpu_scene_begin(gpu, 428, 240);
            CHECK(hm_frame_replay_draw(capture, &live));
            hm_gpu_scene_data data;
            CHECK(hm_gpu_scene_end(gpu, prior, &data));
            /* The old displayed capture can have a different clear colour from
             * the new drawing buffer at a splash/TV transition. */
            u16 background = phase < 2 || (phase == 2 && frame == 0) ? 0x7fff : 15u << 5;
            CHECK(data.words[14] == background);
        }
    }
    hm_frame_replay_release(capture);
    hm_software_output_destroy(software);
    hm_gpu_scene_destroy(gpu);
    return 0;
}
