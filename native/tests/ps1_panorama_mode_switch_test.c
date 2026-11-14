/* The game's stepped fade temporarily presents 640x480, then returns to
 * 320x240. A transient display size must not dispose its Hor+ sidecars. */
#include "ps1_video.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static Ps1Video video;
static int sample(int page, int x, int y, uint32_t *out) {
    int w = ps1_video_panorama_width(320);
    if (w <= 320) return 1;
    uint32_t *pixels = calloc((size_t)w * 240, sizeof(*pixels));
    if (!pixels) return 2;
    int ok = ps1_video_copy_panorama(&video, 0, page, 320, 240, pixels, (size_t)w);
    *out = ok ? pixels[(size_t)y * w + (size_t)((w - 320) / 2 + x)] : 0;
    free(pixels);
    return ok ? 0 : 3;
}

int main(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    ps1_video_panorama_begin_frame();
    ps1_video_set_2d_widescreen_expansion(true);
    for (int page = 0; page <= 240; page += 240) {
        Ps1DrawState state;
        ps1_draw_state_reset(&state);
        state.left = 0; state.right = 319;
        state.top = page; state.bottom = page + 239;
        state.offset_y = page;
        if (!ps1_video_draw_tile(&video, &state, 332, 50, 9, 12,
                                 page ? 0x00FF00u : 0xFF0000u, false)) return 2;
    }
    ps1_video_set_2d_widescreen_expansion(false);
    uint32_t before_a = 0, before_b = 0, after_a = 0, after_b = 0;
    if (sample(0, 334, 55, &before_a) || sample(240, 334, 55, &before_b)) return 3;
    if (before_a != 0xFFFF0000u || before_b != 0xFF00FF00u) return 4;
    /* A 640x480 fade is an authentic PS1 display mode, not user disabling
     * widescreen. The 320x240 panorama must retain BOTH double buffers. */
    if (!ps1_video_configure_panorama(640, 480, 1.0f)) return 5;
    if (ps1_video_panorama_width(640) != 640 ||
        ps1_video_panorama_width(320) <= 320) return 6;
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 7;
    if (sample(0, 334, 55, &after_a) || sample(240, 334, 55, &after_b)) return 8;
    if (after_a != before_a || after_b != before_b) return 9;
    if (!ps1_video_configure_panorama(0, 0, 1.0f) ||
        ps1_video_panorama_width(320) != 320) return 10;
    puts("ps1_panorama_mode_switch_test: 640x480 fade preserves both 320x240 pages");
    return 0;
}
