/* The PS1 game draws one VRAM page while displaying the other. Panoramic
 * pixels for the displayed page must survive clearing the next draw page. */
#include "ps1_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;

static Ps1Vertex v(int x, int y) {
    Ps1Vertex out = {0};
    out.x = x;
    out.y = y;
    out.sub_x = (float)x;
    out.sub_y = (float)y;
    out.z = 100.0f;
    out.w = 0.01f;
    out.has_subpixel = true;
    out.is_projected = true;
    return out;
}

static int draw_extension(int page_y, int right, unsigned color) {
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0;
    state.right = 319;
    state.top = page_y;
    state.bottom = page_y + 239;
    int x = right ? 325 : -28;
    Ps1Vertex a = v(x, page_y + 30);
    Ps1Vertex b = v(x + 20, page_y + 30);
    Ps1Vertex c = v(x, page_y + 50);
    return ps1_video_draw_triangle(&video, &state, &a, &b, &c,
                                   color, 0, 0, false, false, false);
}

static uint32_t sample_side(int page_y, int right) {
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 0;
    uint32_t color = 0;
    if (ps1_video_copy_panorama(&video, 0, page_y, 320, 240, frame, (size_t)width)) {
        int x = right ? (width - 320) / 2 + 330 : (width - 320) / 2 - 19;
        color = frame[(size_t)35 * width + x];
    }
    free(frame);
    return color;
}

int main(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    ps1_video_panorama_begin_frame();
    ps1_video_set_true_color(true);
    /* First render page A while page B remains on screen. */
    ps1_video_panorama_clear_rows(0, 240);
    if (!draw_extension(0, 1, 0xFF0000u)) return 2;
    if (sample_side(0, 1) != 0xFFFF0000u) return 3;
    if (sample_side(240, 1) != 0xFF000000u) return 4;
    /* A must survive clearing B and rendering a different world scene. */
    ps1_video_panorama_clear_rows(240, 240);
    if (sample_side(0, 1) != 0xFFFF0000u) return 5;
    if (!draw_extension(240, 0, 0x00FF00u)) return 6;
    if (sample_side(240, 0) != 0xFF00FF00u) return 7;
    if (sample_side(0, 1) != 0xFFFF0000u) return 8;
    /* Next frame reuses A, leaving B untouched for the display. */
    ps1_video_panorama_clear_rows(0, 240);
    if (sample_side(0, 1) != 0xFF000000u) return 9;
    if (sample_side(240, 0) != 0xFF00FF00u) return 10;
    if (!draw_extension(0, 1, 0x0000FFu)) return 11;
    if (sample_side(0, 1) != 0xFF0000FFu) return 12;
    if (sample_side(240, 0) != 0xFF00FF00u) return 13;
    /* A real GPU reset, unlike a frame begin, clears both pages. */
    ps1_video_panorama_begin_frame();
    if (sample_side(0, 1) != 0xFF000000u ||
        sample_side(240, 0) != 0xFF000000u) return 14;
    ps1_video_configure_panorama(0, 0, 1.0f);
    puts("ps1_panorama_double_buffer_test: alternating draw/display pages preserve Hor+ pixels");
    return 0;
}
