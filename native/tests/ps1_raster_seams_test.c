#include "ps1_video.h"
#include "ps1_pgxp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static Ps1Video video;

static Ps1DrawState make_draw_state(void) {
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0;
    state.top = 0;
    state.right = 319;
    state.bottom = 239;
    return state;
}

static void reset_video(void) {
    memset(&video, 0, sizeof(video));
    ps1_video_clear_zbuffer(&video);
    ps1_pgxp_set_enabled(true, true);
    ps1_video_set_true_depth_buffer(true);
    ps1_video_set_near_plane_clipping(false);
    ps1_video_set_quad_subdivision(false);
    ps1_video_set_anti_aliasing(PS1_AA_OFF);
    ps1_video_set_texture_filter(PS1_FILTER_NEAREST);
    ps1_video_set_widescreen_scale(1.0f, 160.0f);
    ps1_video_set_2d_widescreen_expansion(false);
}

static Ps1Vertex vertex(float x, float y, bool projected) {
    Ps1Vertex result;
    memset(&result, 0, sizeof(result));
    result.x = (int)(x + 0.5f);
    result.y = (int)(y + 0.5f);
    result.sub_x = x;
    result.sub_y = y;
    result.z = 100.0f;
    result.w = 0.01f;
    result.has_subpixel = true;
    result.is_projected = projected;
    return result;
}

static void test_subpixel_shared_edge_has_no_black_crack(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();

    /* This shared diagonal used to produce a one-pixel black crack at (71,32).
     * The two float edge equations subtracted in float before being promoted to
     * double, so opposite directions disagreed by more than the edge epsilon. */
    Ps1Vertex v0 = vertex(12.1f, 27.1f, true);
    Ps1Vertex v1 = vertex(77.7f, 27.9f, true);
    Ps1Vertex v2 = vertex(12.6f, 76.2f, true);
    Ps1Vertex v3 = vertex(78.2f, 77.0f, true);

    CHECK(ps1_video_draw_quad(&video, &state, &v0, &v1, &v2, &v3,
                              0xFFFFFFu, 0, 0, false, false, false));
    CHECK(video.vram[32][71] != 0);
    CHECK(video.color32[32][71] != 0);
}

static void test_2d_tile_uses_centered_safe_area(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    CHECK(ps1_video_draw_tile(&video, &state, 100, 0, 20, 8, 0xFFFFFFu, false));
    CHECK(video.vram[0][114] == 0);
    CHECK(video.vram[0][115] != 0);
    CHECK(video.vram[0][129] != 0);
    CHECK(video.vram[0][130] == 0);
}

static void test_2d_clip_area_tracks_safe_area(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    state.right = 80;
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    CHECK(ps1_video_draw_tile(&video, &state, 20, 0, 20, 8, 0xFFFFFFu, false));
    CHECK(video.vram[0][55] != 0);
    CHECK(video.vram[0][69] != 0);
    CHECK(video.vram[0][70] == 0);
}

static void test_fullscreen_fade_quad_reaches_widescreen_edges(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    Ps1Vertex v0 = vertex(0.0f, 0.0f, false);
    Ps1Vertex v1 = vertex(320.0f, 0.0f, false);
    Ps1Vertex v2 = vertex(0.0f, 240.0f, false);
    Ps1Vertex v3 = vertex(320.0f, 240.0f, false);
    CHECK(ps1_video_draw_quad(&video, &state, &v0, &v1, &v2, &v3,
                              0xFFFFFFu, 0, 0, false, false, false));
    CHECK(video.vram[120][0] != 0);
    CHECK(video.vram[120][319] != 0);
}

static void test_tiled_background_expansion_mode(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    ps1_video_set_2d_widescreen_expansion(true);
    CHECK(ps1_video_get_2d_widescreen_expansion());
    CHECK(ps1_video_draw_tile(&video, &state, 0, 0, 24, 24, 0xFFFFFFu, false));
    CHECK(video.vram[10][0] != 0);
    CHECK(video.vram[10][23] != 0);
    CHECK(video.vram[10][24] == 0);

    ps1_video_set_2d_widescreen_expansion(false);
    CHECK(!ps1_video_get_2d_widescreen_expansion());
    memset(&video, 0, sizeof(video));
    CHECK(ps1_video_draw_tile(&video, &state, 100, 0, 24, 24, 0xFFFFFFu, false));
    CHECK(video.vram[10][115] != 0);
    CHECK(video.vram[10][132] != 0);
    CHECK(video.vram[10][133] == 0);
}

static void test_2d_polygon_is_compressed_but_gte_polygon_is_not(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    Ps1Vertex hud0 = vertex(0.0f, 0.0f, false);
    Ps1Vertex hud1 = vertex(80.0f, 0.0f, false);
    Ps1Vertex hud2 = vertex(0.0f, 80.0f, false);
    CHECK(ps1_video_draw_triangle(&video, &state, &hud0, &hud1, &hud2,
                                  0x00FF00u, 0, 0, false, false, false));
    CHECK(video.vram[10][50] != 0);
    CHECK(video.vram[10][20] == 0);

    reset_video();
    state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);
    Ps1Vertex world0 = vertex(10.0f, 0.0f, true);
    Ps1Vertex world1 = vertex(40.0f, 0.0f, true);
    Ps1Vertex world2 = vertex(10.0f, 80.0f, true);
    CHECK(ps1_video_draw_triangle(&video, &state, &world0, &world1, &world2,
                                  0x00FF00u, 0, 0, false, false, false));
    CHECK(video.vram[10][15] != 0);
    CHECK(video.vram[10][50] == 0);
}

static void test_quad_provenance_is_consistent_across_diagonal(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    Ps1Vertex v0 = vertex(10.0f, 10.0f, true);
    Ps1Vertex v1 = vertex(40.0f, 10.0f, false);
    Ps1Vertex v2 = vertex(10.0f, 40.0f, false);
    Ps1Vertex v3 = vertex(40.0f, 40.0f, false);
    CHECK(ps1_video_draw_quad(&video, &state, &v0, &v1, &v2, &v3,
                              0xFFFFFFu, 0, 0, false, false, false));
    CHECK(video.vram[25][35] != 0);
    CHECK(video.vram[25][50] == 0);
}

static void test_widescreen_sprite_samples_full_source_width(void) {
    reset_video();
    Ps1DrawState state = make_draw_state();
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);

    for (unsigned x = 0; x < 16; ++x) {
        video.vram[10][x] = (uint16_t)(0x001Fu + x * 0x0020u);
    }

    Ps1Sprite sprite = {0};
    sprite.x = 100;
    sprite.y = 0;
    sprite.width = 16;
    sprite.height = 1;
    sprite.u = 0;
    sprite.v = 10;
    sprite.tpage = 2u << 7; /* Direct 15-bit texture page. */
    sprite.rgb24 = 0x808080u;
    sprite.raw_texture = true;

    CHECK(ps1_video_draw_sprite(&video, &state, &sprite));
    CHECK(video.vram[0][115] == video.vram[10][0]);
    CHECK(video.vram[0][126] == video.vram[10][15]);
}

int main(void) {
    test_subpixel_shared_edge_has_no_black_crack();
    test_2d_tile_uses_centered_safe_area();
    test_2d_clip_area_tracks_safe_area();
    test_fullscreen_fade_quad_reaches_widescreen_edges();
    test_tiled_background_expansion_mode();
    test_2d_polygon_is_compressed_but_gte_polygon_is_not();
    test_quad_provenance_is_consistent_across_diagonal();
    test_widescreen_sprite_samples_full_source_width();
    ps1_video_set_widescreen_scale(1.0f, 160.0f);
    puts("ps1_raster_seams_test: shared edges, full-width fades, and widescreen 2D backgrounds passed.");
    return 0;
}
