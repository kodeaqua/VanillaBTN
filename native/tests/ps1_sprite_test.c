#include "ps1_video.h"

static Ps1Video video;

int main(void) {
    Ps1DrawState draw;
    Ps1Sprite sprite = {10, 10, 4, 1, 0, 1, 1, 2u << 6,
                        0x808080u, false, true};
    ps1_draw_state_reset(&draw);

    /* Four 4-bit indexes: transparent, opaque red, blended green, blended black. */
    video.vram[1][64] = 0x3210u;
    video.vram[2][0] = 0;
    video.vram[2][1] = 0x001Fu;
    video.vram[2][2] = 0x83E0u;
    video.vram[2][3] = 0x8000u;
    video.vram[10][10] = 0x7C00u;
    video.vram[10][11] = 0x7C00u;
    video.vram[10][12] = 0x001Fu;
    video.vram[10][13] = 0x7FFFu;
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[10][10] != 0x7C00u || video.vram[10][11] != 0x001Fu
        || video.vram[10][12] != 0x81EFu || video.vram[10][13] != 0xBDEFu) {
        return 1;
    }

    sprite.x = 14;
    sprite.width = 1;
    sprite.u = 1;
    sprite.rgb24 = 0x400000u;
    sprite.semi_transparent = false;
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[10][14] != 0x000Fu) {
        return 2;
    }
    sprite.raw_texture = true;
    sprite.rgb24 = 0;
    sprite.x = 15;
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[10][15] != 0x001Fu) {
        return 3;
    }
    draw.check_mask = true;
    sprite.x = 16;
    video.vram[10][16] = 0x8000u;
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[10][16] != 0x8000u) {
        return 4;
    }
    draw.check_mask = false;

    /* The same drawing path accepts 8-bit CLUT and direct 15-bit pages. */
    video.vram[0][128] = 0x0201u;
    sprite = (Ps1Sprite){20, 20, 2, 1, 0, 0, 2u | (1u << 7), 2u << 6,
                          0, true, false};
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[20][20] != 0x001Fu || video.vram[20][21] != 0x83E0u) {
        return 5;
    }
    video.vram[0][192] = 0x8000u;
    video.vram[0][193] = 0;
    video.vram[21][31] = 0x7FFFu;
    sprite = (Ps1Sprite){30, 21, 2, 1, 0, 0, 3u | (2u << 7), 0,
                          0, true, false};
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[21][30] != 0x8000u || video.vram[21][31] != 0x7FFFu) {
        return 6;
    }
    draw.left = 11;
    draw.right = 12;
    video.vram[10][10] = 0x7C00u;
    video.vram[10][11] = 0;
    video.vram[10][12] = 0;
    sprite = (Ps1Sprite){10, 10, 4, 1, 0, 1, 1, 2u << 6,
                          0, true, false};
    if (!ps1_video_draw_sprite(&video, &draw, &sprite)
        || video.vram[10][10] != 0x7C00u || video.vram[10][11] != 0x001Fu
        || video.vram[10][12] != 0x83E0u) {
        return 7;
    }
    ps1_draw_state_reset(&draw);
    sprite.tpage = 3u << 7;
    if (ps1_video_draw_sprite(&video, &draw, &sprite)) {
        return 8;
    }
    return 0;
}
