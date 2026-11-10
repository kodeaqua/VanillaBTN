#include "ps1_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

static Ps1Video video;

static void test_true_color_preservation(void) {
    memset(&video, 0, sizeof(video));
    ps1_video_set_true_color(true);
    ps1_video_set_dithering(PS1_DITHER_OFF);

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;

    /* Draw a flat tile with an RGB color that cannot be precisely represented in 15-bit:
     * 0x12, 0x34, 0x56.
     * In 15-bit:
     * R: 0x12 >> 3 = 2 -> expanded to 8-bit is (2 << 3 | 2 >> 2) = 16 (0x10).
     * G: 0x34 >> 3 = 6 -> expanded to 8-bit is (6 << 3 | 6 >> 2) = 48 + 1 = 49 (0x31).
     * B: 0x56 >> 3 = 10 -> expanded to 8-bit is (10 << 3 | 10 >> 2) = 80 + 2 = 82 (0x52).
     */
    uint32_t subtle_rgb = 0x123456u;
    bool ok = ps1_video_draw_tile(&video, &state, 10, 10, 4, 4, subtle_rgb, false);
    assert(ok);

    /* Verify true 32-bit color buffer holds exact ARGB 0xFF123456 */
    assert(video.color32[10][10] == 0xFF123456u);

    /* Read back with ps1_video_copy_argb with true color enabled */
    uint32_t output_pixel = 0;
    ok = ps1_video_copy_argb(&video, 10, 10, 1, 1, &output_pixel, 1);
    assert(ok);
    assert(output_pixel == 0xFF123456u);

    /* Now switch to authentic PS1 15-bit mode */
    ps1_video_set_true_color(false);
    ok = ps1_video_copy_argb(&video, 10, 10, 1, 1, &output_pixel, 1);
    assert(ok);
    /* In authentic mode, it must be the 15-bit quantized and expanded color, NOT 0xFF123456 */
    assert(output_pixel != 0xFF123456u);
    uint32_t r = (output_pixel >> 16) & 0xFFu;
    uint32_t g = (output_pixel >> 8) & 0xFFu;
    uint32_t b = output_pixel & 0xFFu;
    assert(r == 16);
    assert(g == 49);
    assert(b == 82);

    /* Restore true color */
    ps1_video_set_true_color(true);
}

static void test_gouraud_gradient_true_color(void) {
    memset(&video, 0, sizeof(video));
    ps1_video_set_true_color(true);
    ps1_video_set_dithering(PS1_DITHER_OFF);

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;

    Ps1Vertex v0 = {.x = 10, .y = 10, .z = 100.0f, .w = 0.01f, .sub_x = 10.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v1 = {.x = 30, .y = 10, .z = 100.0f, .w = 0.01f, .sub_x = 30.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v2 = {.x = 10, .y = 30, .z = 100.0f, .w = 0.01f, .sub_x = 10.0f, .sub_y = 30.0f, .has_subpixel = true};

    uint32_t colors[3] = {0x000000u, 0xFF0000u, 0x000000u};
    bool ok = ps1_video_draw_gouraud_triangle(&video, &state, &v0, &v1, &v2, colors, false);
    assert(ok);

    /* In true 32-bit color, gradient values take intermediate steps between 0 and 255.
     * Check that we have fine steps in color32. */
    int distinct_reds = 0;
    uint32_t last_r = 999;
    for (int x = 11; x < 25; ++x) {
        uint32_t c = video.color32[12][x];
        uint32_t r = (c >> 16) & 0xFFu;
        if (r != last_r) {
            distinct_reds++;
            last_r = r;
        }
    }
    /* We sampled 14 pixels across a span of 20 pixels. True color should have multiple distinct steps */
    assert(distinct_reds >= 8);
}

static void test_dithering_suppression_and_application(void) {
    memset(&video, 0, sizeof(video));

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;
    state.dither = true;

    Ps1Vertex v0 = {.x = 10, .y = 10, .z = 100.0f, .w = 0.01f, .sub_x = 10.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v1 = {.x = 30, .y = 10, .z = 100.0f, .w = 0.01f, .sub_x = 30.0f, .sub_y = 10.0f, .has_subpixel = true};
    Ps1Vertex v2 = {.x = 10, .y = 30, .z = 100.0f, .w = 0.01f, .sub_x = 10.0f, .sub_y = 30.0f, .has_subpixel = true};

    /* First test DITHER_OFF: even with state.dither = true, 15-bit VRAM pixels across a flat triangle
     * must NOT have dither pattern fluctuations. */
    ps1_video_set_dithering(PS1_DITHER_OFF);
    bool ok = ps1_video_draw_triangle(&video, &state, &v0, &v1, &v2, 0x434343u, 0, 0, false, false, false);
    assert(ok);

    uint16_t vram_val0 = video.vram[12][12];
    uint16_t vram_val1 = video.vram[12][13];
    uint16_t vram_val2 = video.vram[13][12];
    assert(vram_val0 == vram_val1);
    assert(vram_val0 == vram_val2);

    /* Second test DITHER_NATIVE: with state.dither = true, 15-bit VRAM should receive dither matrix offsets.
     * Color 0x43 (67): 67 - 4 = 63 (>> 3 = 7), 67 + 1 = 68 (>> 3 = 8), producing dither differences. */
    memset(&video, 0, sizeof(video));
    ps1_video_set_dithering(PS1_DITHER_NATIVE);
    ok = ps1_video_draw_triangle(&video, &state, &v0, &v1, &v2, 0x434343u, 0, 0, false, false, false);
    assert(ok);

    /* Check that at least some adjacent pixels in the 4x4 matrix differ due to dither matrix */
    bool found_difference = false;
    for (int r = 11; r <= 14; ++r) {
        for (int c = 11; c <= 14; ++c) {
            if (video.vram[r][c] != video.vram[11][11]) {
                found_difference = true;
                break;
            }
        }
        if (found_difference) break;
    }
    assert(found_difference);
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    test_true_color_preservation();
    test_gouraud_gradient_true_color();
    test_dithering_suppression_and_application();
    printf("All ps1_color32 tests passed!\n");
    return 0;
}
