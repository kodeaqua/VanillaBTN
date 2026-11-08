#include "ps1_video.h"

#include <stdint.h>

static Ps1Video video;

int main(void) {
    Ps1DrawState draw;
    const uint16_t source[2][3] = {
        {0x001F, 0x03E0, 0x7C00},
        {0x7FFF, 0x801F, 0x0000},
    };
    uint32_t target[2][4] = {{0}};

    if (!ps1_video_upload(&video, 2, 240, 3, 2, &source[0][0], 3)
        || !ps1_video_copy_argb(&video, 2, 240, 3, 2, &target[0][0], 4)) {
        return 1;
    }
    if (target[0][0] != 0xFFFF0000u || target[0][1] != 0xFF00FF00u
        || target[0][2] != 0xFF0000FFu || target[1][0] != 0xFFFFFFFFu
        || target[1][1] != 0xFFFF0000u || target[1][2] != 0xFF000000u
        || target[0][3] != 0 || target[1][3] != 0) {
        return 2;
    }
    if (ps1_video_upload(&video, 1023, 0, 2, 1, &source[0][0], 3)
        || ps1_video_upload(&video, 0, 0, 3, 1, &source[0][0], 2)
        || ps1_video_copy_argb(&video, -1, 0, 1, 1, &target[0][0], 4)
        || ps1_video_copy_argb(&video, 0, 0, 3, 1, &target[0][0], 2)) {
        return 3;
    }
    ps1_video_quick_fill(&video, 31, 511, 17, 2, 0xF80000u);
    if (video.vram[511][15] != 0 || video.vram[511][16] != 0x001Fu
        || video.vram[0][47] != 0x001Fu || video.vram[0][48] != 0) {
        return 4;
    }
    ps1_video_quick_fill(&video, 1008, 2, 32, 1, 0x0000F8u);
    if (video.vram[2][1008] != 0x7C00u || video.vram[2][15] != 0x7C00u) {
        return 5;
    }

    ps1_draw_state_reset(&draw);
    draw.left = 10;
    draw.top = 10;
    draw.right = 12;
    draw.bottom = 11;
    draw.offset_x = 9;
    draw.offset_y = 10;
    if (!ps1_video_draw_tile(&video, &draw, 0, 0, 5, 2, 0x00F800u, false)
        || video.vram[10][9] != 0 || video.vram[10][10] != 0x03E0u
        || video.vram[11][12] != 0x03E0u || video.vram[10][13] != 0) {
        return 6;
    }
    draw.force_mask = true;
    if (!ps1_video_draw_tile(&video, &draw, 1, 0, 1, 1, 0xF80000u, false)
        || video.vram[10][10] != 0x801Fu) {
        return 7;
    }
    draw.force_mask = false;
    draw.check_mask = true;
    if (!ps1_video_draw_tile(&video, &draw, 1, 0, 1, 1, 0x0000F8u, false)
        || video.vram[10][10] != 0x801Fu) {
        return 8;
    }
    draw.check_mask = false;
    if (!ps1_video_draw_tile(&video, &draw, 1, 0, 1, 1, 0x0000F8u, true)
        || video.vram[10][10] != 0x3C0Fu) {
        return 9;
    }

    /* The four GP0(E1) blend modes operate on five-bit channels. */
    const uint16_t expected[4] = {16, 31, 0, 20};
    for (unsigned mode = 0; mode < 4; ++mode) {
        draw.blend_mode = mode;
        video.vram[10][10] = 16;
        if (!ps1_video_draw_tile(&video, &draw, 1, 0, 1, 1, 0x800000u, true)
            || video.vram[10][10] != expected[mode]) {
            return 10;
        }
    }
    draw.blend_mode = 0;
    video.vram[10][10] = 31;
    if (!ps1_video_draw_tile(&video, &draw, 1, 0, 1, 1, 0xF80000u, true)
        || video.vram[10][10] != 31) {
        return 11;
    }
    ps1_video_quick_fill(&video, 0, 4, 1, 1, 0x123456u);
    if (video.vram[4][0] != 0x28C2u || video.vram[4][15] != 0x28C2u
        || video.vram[4][16] != 0) {
        return 12;
    }
    return 0;
}
