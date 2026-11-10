/*
 * ps1_pgxp_test: synthetic checks for PGXP subpixel vertex precision and
 * perspective-correct texture mapping.
 */

#include "ps1_gte.h"
#include "ps1_gpu.h"
#include "ps1_video.h"
#include "ps1_pgxp.h"
#include <libgpu.h>
#include <libgte.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

static Ps1Video video;
static Ps1DrawState draw;

static uint32_t pack_coords(int16_t x, int16_t y) {
    return (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
}

/* Test 1: GTE subpixel calculation and FIFO retention */
static int test_gte_subpixel(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);

    /* Point at (30, 20, 1000) */
    Ps1Vector3 v = {30, 20, 1000, 0};
    if (!ps1_gte_rtps(&gte, &v)) return 1;

    Ps1SubpixelCoord sub;
    ps1_gte_get_sub_sxy(&gte, 2, &sub);
    if (!sub.valid) return 2;

    /* Screen coords: ofx + ir_x * (h / sz) = 160 + 30 * 1 = 190 */
    if (fabsf(sub.x - 190.0f) > 0.05f) return 3;
    if (fabsf(sub.y - 140.0f) > 0.05f) return 4;
    if (sub.z < 999.0f || sub.z > 1001.0f) return 5;
    if (sub.w <= 0.0f) return 6;

    /* Push RTPT on 3 points */
    Ps1Vector3 v0 = { 0,  0, 1000, 0};
    Ps1Vector3 v1 = {50,  0, 1000, 0};
    Ps1Vector3 v2 = { 0, 50, 1000, 0};
    if (!ps1_gte_rtpt(&gte, &v0, &v1, &v2)) return 7;

    for (int i = 0; i < 3; ++i) {
        ps1_gte_get_sub_sxy(&gte, i, &sub);
        if (!sub.valid) return 8;
        if (sub.w <= 0.0f) return 9;
    }
    return 0;
}

/* Test 2: PGXP Cache registration and CPU overwrite detection */
static int test_pgxp_cache(void) {
    ps1_pgxp_init();
    ps1_pgxp_set_enabled(true, true);

    POLY_FT3 poly;
    memset(&poly, 0, sizeof(poly));
    poly.x0 = 10; poly.y0 = 20;
    poly.x1 = 30; poly.y1 = 20;
    poly.x2 = 20; poly.y2 = 40;

    uint32_t raw0 = pack_coords(10, 20);
    ps1_pgxp_register(&poly.x0, 10.35f, 20.75f, 500.0f, 1.0f / 500.0f, raw0);

    /* Lookup should succeed and return registered subpixel values */
    const Ps1PgxpVertex *lookup = ps1_pgxp_lookup(&poly.x0, raw0);
    if (lookup == NULL) return 10;
    if (fabsf(lookup->x - 10.35f) > 1e-4f) return 11;
    if (fabsf(lookup->y - 20.75f) > 1e-4f) return 12;

    /* CPU modifies coordinates: raw value no longer matches */
    poly.x0 = 15;
    uint32_t modified_raw = pack_coords(15, 20);
    if (ps1_pgxp_lookup(&poly.x0, modified_raw) != NULL) return 13;

    return 0;
}

/* Test 3: Subpixel precision rasterization edge shifting (anti-wobble) */
static int test_subpixel_rasterization(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);

    /* Base integer triangle */
    Ps1Vertex v0 = {10, 10, 0, 0, 10.0f, 10.0f, 100.0f, 0.01f, true};
    Ps1Vertex v1 = {20, 10, 0, 0, 20.0f, 10.0f, 100.0f, 0.01f, true};
    Ps1Vertex v2 = {10, 20, 0, 0, 10.0f, 20.0f, 100.0f, 0.01f, true};

    ps1_video_set_pgxp_vertex_precision(true);
    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0xFFFFFF, 0, 0, false, false, false)) return 20;

    int lit_base = 0;
    for (int y = 9; y <= 21; ++y) {
        for (int x = 9; x <= 21; ++x) {
            if (video.vram[y][x] != 0) lit_base++;
        }
    }
    if (lit_base == 0) return 21;

    /* Draw shifted triangle with +0.4f subpixel translation.
     * In integer PS1, 0.4 truncates to 0, so lit pixels would be identical.
     * In PGXP subpixel rasterization, the subpixel center evaluation causes
     * pixels on right/bottom edges to become lit while top/left turn off. */
    memset(&video, 0, sizeof(video));
    Ps1Vertex v0_s = {10, 10, 0, 0, 10.4f, 10.4f, 100.0f, 0.01f, true};
    Ps1Vertex v1_s = {20, 10, 0, 0, 20.4f, 10.4f, 100.0f, 0.01f, true};
    Ps1Vertex v2_s = {10, 20, 0, 0, 10.4f, 20.4f, 100.0f, 0.01f, true};

    if (!ps1_video_draw_triangle(&video, &draw, &v0_s, &v1_s, &v2_s,
                                 0xFFFFFF, 0, 0, false, false, false)) return 22;

    /* Verify that subpixel rasterization is active and differs from unshifted */
    bool difference_found = false;
    for (int y = 9; y <= 21; ++y) {
        for (int x = 9; x <= 21; ++x) {
            bool in_unshifted = (x >= 10 && y >= 10 && (x - 10) + (y - 10) <= 10);
            bool is_lit = (video.vram[y][x] != 0);
            if (is_lit != in_unshifted) {
                difference_found = true;
                break;
            }
        }
        if (difference_found) break;
    }
    if (!difference_found) return 23;

    return 0;
}

/* Test 4: Perspective-correct texturing vs affine texturing (anti-warp) */
static int test_perspective_texturing(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);

    /* Populate a horizontal gradient texture line in VRAM at tpage (0, 256) */
    for (int x = 0; x < 256; ++x) {
        /* Store non-zero 15-bit color proportional to U */
        video.vram[256][x] = (uint16_t)((x >> 3) & 0x1F) | 0x8000;
    }

    /* Oblique triangle: Vertex 0 near (Z=100, W=0.01), Vertices 1 & 2 far (Z=500, W=0.002) */
    Ps1Vertex v0 = {10,  10,   0, 0, 10.0f,  10.0f, 100.0f, 0.010f, true};
    Ps1Vertex v1 = {50,  10, 200, 0, 50.0f,  10.0f, 500.0f, 0.002f, true};
    Ps1Vertex v2 = {50,  50, 200, 0, 50.0f,  50.0f, 500.0f, 0.002f, true};

    /* First, render with perspective correction enabled. TPage 0x0110 = 15-bit direct color at Y=256 */
    ps1_video_set_pgxp_perspective_texture(true);
    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0x808080, 0x0110, 0, true, true, false)) return 30;

    uint16_t color_persp = video.vram[20][30];
    if (color_persp == 0) return 31;

    /* Next, render with perspective correction disabled (authentic affine) */
    memset(&video, 0, sizeof(video));
    for (int x = 0; x < 256; ++x) video.vram[256][x] = (uint16_t)((x >> 3) & 0x1F) | 0x8000;

    ps1_video_set_pgxp_perspective_texture(false);
    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0x808080, 0x0110, 0, true, true, false)) return 32;

    uint16_t color_affine = video.vram[20][30];
    if (color_affine == 0) return 33;

    /* The sampled color at midpoint (30, 20) MUST differ significantly between
     * affine texturing (linear U interpolation) and perspective texturing (U/W division). */
    if (color_persp == color_affine) return 34;

    return 0;
}

/* Test 5: Master preset toggle verification */
static int test_toggles(void) {
    ps1_pgxp_set_enabled(false, false);
    if (ps1_pgxp_is_vertex_precision_enabled()) return 40;
    if (ps1_pgxp_is_perspective_texture_enabled()) return 41;

    ps1_pgxp_set_enabled(true, true);
    if (!ps1_pgxp_is_vertex_precision_enabled()) return 42;
    if (!ps1_pgxp_is_perspective_texture_enabled()) return 43;

    return 0;
}

int main(void) {
    int r;
    if ((r = test_gte_subpixel()) != 0) {
        fprintf(stderr, "test_gte_subpixel failed: %d\n", r);
        return r;
    }
    if ((r = test_pgxp_cache()) != 0) {
        fprintf(stderr, "test_pgxp_cache failed: %d\n", r);
        return r;
    }
    if ((r = test_subpixel_rasterization()) != 0) {
        fprintf(stderr, "test_subpixel_rasterization failed: %d\n", r);
        return r;
    }
    if ((r = test_perspective_texturing()) != 0) {
        fprintf(stderr, "test_perspective_texturing failed: %d\n", r);
        return r;
    }
    if ((r = test_toggles()) != 0) {
        fprintf(stderr, "test_toggles failed: %d\n", r);
        return r;
    }
    printf("All PGXP tests passed successfully.\n");
    return 0;
}
