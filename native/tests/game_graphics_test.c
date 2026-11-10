#include "game_graphics.h"

#include <assert.h>
#include <string.h>

static Ps1Video video;

static void put16(uint8_t *p, unsigned value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void test_texture_banks(void) {
    uint8_t bank[24] = {0x0D,0,1,0};
    bank[4] = 4; bank[7] = 4;
    put16(bank + 8, 10); put16(bank + 10, 2);
    put16(bank + 12, 2); put16(bank + 14, 1);
    put16(bank + 16, 0x001F); put16(bank + 18, 0x03E0);
    assert(game_texture_pack_upload(&video, bank, 20));
    assert(video.vram[2][10] == 0x001F && video.vram[2][11] == 0x03E0);
    bank[4] = 5; bank[7] = 0;
    bank[16] = 252; /* Four literal bytes. */
    put16(bank + 17, 0x7C00); put16(bank + 19, 0x801F);
    assert(game_texture_pack_upload(&video, bank, sizeof(bank)));
    assert(video.vram[2][10] == 0x7C00 && video.vram[2][11] == 0x801F);
    bank[7] = 8;
    put16(bank + 12, 1); put16(bank + 14, 2);
    bank[17] = 31; bank[18] = 224; bank[19] = 0; bank[20] = 3;
    assert(game_texture_pack_upload(&video, bank, sizeof(bank)));
    assert(video.vram[2][10] == 0x001F && video.vram[3][10] == 0x03E0);
    assert(!game_texture_pack_upload(&video, bank, 19));
    bank[0] = 0;
    assert(!game_texture_pack_upload(&video, bank, sizeof(bank)));
}

static void test_animation_layout(void) {
    uint8_t ani[82] = {14};
    Ps1DrawState draw;
    GameAnimation clock = {0};
    /* Five offset-based sections, matching src/main/graphics/anim.h. */
    ani[16] = 36; ani[20] = 52; ani[24] = 56; ani[28] = 68; ani[32] = 72;
    put16(ani + 36, 64); put16(ani + 40, 1); put16(ani + 42, 1);
    put16(ani + 44, 16); put16(ani + 46, 1);
    put16(ani + 52, 1);
    put16(ani + 58, 1); put16(ani + 60, 2); put16(ani + 62, 3);
    put16(ani + 68, 1);
    ani[78] = 2; ani[79] = 255;
    memset(&video, 0, sizeof(video));
    video.vram[0][64] = 0x1111;
    video.vram[1][17] = 0x001F;
    ps1_draw_state_reset(&draw);
    assert(game_animation_draw(&video, &draw, ani, sizeof(ani), 0, 0, 0, 0));
    assert(video.vram[3][2] == 0x001F && video.vram[3][5] == 0x001F);
    assert(video.vram[3][6] == 0);
    video.vram[0][64] = 0x4321;
    video.vram[1][18] = 0x03E0;
    video.vram[1][19] = 0x7C00;
    video.vram[1][20] = 0x7FFF;
    ani[64] = 1; /* FT4 parts mirror their UVs; SPRT parts do not. */
    assert(game_animation_draw_quads(&video, &draw, ani, sizeof(ani), 0, 0, 0, 0));
    assert(video.vram[3][2] == 0x7FFF && video.vram[3][5] == 0x001F);
    assert(game_animation_tick(&clock, ani, sizeof(ani)) && clock.frame == 0);
    assert(game_animation_tick(&clock, ani, sizeof(ani)) && clock.frame == 0);
    assert(!game_animation_draw(&video, &draw, ani, sizeof(ani), 0, 1, 0, 0));
    ani[24] = 255;
    assert(!game_animation_draw(&video, &draw, ani, sizeof(ani), 0, 0, 0, 0));
}

int main(void) {
    test_texture_banks();
    test_animation_layout();
    return 0;
}
