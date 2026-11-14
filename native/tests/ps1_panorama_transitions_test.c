/* Native-only panoramic presentation: full-screen PS1 clears/fades must affect
 * both sidecar pages, whereas authored background repeats must not touch VRAM. */
#include "ps1_video.h"
#include "ps1_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;

static uint32_t sample(int page_y, int screen_x, int screen_y) {
    const int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(uint32_t));
    if (!frame) return 0;
    uint32_t result = 0;
    const int x = (width - 320) / 2 + screen_x;
    if (x >= 0 && x < width && ps1_video_copy_panorama(&video, 0, page_y, 320, 240,
                                                       frame, (size_t)width))
        result = frame[(size_t)screen_y * width + x];
    free(frame);
    return result;
}

static Ps1DrawState page_state(int y) {
    Ps1DrawState s;
    ps1_draw_state_reset(&s);
    s.left = 0; s.right = 319; s.top = y; s.bottom = y + 239;
    s.offset_x = 0; s.offset_y = y;
    return s;
}

static Ps1Vertex vertex(int x, int y) {
    Ps1Vertex v = {0};
    v.x = x; v.y = y; v.sub_x = (float)x; v.sub_y = (float)y;
    v.z = 1.0f; v.w = 1.0f;
    return v;
}

static int test_clear_and_fade(void) {
    ps1_video_panorama_begin_frame();
    ps1_video_quick_fill(&video, 0, 0, 320, 480, 0x806040u);
    if (sample(0, -20, 120) != 0xFF806040u ||
        sample(240, 330, 120) != 0xFF806040u) return 1;
    Ps1DrawState state = page_state(240);
    state.blend_mode = 0; /* PS1 semi-transparent 50/50 blend */
    Ps1Vertex a = vertex(0, 0), b = vertex(320, 0);
    Ps1Vertex c = vertex(0, 240), d = vertex(320, 240);
    if (!ps1_video_draw_quad(&video, &state, &a, &b, &c, &d,
                             0x200808u, 0, 0, false, false, true)) return 2;
    const uint32_t center = sample(240, 100, 120);
    if (center != sample(240, -20, 120) || center != sample(240, 335, 120)) return 3;
    if (sample(0, -20, 120) != 0xFF806040u) return 4; /* other page intact */
    /* Render next frame to A, leaving B displayed and unchanged. */
    ps1_video_panorama_clear_rows(0, 240);
    ps1_video_quick_fill(&video, 0, 0, 320, 240, 0x112233u);
    if (sample(0, -20, 120) != 0xFF112233u ||
        sample(240, -20, 120) != center) return 5;
    Ps1DrawState state_a = page_state(0);
    /* Some transitions use GP0 TILE_VAR rather than POLY_F4. */
    if (!ps1_video_draw_tile(&video, &state_a, 0, 0, 320, 240,
                             0x203040u, false)) return 6;
    if (sample(0, -20, 120) != 0xFF203040u ||
        sample(0, 335, 120) != 0xFF203040u ||
        sample(0, 100, 120) != 0xFF203040u ||
        sample(240, -20, 120) != center) return 7;
    return 0;
}

static int test_tiled_background(void) {
    ps1_video_panorama_clear_rows(240, 240);
    Ps1DrawState state = page_state(240);
    /* 15-bit red texels, page x=192, 16-bit texture depth. */
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) video.vram[y][192 + x] = 0x001Fu;
    state.tpage = 3u | (2u << 7);
    Ps1Sprite sprite = {0};
    sprite.x = 11; sprite.y = 35; sprite.width = 4; sprite.height = 4;
    sprite.tpage = state.tpage; sprite.raw_texture = true;
    /* x=11 is repeated at x=331 by explicitly marked authored background,
     * whereas unmarked screen-space/HUD sprites must remain in 4:3. */
    ps1_video_set_2d_widescreen_expansion(true);
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, false)) return 1;
    ps1_video_set_2d_widescreen_expansion(false);
    if (sample(240, 332, 36) != 0xFFFF0000u) return 2;
    /* A repeated sprite on the far right of the PS1 view supplies pixels
     * to negative X (without altering center artwork). */
    sprite.x = 307; sprite.y = 40;
    ps1_video_set_2d_widescreen_expansion(true);
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, false)) return 6;
    ps1_video_set_2d_widescreen_expansion(false);
    if (sample(240, -12, 41) != 0xFFFF0000u) return 7;
    sprite.x = 11; sprite.y = 35;
    if (video.vram[276][332] != 0) return 3; /* side never corrupts texture VRAM */
    ps1_video_panorama_clear_rows(240, 240);
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, false)) return 4;
    if (sample(240, 332, 36) != 0xFF000000u) return 5;
    return 0;
}

static int test_world_ot_context(void) {
    ps1_video_panorama_clear_rows(240, 240);
    Ps1DrawState state = page_state(240);
    state.tpage = 3u | (2u << 7);
    /* SPRT packet with normal screen-space XY, no PGXP vertex address. */
    uint32_t words[4] = {0x65808080u, (uint32_t)330u | (35u << 16), 0u, (4u << 16) | 4u};
    ps1_video_set_world_primitive_context(true);
    if (!ps1_gpu_exec(&video, &state, words, 4)) return 1;
    ps1_video_set_world_primitive_context(false);
    if (sample(240, 332, 36) != 0xFFFF0000u) return 2;
    ps1_video_panorama_clear_rows(240, 240);
    if (!ps1_gpu_exec(&video, &state, words, 4)) return 3;
    if (sample(240, 332, 36) != 0xFF000000u) return 4;
    return 0;
}


/* The original stepped fade switches the SDK drawing area to 640x480 and
 * renders one full-screen POLY_F4. The host continues presenting the last
 * 320x240 world panorama: the quad must blend BOTH panoramic framebuffer
 * pages as it blends their VRAM pixels, or the margin stays un-faded. */
static int test_640x480_stepped_fade(void) {
    ps1_video_panorama_begin_frame();
    ps1_video_quick_fill(&video, 0, 0, 320, 240, 0x284860u);
    ps1_video_quick_fill(&video, 0, 240, 320, 240, 0x284860u);
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 639; state.bottom = 479;
    state.blend_mode = 0;
    Ps1Vertex a = vertex(0, 0), b = vertex(640, 0);
    Ps1Vertex c = vertex(0, 480), d = vertex(640, 480);
    if (!ps1_video_draw_quad(&video, &state, &a, &b, &c, &d,
                             0x101820u, 0, 0, false, false, true)) return 1;
    for (int page = 0; page <= 240; page += 240) {
        uint32_t center = sample(page, 100, 80);
        uint32_t left = sample(page, -20, 80);
        uint32_t right = sample(page, 334, 80);
        if (center != left || center != right || center == 0xFF284860u) return 2;
    }
    return 0;
}

int main(void) {
    ps1_video_set_true_color(true);
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    int r = test_clear_and_fade();
    if (r) { fprintf(stderr, "clear/fade step %d failed\n", r); return 10 + r; }
    r = test_tiled_background();
    if (r) { fprintf(stderr, "background step %d failed\n", r); return 20 + r; }
    r = test_world_ot_context();
    if (r) { fprintf(stderr, "world context step %d failed\n", r); return 30 + r; }
    r = test_640x480_stepped_fade();
    if (r) { fprintf(stderr, "640x480 fade step %d failed\n", r); return 40 + r; }
    ps1_video_set_world_primitive_context(false);
    ps1_video_configure_panorama(0, 0, 1.0f);
    puts("ps1_panorama_transitions_test: clear, 320/640 fullscreen fades, background tiles and world sprite passed");
    return 0;
}
