#include "ps1_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

static Ps1Video video;

static void test_atmospheric_fog(void) {
    memset(&video, 0, sizeof(video));
    ps1_video_set_true_color(true);
    ps1_video_set_pgxp_vertex_precision(true);
    ps1_video_set_smooth_fog(true);
    ps1_video_set_fog_params(100.0f, 500.0f, 0xFF808080u);

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;

    /* Triangle 1: Near camera (Z = 50.0f < near_z 100.0f) -> Pure Red (0xFF0000) */
    Ps1Vertex v0 = {.x = 10, .y = 10, .z = 50.0f, .w = 1.0f / 50.0f, .sub_x = 10.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v1 = {.x = 30, .y = 10, .z = 50.0f, .w = 1.0f / 50.0f, .sub_x = 30.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v2 = {.x = 10, .y = 30, .z = 50.0f, .w = 1.0f / 50.0f, .sub_x = 10.0f, .sub_y = 30.0f, .has_subpixel = true};
    bool ok = ps1_video_draw_triangle(&video, &state, &v0, &v1, &v2, 0xFF0000u, 0, 0, false, false, false);
    assert(ok);
    uint32_t c_near = video.color32[12][12];
    assert(((c_near >> 16) & 255u) == 255);
    assert(((c_near >> 8) & 255u) == 0);
    assert((c_near & 255u) == 0);

    /* Triangle 2: In the distance (Z = 500.0f >= far_z 500.0f) -> Fully submerged in fog (0xFF808080) */
    memset(&video, 0, sizeof(video));
    v0.z = 500.0f; v0.w = 1.0f / 500.0f;
    v1.z = 500.0f; v1.w = 1.0f / 500.0f;
    v2.z = 500.0f; v2.w = 1.0f / 500.0f;
    ok = ps1_video_draw_triangle(&video, &state, &v0, &v1, &v2, 0xFF0000u, 0, 0, false, false, false);
    assert(ok);
    uint32_t c_far = video.color32[12][12];
    assert(((c_far >> 16) & 255u) == 128);
    assert(((c_far >> 8) & 255u) == 128);
    assert((c_far & 255u) == 128);

    /* Triangle 3: Intermediate (Z = 300.0f, midpoint) -> 50% blend between Red (255, 0, 0) and Gray (128, 128, 128) */
    memset(&video, 0, sizeof(video));
    v0.z = 300.0f; v0.w = 1.0f / 300.0f;
    v1.z = 300.0f; v1.w = 1.0f / 300.0f;
    v2.z = 300.0f; v2.w = 1.0f / 300.0f;
    ok = ps1_video_draw_triangle(&video, &state, &v0, &v1, &v2, 0xFF0000u, 0, 0, false, false, false);
    assert(ok);
    uint32_t c_mid = video.color32[12][12];
    uint32_t r = (c_mid >> 16) & 255u;
    uint32_t g = (c_mid >> 8) & 255u;
    uint32_t b = c_mid & 255u;
    /* Expected: R = round(0.5 * 255 + 0.5 * 128) = 192. G = 64. B = 64. */
    assert(r >= 190 && r <= 194);
    assert(g >= 62 && g <= 66);
    assert(b >= 62 && b <= 66);
}

static void test_anti_aliasing(void) {
    memset(&video, 0, sizeof(video));
    ps1_video_set_true_color(true);

    /* Set up a high-contrast vertical edge in color32: left 4 columns black, right 4 columns white */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 4; ++c) {
            video.color32[r][c] = 0xFF000000u;
        }
        for (int c = 4; c < 8; ++c) {
            video.color32[r][c] = 0xFFFFFFFFu;
        }
    }

    uint32_t output_aa[8][8];
    uint32_t output_no_aa[8][8];

    /* Copy with Anti-Aliasing ON */
    ps1_video_set_anti_aliasing(PS1_AA_SMAA);
    bool ok = ps1_video_copy_argb(&video, 0, 0, 8, 8, &output_aa[0][0], 8);
    assert(ok);

    /* Copy with Anti-Aliasing OFF */
    ps1_video_set_anti_aliasing(PS1_AA_OFF);
    ok = ps1_video_copy_argb(&video, 0, 0, 8, 8, &output_no_aa[0][0], 8);
    assert(ok);

    /* When AA is OFF, boundary pixel at column 3 is pure black (0x000000) and column 4 is pure white (0xFFFFFF) */
    assert((output_no_aa[4][3] & 0xFFFFFFu) == 0x000000u);
    assert((output_no_aa[4][4] & 0xFFFFFFu) == 0xFFFFFFu);

    /* When AA is ON, boundary pixel at column 3 or 4 receives smoothed edge blending */
    uint32_t luma_boundary = output_aa[4][3] & 0xFFu;
    assert(luma_boundary > 0); /* Blended, not harsh 0 */
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    test_atmospheric_fog();
    test_anti_aliasing();
    printf("All ps1_aa_fog tests passed!\n");
    return 0;
}
