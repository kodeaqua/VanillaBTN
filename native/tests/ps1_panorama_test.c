/* Dynamic Hor+ extras must not overwrite PS1 texture VRAM.
 * Check real projected geometry, non-mirrored 2D margins, and resizing. */
#include "ps1_video.h"
#include "ps1_pgxp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;

static Ps1Vertex projected(int x, int y) {
    Ps1Vertex v = {0};
    v.x = x; v.y = y;
    v.sub_x = (float)x; v.sub_y = (float)y;
    v.z = 100.0f; v.w = 0.01f;
    v.has_subpixel = true; v.is_projected = true;
    return v;
}

static int verify_ratio(float scale, int expected_width) {
    if (!ps1_video_configure_panorama(320, 240, scale)) return 1;
    if (ps1_video_panorama_width(320) != expected_width) return 2;

    ps1_video_panorama_begin_frame();
    Ps1DrawState draw;
    ps1_draw_state_reset(&draw);
    draw.left = 0; draw.top = 240; draw.right = 319; draw.bottom = 479;
    ps1_video_set_pgxp_vertex_precision(true);

    /* Write a 16-bit sentinel into the PS1 texture page next to framebuffer. */
    video.vram[275][340] = 0x7C00u;
    video.color32[275][340] = 0xFF0000FFu;
    /* Right-side world triangle, one corner far beyond the original clip. */
    Ps1Vertex r0 = projected(305, 270), r1 = projected(390, 270);
    Ps1Vertex r2 = projected(305, 340);
    if (!ps1_video_draw_triangle(&video, &draw, &r0, &r1, &r2,
                                 0x00FF00u, 0, 0, false, false, false)) return 3;
    /* Left-side world triangle, outside legacy coordinate x >= 0. */
    Ps1Vertex l0 = projected(-70, 270), l1 = projected(5, 270);
    Ps1Vertex l2 = projected(5, 345);
    if (!ps1_video_draw_triangle(&video, &draw, &l0, &l1, &l2,
                                 0xFF0000u, 0, 0, false, false, false)) return 4;
    if (video.vram[275][340] != 0x7C00u || video.color32[275][340] != 0xFF0000FFu) return 5;

    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 6;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 7;
    }
    int pad = (width - 320) / 2;
    if (pad < 6) {free(frame); return 8;}
    /* A side pixel covered by projected geometry must be colored, not black. */
    if (frame[35u * width + (unsigned)(pad + 325)] != 0xFF00FF00u) {
        free(frame); return 9;
    }
    if (frame[35u * width + (unsigned)(pad - 5)] != 0xFFFF0000u) {
        free(frame); return 10;
    }
    /* The first and last visible columns exercise both compact sidecar
     * ranges, including unequal left/right margins for an odd canvas width. */
    if (frame[35u * width] == 0 || frame[35u * width + width - 1] == 0) {
        free(frame); return 15;
    }
    /* Central pixels must use the original VRAM even when an extension exists. */
    if (frame[35u * width + (unsigned)(pad + 0)] != 0xFFFF0000u) {
        free(frame); return 11;
    }
    /* A non-world (HUD) polygon never enters the sidecar. */
    ps1_video_panorama_begin_frame();
    Ps1Vertex a = projected(300, 270), b = projected(380, 270), c = projected(300, 340);
    a.is_projected = b.is_projected = c.is_projected = false;
    if (!ps1_video_draw_triangle(&video, &draw, &a, &b, &c,
                                 0xFFFF00u, 0, 0, false, false, false)) {
        free(frame); return 12;
    }
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 13;
    }
    /* Empty panorama margin must not inherit stale or mirrored data. */
    if (frame[35u * width + (unsigned)(pad + 325)] != 0xFF000000u) {
        free(frame); return 14;
    }
    /* Both far margins must be reset even when a previous frame had 3D
     * polygons; no stale pixels may survive in the compact sidecar. */
    if (frame[35u * width] != 0xFF000000u || frame[35u * width + width - 1] != 0xFF000000u) {
        free(frame); return 16;
    }
    free(frame);
    return 0;
}

/* Mirrored side strips used to duplicate scenery on an otherwise valid
 * 3D viewport. Distinct edge and interior colors make this detectable. */
static int verify_black_margin_fallback(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    memset(&video, 0, sizeof(video));
    ps1_video_panorama_begin_frame();
    for (int y = 240; y < 480; ++y) {
        video.color32[y][0] = 0xFFFF0000u;    /* left edge red */
        video.color32[y][1] = 0xFF0000FFu;    /* nearby blue must NOT be reflected */
        video.color32[y][318] = 0xFFFFFF00u;  /* nearby yellow must NOT be reflected */
        video.color32[y][319] = 0xFF00FF00u;  /* right edge green */
    }
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 2;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, (size_t)width)) {
        free(frame);
        return 3;
    }
    int left = (width - 320) / 2;
    for (int x = 0; x < left; ++x) {
        if (frame[(size_t)48 * width + x] != 0xFF000000u) {
            free(frame);
            return 4;
        }
    }
    for (int x = left + 320; x < width; ++x) {
        if (frame[(size_t)48 * width + x] != 0xFF000000u) {
            free(frame);
            return 5;
        }
    }
    /* The real 4:3 interior must retain its distinct neighboring pixels. */
    if (frame[(size_t)48 * width + (left + 1)] != 0xFF0000FFu ||
        frame[(size_t)48 * width + (left + 318)] != 0xFFFFFF00u) {
        free(frame);
        return 6;
    }
    free(frame);
    return 0;
}

static int verify_world_sprite(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    memset(&video, 0, sizeof(video));
    ps1_video_panorama_begin_frame();
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319;
    state.top = 240; state.bottom = 479;
    /* PS1 15-bit red texels in a separate texture page. */
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 4; ++x) video.vram[y][192 + x] = 0x001Fu;
    }
    video.vram[275][323] = 0x7C00u; /* Texture VRAM sentinel */
    Ps1Sprite sprite = {0};
    sprite.x = 322; sprite.y = 275; sprite.width = 4; sprite.height = 5;
    sprite.tpage = 3u | (2u << 7); sprite.raw_texture = true;
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, true)) return 2;
    if (video.vram[275][323] != 0x7C00u) return 3;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 4;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 5;
    }
    int left = (width - 320) / 2;
    if (frame[(size_t)35 * width + (left + 323)] != 0xFFFF0000u) {
        free(frame); return 6;
    }
    /* Same sprite without GTE provenance must not enter world margins. */
    ps1_video_panorama_begin_frame();
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, false)) {
        free(frame); return 7;
    }
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 8;
    }
    if (frame[(size_t)35 * width + (left + 323)] == 0xFFFF0000u) {
        free(frame); return 9;
    }
    free(frame);
    return 0;
}

static int verify_extension_seams(void) {
    /* Two projected triangles share a diagonal across the old x=319 limit.
     * Every interior pixel must be covered exactly like the original VRAM. */
    if (!ps1_video_configure_panorama(320, 240, 1.2f)) return 1;
    memset(&video, 0, sizeof(video));
    ps1_video_panorama_begin_frame();
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319;
    state.top = 240; state.bottom = 479;
    ps1_video_set_pgxp_vertex_precision(true);
    Ps1Vertex a = projected(312, 272), b = projected(338, 272);
    Ps1Vertex c = projected(338, 300), d = projected(312, 300);
    if (!ps1_video_draw_triangle(&video, &state, &a, &b, &c,
                                 0xFF8040u, 0, 0, false, false, false)) return 2;
    if (!ps1_video_draw_triangle(&video, &state, &a, &c, &d,
                                 0xFF8040u, 0, 0, false, false, false)) return 3;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 4;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 5;
    }
    int margin = (width - 320) / 2;
    for (int y = 274; y < 298; ++y) {
        for (int x = 314; x < 336; ++x) {
            if (frame[(size_t)(y - 240) * width + (margin + x)] != 0xFFFF8040u) {
                free(frame); return 6;
            }
        }
    }
    /* The negative-X seam must use identical edge fill and sidecar layout. */
    Ps1Vertex e = projected(-12, 272), f = projected(12, 272);
    Ps1Vertex g = projected(12, 300), h = projected(-12, 300);
    if (!ps1_video_draw_triangle(&video, &state, &e, &f, &g,
                                  0x40AAFFu, 0, 0, false, false, false) ||
        !ps1_video_draw_triangle(&video, &state, &e, &g, &h,
                                  0x40AAFFu, 0, 0, false, false, false)) {
        free(frame); return 7;
    }
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 8;
    }
    for (int y = 274; y < 298; ++y) {
        for (int x = -8; x < 8; ++x) {
            if (frame[(size_t)(y - 240) * width + (margin + x)] != 0xFF40AAFFu) {
                free(frame); return 9;
            }
        }
    }
    free(frame);
    return 0;
}

/* Hor+ still needs to work when PGXP subpixel precision is disabled. */
static int verify_integer_projected_extension(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    memset(&video, 0, sizeof(video));
    ps1_video_panorama_begin_frame();
    ps1_video_set_pgxp_vertex_precision(false);
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319;
    state.top = 240; state.bottom = 479;
    Ps1Vertex a = projected(300, 270), b = projected(350, 270);
    Ps1Vertex c = projected(300, 320);
    a.has_subpixel = b.has_subpixel = c.has_subpixel = false;
    if (!ps1_video_draw_triangle(&video, &state, &a, &b, &c,
                                 0x00FF00u, 0, 0, false, false, false)) return 2;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 3;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) {
        free(frame); return 4;
    }
    int left = (width - 320) / 2;
    bool rendered = frame[(size_t)40 * width + (left + 323)] == 0xFF00FF00u;
    free(frame);
    ps1_video_set_pgxp_vertex_precision(true);
    return rendered ? 0 : 5;
}

int main(void) {
    int edge_fallback = verify_black_margin_fallback();
    if (edge_fallback != 0) {
        fprintf(stderr, "black-margin panorama fallback failed: %d\n", edge_fallback);
        return 30 + edge_fallback;
    }
    /* 16:10, 16:9, 21:9 and 32:9. No aspect ratio hard-coding. */
    struct { float scale; int width; } ratios[] = {
        {1.2f,384}, {4.0f/3.0f,427}, {1.75f,560}, {8.0f/3.0f,853}
    };
    for (size_t i = 0; i < sizeof(ratios)/sizeof(ratios[0]); ++i) {
        int result = verify_ratio(ratios[i].scale, ratios[i].width);
        if (result) {
            fprintf(stderr, "aspect test %zu failed: %d\n", i, result);
            return result;
        }
    }
    int world_sprite = verify_world_sprite();
    if (world_sprite) { fprintf(stderr, "world sprite test failed: %d\n", world_sprite); return 50 + world_sprite; }
    int integer_world = verify_integer_projected_extension();
    if (integer_world) {
        fprintf(stderr, "non-PGXP panorama failed: %d\n", integer_world);
        return 60 + integer_world;
    }
    int seams = verify_extension_seams();
    if (seams) { fprintf(stderr, "extension seams test failed: %d\n", seams); return 30 + seams; }
    if (!ps1_video_configure_panorama(320, 240, 1.0f)) return 20;
    if (ps1_video_panorama_width(320) != 320) return 21;
    /* Unsupported video modes fall back to original presentation. */
    if (!ps1_video_configure_panorama(640, 480, 2.0f)) return 22;
    if (ps1_video_panorama_width(640) != 640) return 23;
    puts("ps1_panorama_test: dynamic ratios, safe VRAM, world extras, seams, sprites, black-margin fallback passed");
    return 0;
}
