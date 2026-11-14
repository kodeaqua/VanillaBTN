/*
 * ps1_zbuffer_test: synthetic checks for true 32-bit floating-point Z-buffer
 * and shadow polygon depth bias.
 */

#include "ps1_video.h"
#include "ps1_pgxp.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static Ps1Video video;
static Ps1DrawState draw;

static int test_zbuffer_control_and_clearing(void) {
    ps1_video_set_true_depth_buffer(true);
    if (!ps1_video_get_true_depth_buffer()) return 1;

    ps1_video_set_true_depth_buffer(false);
    if (ps1_video_get_true_depth_buffer()) return 2;

    ps1_video_set_true_depth_buffer(true);

    /* Fill dummy values in zbuffer */
    video.zbuffer[10][20] = 0.5f;
    video.zbuffer[100][200] = 1.0f;

    ps1_video_clear_zbuffer(&video);
    if (video.zbuffer[10][20] != 0.0f) return 3;
    if (video.zbuffer[100][200] != 0.0f) return 4;

    /* Test that quick_fill clears the zbuffer rectangle to 0.0f */
    video.zbuffer[50][50] = 0.123f;
    video.zbuffer[60][60] = 0.456f;
    video.zbuffer[80][80] = 0.789f;

    ps1_video_quick_fill(&video, 40, 40, 30, 30, 0x000000);
    if (video.zbuffer[50][50] != 0.0f) return 5;
    if (video.zbuffer[60][60] != 0.0f) return 6;
    /* Outside the fill rect must remain untouched */
    if (fabsf(video.zbuffer[80][80] - 0.789f) > 1e-5f) return 7;

    return 0;
}

static int test_zbuffer_occlusion(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_true_depth_buffer(true);
    ps1_video_set_pgxp_vertex_precision(true);

    /* Far triangle (Z = 400, W = 1/400 = 0.0025, Blue) */
    Ps1Vertex far_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 400.0f, 1.0f / 400.0f, true};
    Ps1Vertex far_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 400.0f, 1.0f / 400.0f, true};
    Ps1Vertex far_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 400.0f, 1.0f / 400.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &far_v0, &far_v1, &far_v2,
                                 0x0000FF, 0, 0, false, false, false)) return 10;

    /* Pixel (15, 15) must be Blue, and zbuffer must equal 0.0025 */
    uint16_t blue_15 = (31 << 10); /* BGR555 blue */
    if (video.vram[15][15] != blue_15) return 11;
    if (fabsf(video.zbuffer[15][15] - (1.0f / 400.0f)) > 1e-6f) return 12;

    /* Near triangle drawn second (Z = 100, W = 1/100 = 0.01, Red) */
    Ps1Vertex near_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex near_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex near_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 100.0f, 1.0f / 100.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &near_v0, &near_v1, &near_v2,
                                 0xFF0000, 0, 0, false, false, false)) return 13;

    /* Near triangle must overwrite far triangle: Pixel (15, 15) must be Red */
    uint16_t red_15 = 31; /* BGR555 red */
    if (video.vram[15][15] != red_15) return 14;
    if (fabsf(video.zbuffer[15][15] - (1.0f / 100.0f)) > 1e-6f) return 15;

    /* Now draw a very far triangle THIRD (Z = 600, W = 1/600 = 0.00166, Green) */
    Ps1Vertex very_far_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 600.0f, 1.0f / 600.0f, true};
    Ps1Vertex very_far_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 600.0f, 1.0f / 600.0f, true};
    Ps1Vertex very_far_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 600.0f, 1.0f / 600.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &very_far_v0, &very_far_v1, &very_far_v2,
                                 0x00FF00, 0, 0, false, false, false)) return 16;

    /* Very far triangle MUST BE OCCLUDED: Pixel (15, 15) must still be Red! */
    if (video.vram[15][15] != red_15) return 17;
    if (fabsf(video.zbuffer[15][15] - (1.0f / 100.0f)) > 1e-6f) return 18;

    return 0;
}

static int test_zbuffer_disabled_painter_order(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_true_depth_buffer(false); /* Disable Z-buffer */

    /* Near triangle first (Red) */
    Ps1Vertex near_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex near_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex near_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 100.0f, 1.0f / 100.0f, true};
    ps1_video_draw_triangle(&video, &draw, &near_v0, &near_v1, &near_v2,
                            0xFF0000, 0, 0, false, false, false);

    /* Far triangle second (Green) */
    Ps1Vertex far_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 500.0f, 1.0f / 500.0f, true};
    Ps1Vertex far_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 500.0f, 1.0f / 500.0f, true};
    Ps1Vertex far_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 500.0f, 1.0f / 500.0f, true};
    ps1_video_draw_triangle(&video, &draw, &far_v0, &far_v1, &far_v2,
                            0x00FF00, 0, 0, false, false, false);

    /* In painter's algorithm without Z-buffer, second polygon always overwrites */
    uint16_t green_15 = (31 << 5); /* BGR555 green */
    if (video.vram[15][15] != green_15) return 20;

    /* Re-enable for subsequent tests */
    ps1_video_set_true_depth_buffer(true);
    return 0;
}

static int test_shadow_depth_bias(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_true_depth_buffer(true);
    ps1_video_set_pgxp_vertex_precision(true);

    /* 1. Ground polygon at Z = 200, W = 1/200 = 0.005, Green */
    Ps1Vertex ground_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 200.0f, 1.0f / 200.0f, true};
    Ps1Vertex ground_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 200.0f, 1.0f / 200.0f, true};
    Ps1Vertex ground_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 200.0f, 1.0f / 200.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &ground_v0, &ground_v1, &ground_v2,
                                 0x00FF00, 0, 0, false, false, false)) return 30;

    uint16_t initial_ground = video.vram[15][15];
    if (initial_ground != (31 << 5)) return 31;

    /* 2. Semi-transparent shadow polygon coplanar at exact same Z = 200 */
    /* Blend mode 0: 0.5 * Back + 0.5 * Front */
    draw.blend_mode = 0;
    Ps1Vertex shadow_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 200.0f, 1.0f / 200.0f, true};
    Ps1Vertex shadow_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 200.0f, 1.0f / 200.0f, true};
    Ps1Vertex shadow_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 200.0f, 1.0f / 200.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &shadow_v0, &shadow_v1, &shadow_v2,
                                 0x000000, 0, 0, false, false, true)) return 32;

    /* Shadow bias must allow shadow to blend onto ground */
    uint16_t shadowed_pixel = video.vram[15][15];
    if (shadowed_pixel == initial_ground) return 33; /* Should be darkened */
    /* 0.5 * 31 = 15 green */
    if (((shadowed_pixel >> 5) & 31) != 15) return 34;

    /* Semi-transparent polygon must NOT update the Z-buffer */
    if (fabsf(video.zbuffer[15][15] - (1.0f / 200.0f)) > 1e-6f) return 35;

    /* 3. Obstacle in front (Z = 100, W = 0.01, Red) must occlude both */
    Ps1Vertex front_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex front_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 100.0f, 1.0f / 100.0f, true};
    Ps1Vertex front_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 100.0f, 1.0f / 100.0f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &front_v0, &front_v1, &front_v2,
                                 0xFF0000, 0, 0, false, false, false)) return 36;

    if (video.vram[15][15] != 31) return 37; /* Overwritten by Red obstacle */

    return 0;
}

static int test_2d_hud_safety(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_true_depth_buffer(true);

    /* 3D polygon with high depth (Z = 50, W = 0.02, Red) */
    Ps1Vertex d3_v0 = {10, 10, 0, 0, 10.0f, 10.0f, 50.0f, 1.0f / 50.0f, true};
    Ps1Vertex d3_v1 = {30, 10, 0, 0, 30.0f, 10.0f, 50.0f, 1.0f / 50.0f, true};
    Ps1Vertex d3_v2 = {10, 30, 0, 0, 10.0f, 30.0f, 50.0f, 1.0f / 50.0f, true};
    ps1_video_draw_triangle(&video, &draw, &d3_v0, &d3_v1, &d3_v2,
                            0xFF0000, 0, 0, false, false, false);

    /* 2D HUD polygon without subpixel coords (has_subpixel = false, White) */
    Ps1Vertex hud_v0 = {10, 10, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false};
    Ps1Vertex hud_v1 = {30, 10, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false};
    Ps1Vertex hud_v2 = {10, 30, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false};

    if (!ps1_video_draw_triangle(&video, &draw, &hud_v0, &hud_v1, &hud_v2,
                                 0xFFFFFF, 0, 0, false, false, false)) return 40;

    /* 2D HUD must draw directly on top, unoccluded by 3D depth */
    uint16_t white_15 = 0x7FFF;
    if (video.vram[15][15] != white_15) return 41;

    return 0;
}

int main(void) {
    int res;
    if ((res = test_zbuffer_control_and_clearing()) != 0) {
        fprintf(stderr, "test_zbuffer_control_and_clearing failed with %d\n", res);
        return 1;
    }
    if ((res = test_zbuffer_occlusion()) != 0) {
        fprintf(stderr, "test_zbuffer_occlusion failed with %d\n", res);
        return 2;
    }
    if ((res = test_zbuffer_disabled_painter_order()) != 0) {
        fprintf(stderr, "test_zbuffer_disabled_painter_order failed with %d\n", res);
        return 3;
    }
    if ((res = test_shadow_depth_bias()) != 0) {
        fprintf(stderr, "test_shadow_depth_bias failed with %d\n", res);
        return 4;
    }
    if ((res = test_2d_hud_safety()) != 0) {
        fprintf(stderr, "test_2d_hud_safety failed with %d\n", res);
        return 5;
    }

    printf("ps1_zbuffer_test: all tests passed\n");
    return 0;
}
