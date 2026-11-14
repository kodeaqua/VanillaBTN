/* A new OT/frame does not clear the PS1 framebuffer. The panorama sidecar
 * must likewise preserve its underlay until an actual VRAM clear/draw occurs.
 * Title screen semi-transparent sprites depend on this retained backdrop. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static Ps1Video video;
static u_long ot[4];

static uint32_t pixel_at(int page, int x, int y) {
    const int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 0;
    uint32_t result = 0;
    if (ps1_video_copy_panorama(&video, 0, page, 320, 240, frame, (size_t)width))
        result = frame[(size_t)y * width + (size_t)((width - 320) / 2 + x)];
    free(frame);
    return result;
}

static uint32_t side_pixel(int page) { return pixel_at(page, -50, 100); }

static int use_page(int page) {
    DRAWENV draw;
    if (!SetDefDrawEnv(&draw, 0, page, 320, 240)) return 1;
    sdk_gpu_begin_frame();
    if (!PutDrawEnv(&draw)) return 2;
    ClearOTagR(ot, 4);
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok()) return 3;
    return 0;
}

int main(void) {
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(ot, sizeof(ot)) ||
        !ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    RECT a = {0, 0, 320, 240}, b = {0, 240, 320, 240};
    if (ClearImage(&a, 32, 92, 56) != 0 ||
        ClearImage(&b, 19, 45, 71) != 0) return 2;
    if (side_pixel(0) != 0xFF205C38u || side_pixel(240) != 0xFF132D47u) return 3;
    if (use_page(0) || side_pixel(0) != 0xFF205C38u ||
        side_pixel(240) != 0xFF132D47u) return 4;
    if (use_page(240) || side_pixel(0) != 0xFF205C38u ||
        side_pixel(240) != 0xFF132D47u) return 5;
    /* Title backdrop sprites often contain semi-transparent texels: they
     * must blend over the retained clear color equally in central VRAM and
     * the panoramic sidecar, not over black in the margin. */
    video.vram[0][192] = 0x8000u | 0x4210u;
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319; state.top = 0; state.bottom = 239;
    state.blend_mode = 0;
    Ps1Sprite spr = {0};
    spr.x = 12; spr.y = 100; spr.width = spr.height = 1;
    spr.tpage = 3u | (2u << 7);
    spr.rgb24 = 0x808080u;
    spr.raw_texture = true;
    spr.semi_transparent = true;
    ps1_video_set_2d_widescreen_expansion(true);
    bool drawn = ps1_video_draw_sprite_with_provenance(&video, &state, &spr, false);
    ps1_video_set_2d_widescreen_expansion(false);
    uint32_t center = pixel_at(0, 12, 100), margin = pixel_at(0, 332, 100);
    if (!drawn || margin != center || margin == 0xFF205C38u ||
        ((margin >> 16) & 255u) == ((margin >> 8) & 255u)) return 7;
    /* Only an explicit clear is allowed to replace the retained page. */
    if (ClearImage(&a, 0, 0, 0) != 0 ||
        side_pixel(0) != 0xFF000000u ||
        side_pixel(240) != 0xFF132D47u) return 6;
    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_panorama_persistence_test: OT startup retains backdrop; explicit clear updates only the page");
    return 0;
}
