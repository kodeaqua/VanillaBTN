#include "ps1_video.h"
#include "ps1_pgxp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

static Ps1Video video;
static uint32_t output[1280 * 3 * 240];

static Ps1Vertex projected(float x, float y) {
    Ps1Vertex v = {0};
    v.x = (int)x; v.y = (int)y;
    v.sub_x = x; v.sub_y = y;
    v.z = 100.0f; v.w = 0.01f;
    v.has_subpixel = true; v.is_projected = true;
    return v;
}

static Ps1DrawState state_for_page(int page_y) {
    Ps1DrawState s;
    ps1_draw_state_reset(&s);
    s.left = 0; s.right = 319;
    s.top = page_y; s.bottom = page_y + 239;
    return s;
}

static void test_actual_subpixel_shading_and_hud_compositing(void) {
    ps1_video_set_render_scale(3);
    CHECK(ps1_video_configure_panorama(320, 240, 1.0f));
    CHECK(ps1_video_get_render_scale() == 3);
    ps1_pgxp_set_enabled(true, true);
    ps1_video_set_anti_aliasing(PS1_AA_OFF);
    ps1_video_set_true_color(true);
    ps1_video_set_true_depth_buffer(false);
    ps1_video_set_near_plane_clipping(false);
    Ps1DrawState state = state_for_page(0);
    ps1_video_quick_fill(&video, 0, 0, 320, 240, 0x000000u);
    Ps1Vertex v0 = projected(20.1f, 20.1f);
    Ps1Vertex v1 = projected(70.8f, 20.2f);
    Ps1Vertex v2 = projected(20.3f, 70.7f);
    const uint32_t colors[3] = {0xFF0000u, 0x00FF00u, 0x0000FFu};
    CHECK(ps1_video_draw_gouraud_triangle(&video, &state, &v0, &v1, &v2, colors, false));
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 960));
    /* A nearest-neighbor upscale would repeat an identical RGB value for
     * every output sample inside this one original PS1 pixel. */
    uint32_t c0 = output[30 * 3 * 960 + 30 * 3];
    uint32_t c1 = output[30 * 3 * 960 + 30 * 3 + 2];
    CHECK(c0 != c1);
    uint16_t ps1_word = video.vram[30][30];
    CHECK(ps1_word != 0);
    /* A read-only screenshot must not discard already rasterized subpixels. */
    CHECK(ps1_video_copy_argb(&video, 0, 0, 320, 240, output, 320));
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 960));
    CHECK(output[30 * 3 * 960 + 30 * 3] == c0);
    CHECK(output[30 * 3 * 960 + 30 * 3 + 2] == c1);

    /* Semitransparent HUD shades preserve supersample differences. */
    CHECK(ps1_video_draw_tile(&video, &state, 30, 30, 1, 1, 0x204060u, true));
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 960));
    CHECK(output[30 * 3 * 960 + 30 * 3] != output[30 * 3 * 960 + 30 * 3 + 2]);

    /* The real HUD is submitted after the world, covering every supersample
     * in its PS1 rectangle, without moving world geometry or texture VRAM. */
    CHECK(ps1_video_draw_tile(&video, &state, 30, 30, 1, 1, 0xFFFFFFu, false));
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 960));
    for (int y = 30 * 3; y < 31 * 3; ++y)
        for (int x = 30 * 3; x < 31 * 3; ++x)
            CHECK(output[y * 960 + x] == 0xFFFFFFFFu);
    CHECK(video.vram[30][30] != ps1_word);
}

static void test_panorama_margins_and_second_page(void) {
    ps1_video_set_render_scale(2);
    CHECK(ps1_video_configure_panorama(320, 240, 4.0f / 3.0f));
    CHECK(ps1_video_get_render_scale() == 2);
    CHECK(ps1_video_panorama_width(320) == 427);
    Ps1DrawState state = state_for_page(240);
    ps1_video_quick_fill(&video, 0, 240, 320, 240, 0);
    Ps1Vertex a = projected(-30.0f, 250.0f);
    Ps1Vertex b = projected(30.0f, 250.0f);
    Ps1Vertex c = projected(-30.0f, 290.0f);
    CHECK(ps1_video_draw_triangle(&video, &state, &a, &b, &c,
        0x00FFFFu, 0, 0, false, false, false));
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 240, 320, 240, output, 854));
    /* x=-10 is visible within the sidecar; the original VRAM left edge is
     * untouched by pixels with negative logical coordinates. */
    CHECK(output[20 * 854 + (53 - 10) * 2] == 0xFF00FFFFu);
    CHECK(video.vram[260][0] != 0x7FFFu);
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 854));
    CHECK(output[20 * 854 + (53 - 10) * 2] != 0xFF00FFFFu);
}

static void test_white_margin_after_clearing(void) {
    ps1_video_set_render_scale(2);
    CHECK(ps1_video_configure_panorama(320, 240, 4.0f / 3.0f));
    ps1_video_panorama_clear_rows(0, 240);
    ps1_video_quick_fill(&video, 0, 0, 320, 240, 0xFFFFFFu);
    CHECK(ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 854));
    CHECK(output[0] == 0xFFFFFFFFu);
}

static void test_bounded_scale_and_original_mode(void) {
    ps1_video_set_render_scale(16);
    CHECK(ps1_video_configure_panorama(320, 240, 4.0f));
    CHECK(ps1_video_get_render_scale() >= 2);
    CHECK(ps1_video_get_render_scale() < 16);
    ps1_video_set_render_scale(1);
    CHECK(ps1_video_configure_panorama(320, 240, 1.0f));
    CHECK(ps1_video_get_render_scale() == 1);
    CHECK(!ps1_video_copy_scaled_panorama(&video, 0, 0, 320, 240, output, 960));
}

int main(void) {
    test_actual_subpixel_shading_and_hud_compositing();
    test_panorama_margins_and_second_page();
    test_white_margin_after_clearing();
    test_bounded_scale_and_original_mode();
    puts("ps1_hires_raster_test: passed");
    return 0;
}
