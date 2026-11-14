/* The title already draws a 3x3 scrolling tile field that extends past 4:3.
 * Copying each sprite again at +/-320 creates overlapping translucent tiles
 * only in panorama margins, so their color no longer matches the center. */
#include "ps1_video.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static Ps1Video video;

int main(void) {
    const uint32_t base = 0xFF206020u;
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 2;
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.right = 319;
    state.top = 240; state.bottom = 479; state.offset_y = 240;
    state.tpage = 3u | (2u << 7); /* direct 16-bit texture page at x=192 */
    ps1_video_set_true_color(true);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            video.vram[y][192 + x] = 0xFFFFu; /* semi-transparent white */

    const int positions[] = {-64, 256};
    for (int pass = 0; pass < 2; ++pass) {
        ps1_video_quick_fill(&video, 0, 240, 320, 240, base & 0xFFFFFFu);
        ps1_video_panorama_fill_rows(240, 240, base & 0xFFFFFFu);
        ps1_video_set_2d_widescreen_expansion(true);
        ps1_video_set_2d_widescreen_repeat(pass == 0);
        for (int i = 0; i < 2; ++i) {
            Ps1Sprite sprite = {0};
            sprite.x = positions[i]; sprite.y = 40;
            sprite.width = 64; sprite.height = 64;
            sprite.u = 0; sprite.v = 0; sprite.tpage = state.tpage;
            sprite.rgb24 = 0x808080u;
            sprite.semi_transparent = true;
            if (!ps1_video_draw_sprite_with_provenance(&video, &state,
                                                        &sprite, false)) return 3;
        }
        ps1_video_set_2d_widescreen_expansion(false);
        ps1_video_set_2d_widescreen_repeat(true);
        if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) return 4;
        int pad = (width - 320) / 2;
        uint32_t side = frame[60u * width + (unsigned)(pad - 20)];
        uint32_t middle = frame[60u * width + (unsigned)(pad + 280)];
        if (pass == 0 && side == middle) {
            fprintf(stderr, "invalid fixture: repeated translucent tiles did not overdraw\n");
            return 5;
        }
        if (pass == 1 && side != middle) {
            fprintf(stderr, "title side color %08x differs from center %08x\n", side, middle);
            return 6;
        }
    }
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    puts("ps1_title_scroll_overdraw_test: title tile blend has no duplicate side copies");
    return 0;
}
