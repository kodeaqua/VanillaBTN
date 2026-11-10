/* ps1_fillrate_oit_test.c — test suite for Zero Fill-Rate Bottleneck,
 * Order-Independent Transparency (OIT), HDR Soft-Additive Tonemapping,
 * and Atomic Double-Buffered Palette Updates.
 */

#include "ps1_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static Ps1Video video;

/* 1. Test HDR Soft-Additive Tonemapping (Flaw 21) */
static void test_hdr_tonemapping(void) {
    Ps1DrawState draw;
    ps1_draw_state_reset(&draw);
    draw.blend_mode = 1; /* Additive: Back + Front */

    /* Clear video */
    ps1_video_quick_fill(&video, 0, 0, 100, 100, 0x000000);

    /* Enable HDR tonemapping */
    ps1_video_set_hdr_tonemapping(true);
    TEST_ASSERT(ps1_video_get_hdr_tonemapping());

    /* Draw base tile with color (200, 50, 50) */
    ps1_video_draw_tile(&video, &draw, 10, 10, 20, 20, 0xC83232, false);
    uint32_t c_base = video.color32[15][15];
    uint32_t r_base = (c_base >> 16) & 255;
    TEST_ASSERT(r_base == 200);

    /* Draw additive semi-transparent tile with color (200, 50, 50) */
    ps1_video_draw_tile(&video, &draw, 10, 10, 20, 20, 0xC83232, true);
    uint32_t c_hdr = video.color32[15][15];
    uint32_t r_hdr = (c_hdr >> 16) & 255;

    /* In HDR mode: 200 + 200 - (200 * 200 + 127) / 255 = 400 - 157 = 243 (< 255, no clipping blowout!) */
    TEST_ASSERT(r_hdr > 200 && r_hdr < 255);

    /* Now test with authentic PS1 mode (HDR tonemapping disabled) */
    ps1_video_set_hdr_tonemapping(false);
    TEST_ASSERT(!ps1_video_get_hdr_tonemapping());

    ps1_video_quick_fill(&video, 0, 0, 100, 100, 0x000000);
    ps1_video_draw_tile(&video, &draw, 10, 10, 20, 20, 0xC83232, false);
    ps1_video_draw_tile(&video, &draw, 10, 10, 20, 20, 0xC83232, true);
    uint32_t c_auth = video.color32[15][15];
    uint32_t r_auth = (c_auth >> 16) & 255;

    /* In authentic mode: 200 + 200 hard clamps to 255 blowout */
    TEST_ASSERT(r_auth == 255);

    /* Restore default */
    ps1_video_set_hdr_tonemapping(true);
    printf("test_hdr_tonemapping passed.\n");
}

/* 2. Test Zero Fill-Rate Overdraw Limiter (Flaw 19) */
static void test_zero_fillrate_lag(void) {
    Ps1DrawState draw;
    ps1_draw_state_reset(&draw);
    draw.blend_mode = 0; /* Semi-transparent 50% */

    ps1_video_clear_zbuffer(&video);
    ps1_video_quick_fill(&video, 0, 0, 100, 100, 0x000000);

    ps1_video_set_zero_fillrate_lag(true);
    TEST_ASSERT(ps1_video_get_zero_fillrate_lag());

    /* Draw 50 overlapping semi-transparent lines on the exact same pixels */
    for (int i = 0; i < 50; i++) {
        ps1_video_draw_line(&video, &draw, 10, 10, 20, 10, 0xFFFFFF, true);
    }

    /* With overdraw limiter active, the overdraw counter should be capped at 32 */
    TEST_ASSERT(video.overdraw[10][15] == 32);

    /* Test tile overdraw */
    for (int i = 0; i < 50; i++) {
        ps1_video_draw_tile(&video, &draw, 30, 30, 10, 10, 0xFFFFFF, true);
    }
    TEST_ASSERT(video.overdraw[35][35] == 32);

    /* Disable overdraw limiter and verify */
    ps1_video_set_zero_fillrate_lag(false);
    TEST_ASSERT(!ps1_video_get_zero_fillrate_lag());

    ps1_video_set_zero_fillrate_lag(true);
    printf("test_zero_fillrate_lag passed.\n");
}

/* 3. Test Order-Independent Transparency (Flaw 18) */
static void test_oit_transparency(void) {
    Ps1DrawState draw;
    ps1_draw_state_reset(&draw);
    draw.blend_mode = 0;

    ps1_video_set_true_depth_buffer(true);
    ps1_video_set_oit_transparency(true);
    TEST_ASSERT(ps1_video_get_oit_transparency());

    /* Clear scene */
    ps1_video_clear_zbuffer(&video);
    ps1_video_quick_fill(&video, 0, 0, 200, 200, 0x000000);

    /* Setup two overlapping triangles:
     * T1 is near: Z = 100, W = 1/100 = 0.01
     * T2 is far:  Z = 400, W = 1/400 = 0.0025
     * Test case A: submit T1 (near) first, then T2 (far).
     * OIT under-blending must integrate T2 under T1 instead of overwriting T1! */

    Ps1Vertex v_near[3] = {
        { 10, 10, 0, 0, 10.0f, 10.0f, 100.0f, 1.0f / 100.0f, true },
        { 50, 10, 0, 0, 50.0f, 10.0f, 100.0f, 1.0f / 100.0f, true },
        { 10, 50, 0, 0, 10.0f, 50.0f, 100.0f, 1.0f / 100.0f, true }
    };
    Ps1Vertex v_far[3] = {
        { 10, 10, 0, 0, 10.0f, 10.0f, 400.0f, 1.0f / 400.0f, true },
        { 50, 10, 0, 0, 50.0f, 10.0f, 400.0f, 1.0f / 400.0f, true },
        { 10, 50, 0, 0, 10.0f, 50.0f, 400.0f, 1.0f / 400.0f, true }
    };

    /* Draw near first (red) */
    ps1_video_draw_triangle(&video, &draw, &v_near[0], &v_near[1], &v_near[2], 0xC00000, 0, 0, false, false, true);
    /* Then draw far (blue) */
    ps1_video_draw_triangle(&video, &draw, &v_far[0], &v_far[1], &v_far[2], 0x0000C0, 0, 0, false, false, true);

    uint32_t c_oit = video.color32[20][20];
    uint32_t r_oit = (c_oit >> 16) & 255;
    uint32_t b_oit = c_oit & 255;

    /* Near red layer was front; far blue was inserted under it.
     * Both colors should be present, with red (front) prominent over blue (behind). */
    TEST_ASSERT(r_oit > 0);
    TEST_ASSERT(b_oit > 0);
    TEST_ASSERT(r_oit > b_oit);

    /* Disable OIT and verify flag */
    ps1_video_set_oit_transparency(false);
    TEST_ASSERT(!ps1_video_get_oit_transparency());
    ps1_video_set_oit_transparency(true);

    printf("test_oit_transparency passed.\n");
}

/* 4. Test Atomic CLUT Double-Buffering (Flaw 22) */
static void test_atomic_clut(void) {
    ps1_video_set_atomic_clut(true);
    TEST_ASSERT(ps1_video_get_atomic_clut());

    /* Clear video */
    ps1_video_quick_fill(&video, 0, 0, 100, 100, 0x000000);

    /* Setup a 16-color CLUT at VRAM (0, 480) with initial color 0x7C00 (pure red) */
    uint16_t clut_data[16];
    for (int i = 0; i < 16; i++) clut_data[i] = 0x001F; /* Pure red in BGR555 */

    ps1_video_upload(&video, 0, 480, 16, 1, clut_data, 16);
    /* Commit initial CLUT state */
    ps1_video_commit_clut(&video);

    /* Setup 4-bit texture word at (0, 0) pointing to clut entry 1 */
    video.vram[0][0] = 0x0001;

    unsigned tpage = 0; /* 4-bit, base (0,0) */
    unsigned clut = (480 << 6) | 0; /* y=480, x=0 */
    uint16_t texel = 0;

    TEST_ASSERT(ps1_video_sample_texel(&video, tpage, clut, 0, 0, &texel));
    TEST_ASSERT(texel == 0x001F);

    /* Now simulate a mid-frame VRAM upload to the CLUT with new color 0x03E0 (pure green) */
    uint16_t new_clut[16];
    for (int i = 0; i < 16; i++) new_clut[i] = 0x03E0;
    ps1_video_upload(&video, 0, 480, 16, 1, new_clut, 16);

    /* Because atomic CLUT is enabled, the in-flight texel sampling still sees the committed palette (0x001F) */
    TEST_ASSERT(ps1_video_sample_texel(&video, tpage, clut, 0, 0, &texel));
    TEST_ASSERT(texel == 0x001F);

    /* After committing the atomic CLUT at frame boundary, texel sampling sees the updated palette (0x03E0) */
    ps1_video_commit_clut(&video);
    TEST_ASSERT(ps1_video_sample_texel(&video, tpage, clut, 0, 0, &texel));
    TEST_ASSERT(texel == 0x03E0);

    /* Disable atomic CLUT: uploads now immediately tear into sampling */
    ps1_video_set_atomic_clut(false);
    TEST_ASSERT(!ps1_video_get_atomic_clut());

    for (int i = 0; i < 16; i++) clut_data[i] = 0x7C00; /* Pure blue */
    ps1_video_upload(&video, 0, 480, 16, 1, clut_data, 16);
    TEST_ASSERT(ps1_video_sample_texel(&video, tpage, clut, 0, 0, &texel));
    TEST_ASSERT(texel == 0x7C00);

    ps1_video_set_atomic_clut(true);
    printf("test_atomic_clut passed.\n");
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    test_hdr_tonemapping();
    test_zero_fillrate_lag();
    test_oit_transparency();
    test_atomic_clut();

    printf("All PS1 Fill-Rate, OIT, HDR tonemapping, and Atomic CLUT tests PASSED successfully!\n");
    return 0;
}
