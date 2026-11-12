/*
 * ps1_clipping_test: synthetic checks for near-plane frustum W-clipping
 * and quad tessellation / subdivision.
 */

#include "ps1_video.h"
#include "ps1_pgxp.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static Ps1Video video;
static Ps1DrawState draw;

static int test_clipping_controls(void) {
    ps1_video_set_near_plane_clipping(true);
    if (!ps1_video_get_near_plane_clipping()) return 1;

    ps1_video_set_near_plane_clipping(false);
    if (ps1_video_get_near_plane_clipping()) return 2;

    ps1_video_set_near_plane_clipping(true);

    ps1_video_set_quad_subdivision(true);
    if (!ps1_video_get_quad_subdivision()) return 3;

    ps1_video_set_quad_subdivision(false);
    if (ps1_video_get_quad_subdivision()) return 4;

    ps1_video_set_quad_subdivision(true);
    return 0;
}

static int test_near_plane_culling_all_behind(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_near_plane_clipping(true);

    /* Triangle completely behind near plane (Z < 8.0f) */
    Ps1Vertex v0 = {10, 10, 0, 0, 10.0f, 10.0f, -20.0f, -0.05f, true, true};
    Ps1Vertex v1 = {30, 10, 0, 0, 30.0f, 10.0f, -10.0f, -0.10f, true, true};
    Ps1Vertex v2 = {10, 30, 0, 0, 10.0f, 30.0f,   2.0f,  0.50f, true, true};

    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0xFF0000, 0, 0, false, false, false)) return 10;

    /* Entire triangle must be culled; zero pixels drawn into VRAM */
    for (int y = 0; y < 50; ++y) {
        for (int x = 0; x < 50; ++x) {
            if (video.vram[y][x] != 0) return 11;
        }
    }
    return 0;
}

static int test_near_plane_one_vertex_in_front(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_near_plane_clipping(true);

    /* v0 is in front (Z = 100), v1 and v2 are behind (Z = -20, Z = -40) */
    Ps1Vertex v0 = {160, 120, 0, 0, 160.0f, 120.0f, 100.0f, 0.01f, true, true};
    Ps1Vertex v1 = {260, 120, 0, 0, 260.0f, 120.0f, -20.0f, -0.05f, true, true};
    Ps1Vertex v2 = {160, 220, 0, 0, 160.0f, 220.0f, -40.0f, -0.025f, true, true};

    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0xFF0000, 0, 0, false, false, false)) return 20;

    /* Clipped triangle must draw pixels safely around v0 without crashing or corrupting memory */
    int lit_count = 0;
    for (int y = 100; y < 140; ++y) {
        for (int x = 140; x < 180; ++x) {
            if (video.vram[y][x] != 0) lit_count++;
        }
    }
    if (lit_count == 0) return 21;
    return 0;
}

static int test_near_plane_two_vertices_in_front(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_near_plane_clipping(true);

    /* v0 and v1 are in front (Z = 100, Z = 80), v2 is behind (Z = -20) */
    Ps1Vertex v0 = {150, 110, 0, 0, 150.0f, 110.0f, 100.0f, 0.01f, true, true};
    Ps1Vertex v1 = {180, 110, 0, 0, 180.0f, 110.0f,  80.0f, 0.0125f, true, true};
    Ps1Vertex v2 = {165, 200, 0, 0, 165.0f, 200.0f, -20.0f, -0.05f, true, true};

    if (!ps1_video_draw_triangle(&video, &draw, &v0, &v1, &v2,
                                 0x00FF00, 0, 0, false, false, false)) return 30;

    /* Clipped quad must produce valid green pixels in front */
    int lit_count = 0;
    for (int y = 105; y < 130; ++y) {
        for (int x = 150; x < 180; ++x) {
            if (video.vram[y][x] != 0) lit_count++;
        }
    }
    if (lit_count == 0) return 31;
    return 0;
}

static int test_quad_subdivision(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    ps1_video_set_quad_subdivision(true);

    /* Large non-planar quad (64x64) */
    Ps1Vertex v0 = {20, 20, 0, 0, 20.0f, 20.0f, 100.0f, 0.01f, true, true};
    Ps1Vertex v1 = {84, 20, 0, 0, 84.0f, 20.0f, 120.0f, 1.0f / 120.0f, true, true};
    Ps1Vertex v2 = {20, 84, 0, 0, 20.0f, 84.0f, 130.0f, 1.0f / 130.0f, true, true};
    Ps1Vertex v3 = {84, 84, 0, 0, 84.0f, 84.0f, 180.0f, 1.0f / 180.0f, true, true};

    if (!ps1_video_draw_quad(&video, &draw, &v0, &v1, &v2, &v3,
                             0x0000FF, 0, 0, false, false, false)) return 40;

    /* Center of quad (52, 52) must be lit blue */
    uint16_t blue_15 = (31 << 10);
    if (video.vram[52][52] != blue_15) return 41;

    /* Corners must also be covered */
    if (video.vram[25][25] != blue_15) return 42;
    if (video.vram[80][25] != blue_15) return 43;
    if (video.vram[25][80] != blue_15) return 44;
    if (video.vram[80][80] != blue_15) return 45;

    /* Gouraud quad subdivision */
    memset(&video, 0, sizeof(video));
    uint32_t colors[4] = {
        0xFF0000, /* Top-left Red */
        0x00FF00, /* Top-right Green */
        0x0000FF, /* Bottom-left Blue */
        0xFFFFFF  /* Bottom-right White */
    };
    if (!ps1_video_draw_gouraud_quad(&video, &draw, &v0, &v1, &v2, &v3, colors, false)) return 46;

    /* Verify center is lit with blended color (not black or uninitialized) */
    if (video.vram[52][52] == 0) return 47;

    return 0;
}

int main(void) {
    int res;
    if ((res = test_clipping_controls()) != 0) {
        fprintf(stderr, "test_clipping_controls failed with %d\n", res);
        return 1;
    }
    if ((res = test_near_plane_culling_all_behind()) != 0) {
        fprintf(stderr, "test_near_plane_culling_all_behind failed with %d\n", res);
        return 2;
    }
    if ((res = test_near_plane_one_vertex_in_front()) != 0) {
        fprintf(stderr, "test_near_plane_one_vertex_in_front failed with %d\n", res);
        return 3;
    }
    if ((res = test_near_plane_two_vertices_in_front()) != 0) {
        fprintf(stderr, "test_near_plane_two_vertices_in_front failed with %d\n", res);
        return 4;
    }
    if ((res = test_quad_subdivision()) != 0) {
        fprintf(stderr, "test_quad_subdivision failed with %d\n", res);
        return 5;
    }

    printf("ps1_clipping_test: all tests passed\n");
    return 0;
}
