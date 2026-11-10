#include "ps1_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

static Ps1Video video;

static void setup_test_texture(void) {
    memset(&video, 0, sizeof(video));
    /* Texture page 0, 16-bit direct color (depth 2):
     * Tile A: (u: 0..7, v: 0..7) = Solid RED (0x001F in 15-bit BGR: red=31, green=0, blue=0)
     * Tile B: (u: 8..15, v: 0..7) = Solid BLUE (0x7C00 in 15-bit BGR: red=0, green=0, blue=31)
     */
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            video.vram[y][x] = 0x001Fu; /* Red */
        }
        for (int x = 8; x < 16; ++x) {
            video.vram[y][x] = 0x7C00u; /* Blue */
        }
    }
}

static void test_texture_bleeding_fix(void) {
    setup_test_texture();

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;

    /* Draw a sprite that covers only Tile A (u: 0..7, width: 8, height: 8) */
    Ps1Sprite sprite = {
        .x = 20, .y = 20, .width = 8, .height = 8,
        .u = 0, .v = 0,
        .tpage = (2u << 7), /* 16-bit direct texture depth */
        .clut = 0,
        .rgb24 = 0x808080u,
        .raw_texture = true,
        .semi_transparent = false
    };

    /* First, with texture bleeding fix ENABLED */
    ps1_video_set_texture_bleeding_fix(true);
    bool ok = ps1_video_draw_sprite(&video, &state, &sprite);
    assert(ok);

    /* Verify that every pixel in the drawn 8x8 sprite is purely RED, none are BLUE */
    for (int r = 20; r < 28; ++r) {
        for (int c = 20; c < 28; ++c) {
            uint16_t pix = video.vram[r][c];
            uint32_t red = pix & 31u;
            uint32_t blue = (pix >> 10) & 31u;
            assert(red == 31);
            assert(blue == 0);
        }
    }
}

static void test_alpha_halo_fix(void) {
    memset(&video, 0, sizeof(video));
    /* Place an opaque WHITE texel at (0, 0) and transparent (0x0000) at (1, 0), (0, 1), (1, 1) */
    video.vram[0][0] = 0x7FFFu; /* White: R=31, G=31, B=31 */
    video.vram[0][1] = 0x0000u; /* Transparent */
    video.vram[1][0] = 0x0000u; /* Transparent */
    video.vram[1][1] = 0x0000u; /* Transparent */

    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0; state.top = 0;
    state.right = 100; state.bottom = 100;

    Ps1Sprite sprite = {
        .x = 10, .y = 10, .width = 2, .height = 2,
        .u = 0, .v = 0,
        .tpage = (2u << 7), /* 16-bit depth */
        .clut = 0,
        .rgb24 = 0x808080u,
        .raw_texture = true,
        .semi_transparent = false
    };

    /* With bilinear filtering and alpha halo fix ON */
    ps1_video_set_texture_filter(PS1_FILTER_BILINEAR);
    ps1_video_set_alpha_halo_fix(true);

    bool ok = ps1_video_draw_sprite(&video, &state, &sprite);
    assert(ok);

    /* Pixel at (10, 10) corresponds to u=0, v=0, which is pure opaque white.
     * With alpha halo fix, its color must remain pure white (R=31, G=31, B=31),
     * NOT darkened by neighboring transparent black pixels! */
    uint16_t pix = video.vram[10][10];
    uint32_t red = pix & 31u;
    uint32_t green = (pix >> 5) & 31u;
    uint32_t blue = (pix >> 10) & 31u;
    assert(red == 31 && green == 31 && blue == 31);

    /* Now test with alpha halo fix OFF (authentic PS1 behavior where black leaks in) */
    memset(&video, 0, sizeof(video));
    video.vram[0][0] = 0x7FFFu;
    ps1_video_set_alpha_halo_fix(false);

    /* Draw with a sub-pixel offset so bilinear filtering blends opaque white with transparent black */
    /* Restore filter to nearest for remaining tests */
    ps1_video_set_texture_filter(PS1_FILTER_NEAREST);
    ps1_video_set_alpha_halo_fix(true);
}

static void test_mipmapping_toggle(void) {
    ps1_video_set_mipmapping(true);
    assert(ps1_video_get_mipmapping() == true);

    ps1_video_set_mipmapping(false);
    assert(ps1_video_get_mipmapping() == false);

    ps1_video_set_mipmapping(true);
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    test_texture_bleeding_fix();
    test_alpha_halo_fix();
    test_mipmapping_toggle();
    printf("All ps1_tex_filter tests passed!\n");
    return 0;
}
