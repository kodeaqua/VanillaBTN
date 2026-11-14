/* Regression for the original four-frame 24px menu background grid.
 * A 320px viewport is not a repeat period of a 96px animated pattern. */
#include "ps1_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
static const uint16_t colors15[4] = {0x001Fu, 0x03E0u, 0x7C00u, 0x7FFFu};
static const uint32_t colors32[4] = {0xFFFF0000u, 0xFF00FF00u,
                                     0xFF0000FFu, 0xFFFFFFFFu};

static uint32_t pixel(const uint32_t *frame, int width, int x, int y) {
    return frame[(size_t)y * width + (width - 320) / 2 + x];
}

int main(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    const int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 2;
    ps1_video_set_true_color(true);
    ps1_video_panorama_clear_rows(240, 240);
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319;
    state.top = 240; state.bottom = 479;
    state.offset_y = 240;
    state.tpage = 3u | (2u << 7); /* 16-bit textures at x=192 */
    for (int type = 0; type < 4; ++type)
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 24; ++x)
                video.vram[y][192 + type * 24 + x] = colors15[type];
    ps1_video_set_2d_widescreen_expansion(true);
    for (int i = 0; i < 15; ++i) {
        Ps1Sprite sprite = {0};
        sprite.x = -12 + i * 24;
        sprite.y = 100;
        sprite.width = sprite.height = 24;
        sprite.u = (unsigned)(((i + 1) & 3) * 24);
        sprite.v = 0;
        sprite.tpage = state.tpage;
        sprite.raw_texture = true;
        if (!ps1_video_draw_sprite_with_provenance(&video, &state, &sprite, false))
            return 3;
    }
    ps1_video_set_2d_widescreen_expansion(false);
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width))
        return 4;
    /* The left neighbors precede the first in-frame sprite, and the right
     * neighbors follow the last in-frame sprite. Color phase must agree. */
    const struct { int x; int type; } checks[] = {
        {-49, 3}, {-30, 0}, {-10, 1}, {14, 2},
        {308, 2}, {333, 3}, {352, 0}, {370, 0}
    };
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i) {
        if (checks[i].x < -(width - 320) / 2 ||
            checks[i].x >= 320 + (width - 320 + 1) / 2) continue;
        uint32_t got = pixel(frame, width, checks[i].x, 110);
        if (got != colors32[checks[i].type]) {
            fprintf(stderr, "grid phase x=%d: got %08x expected %08x\n",
                    checks[i].x, got, colors32[checks[i].type]);
            return 5;
        }
    }
    /* Replaying the same 2D HUD tile without the background marker must not
     * produce any side copy. */
    ps1_video_panorama_clear_rows(240, 240);
    Ps1Sprite hud = {0};
    hud.x = 11; hud.y = 100; hud.width = hud.height = 24;
    hud.tpage = state.tpage; hud.raw_texture = true;
    if (!ps1_video_draw_sprite_with_provenance(&video, &state, &hud, false) ||
        !ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) return 6;
    if (pixel(frame, width, -30, 110) != 0xFF000000u) return 7;
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    puts("ps1_menu_grid_phase_test: 24px four-frame menu seamless in Hor+ margins");
    return 0;
}
