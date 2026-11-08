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
 * GP0 packs the command above B/G/R bytes: red occupies bits 0..7.
 */
static uint32_t cmd_rgb(unsigned code, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)code << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
}

static uint32_t xy_word(int16_t x, int16_t y) {
    return ((uint32_t)(uint16_t)y << 16) | (uint16_t)x;
}

static uint32_t wh_word(int16_t w, int16_t h) {
    return ((uint32_t)(uint16_t)h << 16) | (uint16_t)w;
}

int main(void) {
    /* Lines include both endpoints and a coincident pair draws one pixel. */
    ps1_draw_state_reset(&draw);
    const uint32_t line[] = {0x400000F8, 10u | (20u << 16), 13u | (20u << 16)};
    if (!ps1_gpu_exec(&video, &draw, line, 3)) return 101;
    for (unsigned x = 10; x <= 13; ++x) if (video.vram[20][x] != 0x001F) return 102;
    if (video.vram[20][9] || video.vram[20][14]) return 103;
    const uint32_t point[] = {0x4000F800, 30u | (30u << 16), 30u | (30u << 16)};
    if (!ps1_gpu_exec(&video, &draw, point, 3) || video.vram[30][30] != 0x03E0) return 104;
    /* Independent Gouraud quad: black at y=40, red=248 at y=44.
     * Interior rows interpolate before truncating to five-bit channels. */
    const uint32_t gradient[] = {0x38000000, 40u | (40u << 16),
        0, 44u | (40u << 16), 248, 40u | (44u << 16), 248, 44u | (44u << 16)};
    if (!ps1_gpu_exec(&video, &draw, gradient, 8)) return 105;
    if (video.vram[41][41] != 7 || video.vram[42][41] != 15
        || video.vram[43][41] != 23 || video.vram[44][41] != 0) return 106;
    memset(&video, 0, sizeof(video));
    ps1_draw_state_reset(&draw);
    /* Independent GP0 byte fixture: R occupies bits 0..7, B bits 16..23. */
    const uint32_t red_pixel[] = {0x680000F8u, 0x00050005u};
    if (!ps1_gpu_exec(&video, &draw, red_pixel, 2) || video.vram[5][5] != 0x001Fu) return 100;
    memset(&video, 0, sizeof(video));

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

    /* --- GP0(E6): mask bit setting --- */
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

        /* GP0(A0): inline CPU-to-VRAM image transfer (odd pixel count + bounds). */
        const uint32_t load_ok[5] = {
            0xA0000000u,
            xy_word(12, 34),
            wh_word(3, 1),
            0x03E0001Fu,
            0xDEAD7C00u,
        };
        if (!ps1_gpu_exec(&video, &draw, load_ok, 5)) return 6;
        if (video.vram[34][12] != 0x001Fu
            || video.vram[34][13] != 0x03E0u
            || video.vram[34][14] != 0x7C00u
            || video.vram[34][15] != 0) return 6;
        if (ps1_gpu_exec(&video, &draw, load_ok, 4)) return 6;
        const uint32_t load_oob[4] = { 0xA0000000u, xy_word(1023, 0), wh_word(2, 1), 0 };
        if (ps1_gpu_exec(&video, &draw, load_oob, 4)) return 6;
    }

    /* Adjacent triangles must blend each pixel of a quad exactly once. */
    {
        const Ps1Vertex vertices[4] = {{100,100,0,0},{104,100,0,0},{100,104,0,0},{104,104,0,0}};
        ps1_draw_state_reset(&draw);
        draw.blend_mode = 1;
        if (!ps1_video_draw_quad(&video, &draw, vertices, vertices + 1, vertices + 2,
            vertices + 3, 0x080000u, 0, 0, false, false, true)) return 17;
        for (unsigned y = 100; y < 104; ++y) {
            for (unsigned x = 100; x < 104; ++x) {
                if (video.vram[y][x] != 1) return 17;
            }
        }
        if (video.vram[104][103] != 0 || video.vram[103][104] != 0) return 17;
    }

    /* --- GP0(20): untextured flat triangle (POLY_F3) ---
     * Right-angle triangle: (80, 80), (85, 80), (80, 85) in blue. */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t pkt[4] = {
            cmd_rgb(0x20, 0, 0, 0xF8),
            xy_word(80, 80),
            xy_word(85, 80),
            xy_word(80, 85),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 4)) return 14;
        if (video.vram[80][80] != 0x7C00u) return 14;
        if (video.vram[81][81] != 0x7C00u) return 14;
        if (video.vram[85][85] != 0)       return 14;
    }

    /* --- GP0(28): untextured flat quad (POLY_F4) ---
     * Quad: (50, 50) to (54, 54) in green (0x00F800 = 0x03E0u). */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t pkt[5] = {
            cmd_rgb(0x28, 0, 0xF8, 0),
            xy_word(50, 50),
            xy_word(54, 50),
            xy_word(50, 54),
            xy_word(54, 54),
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 5)) return 15;
        if (video.vram[50][50] != 0x03E0u) return 15;
        if (video.vram[53][53] != 0x03E0u) return 15;
        if (video.vram[50][54] != 0)       return 15;
        if (video.vram[54][50] != 0)       return 15;
        if (video.vram[54][54] != 0)       return 15;
        if (video.vram[49][50] != 0)       return 15;
        if (video.vram[55][54] != 0)       return 15;
    }

    /* --- GP0(2C): textured quad (POLY_FT4) ---
     * 4-bit CLUT texture from VRAM (page 1, CLUT at row 2 col 0).
     * Texels at row 0 col 64: 0x0210 (nibbles 0=trans, 1=red, 2=green, 0=trans). */
    {
        ps1_draw_state_reset(&draw);
        video.vram[70][70] = 0xFFFFu;
        video.vram[70][71] = 0xFFFFu;
        video.vram[70][72] = 0xFFFFu;
        video.vram[70][73] = 0xFFFFu;

        unsigned clut_field = (2u << 6) | 0u;
        const uint32_t pkt[9] = {
            cmd_rgb(0x2C, 0x80, 0x80, 0x80),                  /* neutral modulation */
            xy_word(70, 70),                                  /* v0 */
            (uint32_t)(0 | (0 << 8) | (clut_field << 16)),    /* u0=0, v0=0, clut */
            xy_word(74, 70),                                  /* v1 */
            (uint32_t)(4 | (0 << 8) | (1u << 16)),            /* u1=4, v1=0, tpage=1 */
            xy_word(70, 71),                                  /* v2 */
            (uint32_t)(0 | (1 << 8)),                         /* u2=0, v2=1 */
            xy_word(74, 71),                                  /* v3 */
            (uint32_t)(4 | (1 << 8)),                         /* u3=4, v3=1 */
        };
        if (!ps1_gpu_exec(&video, &draw, pkt, 9)) return 16;
        /* Index 0: transparent -> 0xFFFF preserved */
        if (video.vram[70][70] != 0xFFFFu) return 16;
        /* Index 1: red 0x001F */
        if (video.vram[70][71] != 0x001Fu) return 16;
        /* Index 2: green 0x03E0 */
        if (video.vram[70][72] != 0x03E0u) return 16;
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

    /* --- ps1_gpu_submit_ot: ordering-table walk to GPU dispatch ---
     * Packets in buffer:
     *   idx 0: tag len=2 | GP0(68) red 1x1 at (10, 10)
     *   idx 3: tag len=2 | GP0(68) green 1x1 at (10, 10)
     *   idx 6: tag len=2 | GP0(68) blue 1x1 at (10, 10)
     *
     * In PS1 OT, higher bucket = farther distance = drawn earlier.
     * Lower bucket = closer distance = drawn later (on top). */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t buffer[9] = {
            /* Packet 0: red pixel */
            (2u << 24), cmd_rgb(0x68, 0xF8, 0, 0), xy_word(10, 10),
            /* Packet 1 (idx 3): green pixel */
            (2u << 24), cmd_rgb(0x68, 0, 0xF8, 0), xy_word(10, 10),
            /* Packet 2 (idx 6): blue pixel */
            (2u << 24), cmd_rgb(0x68, 0, 0, 0xF8), xy_word(10, 10),
        };

        Ps1Ot ot = {0};
        if (!ps1_ot_init(&ot, 4, 8)) return 8;

        /* Far (bucket 3) = red, mid (bucket 1) = green, near (bucket 0) = blue. */
        if (!ps1_ot_insert(&ot, 3, 0)
            || !ps1_ot_insert(&ot, 1, 3)
            || !ps1_ot_insert(&ot, 0, 6)) {
            ps1_ot_destroy(&ot);
            return 8;
        }

        video.vram[10][10] = 0;
        if (!ps1_gpu_submit_ot(&video, &draw, &ot, buffer, 9)) {
            ps1_ot_destroy(&ot);
            return 8;
        }
        /* Blue (bucket 0) was drawn last, overwriting red and green. */
        if (video.vram[10][10] != 0x7C00u) {
            ps1_ot_destroy(&ot);
            return 8;
        }

        /* Intra-bucket order: newest packet is visited first in ps1_ot_walk.
         * Inserting packet 0 (red) then packet 3 (green) into bucket 2 means
         * packet 3 draws first, then packet 0 draws over it. */
        ps1_ot_clear(&ot);
        video.vram[10][10] = 0;
        if (!ps1_ot_insert(&ot, 2, 0)
            || !ps1_ot_insert(&ot, 2, 3)
            || !ps1_gpu_submit_ot(&video, &draw, &ot, buffer, 9)) {
            ps1_ot_destroy(&ot);
            return 9;
        }
        /* Packet 0 (red) was visited second, so red remains. */
        if (video.vram[10][10] != 0x001Fu) {
            ps1_ot_destroy(&ot);
            return 9;
        }

        /* Out-of-bounds packet in buffer must fail cleanly. */
        ps1_ot_clear(&ot);
        /* packet_id 6 requires 3 words (tag + 2 cmd words, indices 6..8);
         * buffer size 7 cannot contain it. */
        if (!ps1_ot_insert(&ot, 0, 6)
            || ps1_gpu_submit_ot(&video, &draw, &ot, buffer, 7)) {
            ps1_ot_destroy(&ot);
            return 10;
        }
        /* packet_id equal to or greater than buffer size must fail. */
        ps1_ot_clear(&ot);
        if (!ps1_ot_insert(&ot, 0, 9)
            || ps1_gpu_submit_ot(&video, &draw, &ot, buffer, 9)) {
            ps1_ot_destroy(&ot);
            return 10;
        }

        /* NULL guards. */
        if (ps1_gpu_submit_ot(NULL, &draw, &ot, buffer, 9)
            || ps1_gpu_submit_ot(&video, NULL, &ot, buffer, 9)
            || ps1_gpu_submit_ot(&video, &draw, NULL, buffer, 9)
            || ps1_gpu_submit_ot(&video, &draw, &ot, NULL, 9)) {
            ps1_ot_destroy(&ot);
            return 11;
        }

        ps1_ot_destroy(&ot);
    }

    /* --- ps1_gpu_submit_frame: multi-table frame submission ---
     * Submits an array of OTs in sequence (e.g., background OT then foreground OT),
     * ignoring NULL entries (optional tables). */
    {
        ps1_draw_state_reset(&draw);
        const uint32_t buffer[6] = {
            (2u << 24), cmd_rgb(0x68, 0xF8, 0, 0), xy_word(25, 25), /* red */
            (2u << 24), cmd_rgb(0x68, 0, 0xF8, 0), xy_word(25, 25), /* green */
        };

        Ps1Ot ot_bg = {0};
        Ps1Ot ot_fg = {0};
        if (!ps1_ot_init(&ot_bg, 2, 2) || !ps1_ot_init(&ot_fg, 2, 2)) return 12;

        if (!ps1_ot_insert(&ot_bg, 0, 0)   /* red at idx 0 */
            || !ps1_ot_insert(&ot_fg, 0, 3)) { /* green at idx 3 */
            ps1_ot_destroy(&ot_bg);
            ps1_ot_destroy(&ot_fg);
            return 12;
        }

        const Ps1Ot *frame_ots_list[3] = { &ot_bg, NULL, &ot_fg };
        video.vram[25][25] = 0;
        if (!ps1_gpu_submit_frame(&video, &draw, frame_ots_list, 3, buffer, 6)) {
            ps1_ot_destroy(&ot_bg);
            ps1_ot_destroy(&ot_fg);
            return 12;
        }
        /* Foreground table ran after background, leaving green. */
        if (video.vram[25][25] != 0x03E0u) {
            ps1_ot_destroy(&ot_bg);
            ps1_ot_destroy(&ot_fg);
            return 12;
        }

        /* NULL frame checks. */
        if (ps1_gpu_submit_frame(NULL, &draw, frame_ots_list, 3, buffer, 6)
            || ps1_gpu_submit_frame(&video, NULL, frame_ots_list, 3, buffer, 6)
            || ps1_gpu_submit_frame(&video, &draw, NULL, 1, buffer, 6)
            || ps1_gpu_submit_frame(&video, &draw, frame_ots_list, 3, NULL, 6)) {
            ps1_ot_destroy(&ot_bg);
            ps1_ot_destroy(&ot_fg);
            return 13;
        }

        ps1_ot_destroy(&ot_bg);
        ps1_ot_destroy(&ot_fg);
    }

    return 0;
}
