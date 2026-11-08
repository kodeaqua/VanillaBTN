/*
 * ps1_gpu_test: synthetic checks for the GP0 command dispatcher.
 *
 * Each check builds a minimal packet buffer that mirrors the byte layout the
 * PS1 primitive builders produce, submits it through ps1_gpu_exec, and verifies
 * the resulting VRAM words.  The draw state is kept at its reset values unless
 * a specific command is being tested.
 */
#include "ps1_gpu.h"
#include "ps1_video.h"

#include <stdint.h>
#include <string.h>

static Ps1Video  video;
static Ps1DrawState draw;

/*
 * Helper: pack colour components into the 0xRRGGBB word the game stores.
 * The GP0 first word has the command in bits 31-24 and r/g/b in 23-0
 * with red in bits 23-16 (matching the PS1 convention).
 */
static uint32_t cmd_rgb(unsigned code, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)code << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t xy_word(int16_t x, int16_t y) {
    return ((uint32_t)(uint16_t)y << 16) | (uint16_t)x;
}

static uint32_t wh_word(int16_t w, int16_t h) {
    return ((uint32_t)(uint16_t)h << 16) | (uint16_t)w;
}

int main(void) {
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);

    /* --- GP0(02): quick fill ---
     * Fill a 16-pixel-wide stripe at (32, 10) with pure red (0xF80000 = red
     * in 8-bit, which maps to BGR555 0x001F).  Width 1 rounds up to 16. */
    {
        const uint32_t pkt[3] = {
            cmd_rgb(0x02, 0xF8, 0x00, 0x00),
            xy_word(32, 10),
            wh_word(1, 1),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 3)) return 1;
        /* quick_fill rounds X to 16-pixel boundary: 32 & ~15 = 32. */
        if (video.vram[10][32] != 0x001Fu) return 1;
        if (video.vram[10][47] != 0x001Fu) return 1;
        if (video.vram[10][48] != 0)       return 1;
    }

    /* --- GP0(60): untextured variable-size tile, opaque ---
     * Draw a green (0x00F800) 4x2 tile at (5, 20). */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t pkt[3] = {
            cmd_rgb(0x60, 0x00, 0xF8, 0x00),
            xy_word(5, 20),
            wh_word(4, 2),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 3)) return 2;
        if (video.vram[20][5] != 0x03E0u) return 2;
        if (video.vram[21][8] != 0x03E0u) return 2;
        if (video.vram[20][9] != 0)       return 2;
    }

    /* --- GP0(68): single-pixel tile (1x1) ---
     * Draw one blue pixel at (100, 50). */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t pkt[2] = {
            cmd_rgb(0x68, 0x00, 0x00, 0xF8),
            xy_word(100, 50),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 2)) return 3;
        if (video.vram[50][100] != 0x7C00u) return 3;
        if (video.vram[50][101] != 0)       return 3;
    }

    /* --- GP0(62): semi-transparent variable tile ---
     * Back pixel has value 16 (red channel only), front is red 0xF80000
     * which maps to BGR555 value 31 in red channel.  Blend mode 0 averages:
     * (16 + 31) >> 1 = 23. */
    {
        ps1_draw_state_reset(&draw);
        draw.blend_mode = 0;
        video.vram[30][10] = 16u; /* back = 16 in red channel */
        const uint32_t pkt[3] = {
            cmd_rgb(0x62, 0xF8, 0x00, 0x00),
            xy_word(10, 30),
            wh_word(1, 1),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 3)) return 4;
        if (video.vram[30][10] != 23u) return 4;
    }

    /* --- GP0(64): textured variable sprite ---
     * Set up a 4-bit CLUT: index 0 transparent, index 1 -> 0x001F (red in
     * BGR555), index 2 -> 0x03E0 (green).  TPage X=1 (base_x=64), Y=0,
     * depth=0 (4-bit).  CLUT at (0, 2) = clut field 2<<6 = 0x0080.
     * Place two texel-index words: 0x2100 gives nibbles [0,1,2,0,...]. */
    {
        ps1_draw_state_reset(&draw);
        /* Set tpage via GP0(E1): tpage = 1 (x64=64), depth=4-bit (bits 7-8 = 0). */
        const uint32_t e1[1] = { cmd_rgb(0xE1, 0, 0, 0) | 1u };
        if (!ps1_gpu_exec(&video, &draw, e1, 1)) return 5;

        /* Write CLUT at VRAM row 2, column 0: 16 entries. */
        video.vram[2][0] = 0;       /* index 0 = transparent */
        video.vram[2][1] = 0x001Fu; /* index 1 = red */
        video.vram[2][2] = 0x03E0u; /* index 2 = green */

        /* Write 4-bit texels at TPage base (64,0): one word = 8 nibbles.
         * We set nibbles 0..3 to 0,1,2,0 -> transparent, red, green, transparent. */
        video.vram[0][64] = 0x0210u; /* nibbles: [0]=0,[1]=1,[2]=2,[3]=0 */

        /* Clear destination pixels. */
        video.vram[40][10] = 0xFFFFu;
        video.vram[40][11] = 0xFFFFu;
        video.vram[40][12] = 0xFFFFu;
        video.vram[40][13] = 0xFFFFu;

        /* CLUT field: row=2, x=0 -> (2<<6)|0 = 0x80. */
        unsigned clut_field = (2u << 6) | 0u;
        const uint32_t pkt[4] = {
            cmd_rgb(0x64, 0x80, 0x80, 0x80),         /* neutral modulation */
            xy_word(10, 40),                          /* position */
            (uint32_t)(0 | (0 << 8) | (clut_field << 16)), /* u=0, v=0, clut */
            wh_word(4, 1),                            /* 4x1 pixels */
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 4)) return 5;
        /* Index 0: transparent -> dest unchanged (0xFFFF). */
        if (video.vram[40][10] != 0xFFFFu) return 5;
        /* Index 1: red (0x001F) modulated by 0x808080 -> neutral -> 0x001F. */
        if (video.vram[40][11] != 0x001Fu) return 5;
        /* Index 2: green (0x03E0) modulated by 0x808080 -> neutral -> 0x03E0. */
        if (video.vram[40][12] != 0x03E0u) return 5;
        /* Index 0: transparent -> dest unchanged (0xFFFF). */
        if (video.vram[40][13] != 0xFFFFu) return 5;
    }

    /* --- GP0(E3/E4/E5/E6): draw environment commands ---
     * Verify that draw state is correctly updated by environment packets. */
    {
        ps1_draw_state_reset(&draw);

        /* GP0(E3): set draw area TL to (10, 20). */
        const uint32_t e3[1] = { (0xE3u << 24) | (20u << 10) | 10u };
        if (!ps1_gpu_exec(&video, &draw, e3, 1)) return 6;
        if (draw.left != 10 || draw.top != 20) return 6;

        /* GP0(E4): set draw area BR to (100, 200). */
        const uint32_t e4[1] = { (0xE4u << 24) | (200u << 10) | 100u };
        if (!ps1_gpu_exec(&video, &draw, e4, 1)) return 6;
        if (draw.right != 100 || draw.bottom != 200) return 6;

        /* GP0(E5): set draw offset to (-5, 7). */
        int ox = -5, oy = 7;
        uint32_t e5_param = (uint32_t)((ox & 0x7FF) | ((oy & 0x7FF) << 11));
        const uint32_t e5[1] = { (0xE5u << 24) | e5_param };
        if (!ps1_gpu_exec(&video, &draw, e5, 1)) return 6;
        if (draw.offset_x != -5 || draw.offset_y != 7) return 6;

        /* GP0(E6): force mask and check mask. */
        const uint32_t e6[1] = { (0xE6u << 24) | 3u };
        if (!ps1_gpu_exec(&video, &draw, e6, 1)) return 6;
        if (!draw.force_mask || !draw.check_mask) return 6;
    }

    /* --- NULL / zero-count guard ---
     * ps1_gpu_exec must reject bad inputs without crashing. */
    {
        const uint32_t pkt[1] = { cmd_rgb(0x60, 0, 0xFF, 0) };
        if (ps1_gpu_exec(NULL,   &draw, pkt, 1)) return 7;
        if (ps1_gpu_exec(&video, NULL,  pkt, 1)) return 7;
        if (ps1_gpu_exec(&video, &draw, NULL, 1)) return 7;
        if (ps1_gpu_exec(&video, &draw, pkt, 0)) return 7;
    }

    return 0;
}
