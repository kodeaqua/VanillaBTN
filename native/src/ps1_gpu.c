#include "ps1_gpu.h"

#include <stdint.h>

/*
 * GP0 command codes used by the game's primitive builders in prim.c.
 * Bit 1 of the code byte is the semi-transparency flag for most draw commands.
 *
 * References: PSX-SPX GPU specification
 * https://psx-spx.consoledev.net/graphicsprocessingunitgpu/
 */

/* Quick fill — ignores draw state and mask settings. */
#define GP0_FILL            0x02u

/* Untextured monochrome rectangles (variable size, pixel, 8x8, 16x16). */
#define GP0_TILE_VAR        0x60u
#define GP0_TILE_VAR_ST     0x62u
#define GP0_TILE_1          0x68u
#define GP0_TILE_1_ST       0x6Au
#define GP0_TILE_8          0x70u
#define GP0_TILE_8_ST       0x72u
#define GP0_TILE_16         0x78u
#define GP0_TILE_16_ST      0x7Au

/* Textured sprites (variable size, 8x8, 16x16); bit 0 = raw texture. */
#define GP0_SPRITE_VAR      0x64u
#define GP0_SPRITE_VAR_RAW  0x65u
#define GP0_SPRITE_VAR_ST   0x66u
#define GP0_SPRITE_VAR_RST  0x67u
#define GP0_SPRITE_16       0x7Cu
#define GP0_SPRITE_16_RAW   0x7Du
#define GP0_SPRITE_16_ST    0x7Eu
#define GP0_SPRITE_16_RST   0x7Fu
#define GP0_SPRITE_8        0x74u
#define GP0_SPRITE_8_RAW    0x75u
#define GP0_SPRITE_8_ST     0x76u
#define GP0_SPRITE_8_RST    0x77u

/* Polygons (3-point and 4-point, flat and textured). */
#define GP0_POLY_F3         0x20u
#define GP0_POLY_F3_ST      0x22u
#define GP0_POLY_FT3        0x24u
#define GP0_POLY_FT3_RAW    0x25u
#define GP0_POLY_FT3_ST     0x26u
#define GP0_POLY_FT3_RST    0x27u
#define GP0_POLY_F4         0x28u
#define GP0_POLY_F4_ST      0x2Au
#define GP0_POLY_FT4        0x2Cu
#define GP0_POLY_FT4_RAW    0x2Du
#define GP0_POLY_FT4_ST     0x2Eu
#define GP0_POLY_FT4_RST    0x2Fu

/* Draw environment. */
#define GP0_IMAGE_LOAD      0xA0u  /* Inline CPU-to-VRAM rectangle transfer */
#define GP0_DRAW_MODE       0xE1u  /* Texture page, semi-transparency mode */
#define GP0_DRAW_AREA_TL    0xE3u  /* Top-left corner of draw area */
#define GP0_DRAW_AREA_BR    0xE4u  /* Bottom-right corner of draw area */
#define GP0_DRAW_OFFSET     0xE5u  /* Draw offset (signed 11-bit x/y) */
#define GP0_MASK_SETTING    0xE6u  /* Force/check mask bits */

/* Semi-transparency bit shared by tile and sprite codes. */
#define CODE_SEMI_TRANS     0x02u
/* Raw-texture bit for sprite codes. */
#define CODE_RAW_TEXTURE    0x01u

/* Extract GP0 command byte from first packet word. */
static unsigned gp0_code(uint32_t word) { return (word >> 24) & 0xFFu; }

/* GP0 stores R in bits 0..7 and B in bits 16..23. The native drawing API
 * deliberately takes 0xRRGGBB, so convert at this packet boundary. */
static uint32_t gp0_rgb24(uint32_t word) {
    uint32_t r = word & 0xFFu;
    uint32_t g = (word >>  8) & 0xFFu;
    uint32_t b = (word >> 16) & 0xFFu;
    return (r << 16) | (g << 8) | b;
}

/* Extract signed 16-bit X from packed XY word (bits 15-0). */
static int gp0_x(uint32_t xy) { return (int)(int16_t)(xy & 0xFFFFu); }

/* Extract signed 16-bit Y from packed XY word (bits 31-16). */
static int gp0_y(uint32_t xy) { return (int)(int16_t)((xy >> 16) & 0xFFFFu); }

/* Dispatch a quick-fill packet (GP0 0x02).
 * Packet layout: [0] cmd+rgb, [1] x/y top-left, [2] width/height. */
static void exec_fill(Ps1Video *video, const uint32_t *words, size_t count) {
    if (count < 3) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    int x      = gp0_x(words[1]);
    int y      = gp0_y(words[1]);
    int width  = gp0_x(words[2]);
    int height = gp0_y(words[2]);
    ps1_video_quick_fill(video, x, y, width, height, rgb24);
}

/* Dispatch an untextured tile or pixel packet.
 * Variable-size layout: [0] cmd+rgb, [1] x/y, [2] w/h (1x1 uses 0x00010001).
 * Fixed-size (8x8 or 16x16): [0] cmd+rgb, [1] x/y (w/h implicit). */
static void exec_tile(Ps1Video *video, const Ps1DrawState *state,
                      const uint32_t *words, size_t count,
                      int fixed_w, int fixed_h, bool semi_transparent) {
    if (count < 2) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    int x = gp0_x(words[1]);
    int y = gp0_y(words[1]);
    int w, h;

    if (fixed_w > 0) {
        w = fixed_w;
        h = fixed_h;
    } else {
        if (count < 3) return;
        w = gp0_x(words[2]);
        h = gp0_y(words[2]);
        /* 1x1 tile code 0x68/0x6A: the size word is absent; use 1x1. */
    }
    ps1_video_draw_tile(video, state, x, y, w, h, rgb24, semi_transparent);
}

/* Dispatch a textured sprite packet.
 * Variable-size layout: [0] cmd+rgb, [1] x/y, [2] u/v+clut, [3] w/h.
 * Fixed-size layout:    [0] cmd+rgb, [1] x/y, [2] u/v+clut (w/h implicit). */
static void exec_sprite(Ps1Video *video, const Ps1DrawState *state,
                        const uint32_t *words, size_t count,
                        int fixed_w, int fixed_h,
                        bool raw_texture, bool semi_transparent) {
    if (count < 3) return;
    Ps1Sprite sprite;
    sprite.rgb24           = gp0_rgb24(words[0]);
    sprite.x               = gp0_x(words[1]);
    sprite.y               = gp0_y(words[1]);
    sprite.u               = (words[2]      ) & 0xFFu;
    sprite.v               = (words[2] >>  8) & 0xFFu;
    sprite.clut            = (words[2] >> 16) & 0xFFFFu;
    sprite.raw_texture     = raw_texture;
    sprite.semi_transparent = semi_transparent;

    /* The tpage is taken from the draw state's current texture page setting.
     * GP0(E1) updates state->tpage_word (stored in blend_mode + offset fields).
     * We recover it from the draw state's tpage field that we keep. */
    sprite.tpage = state->tpage;

    if (fixed_w > 0) {
        sprite.width  = (unsigned)fixed_w;
        sprite.height = (unsigned)fixed_h;
    } else {
        if (count < 4) return;
        sprite.width  = (unsigned)gp0_x(words[3]);
        sprite.height = (unsigned)gp0_y(words[3]);
    }
    ps1_video_draw_sprite(video, state, &sprite);
}

/* Dispatch an untextured 3-point polygon.
 * Layout: [0] cmd+rgb, [1] x0/y0, [2] x1/y1, [3] x2/y2. */
static void exec_poly_f3(Ps1Video *video, const Ps1DrawState *state,
                         const uint32_t *words, size_t count,
                         bool semi_transparent) {
    if (count < 4) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    Ps1Vertex v0 = { gp0_x(words[1]), gp0_y(words[1]), 0, 0 };
    Ps1Vertex v1 = { gp0_x(words[2]), gp0_y(words[2]), 0, 0 };
    Ps1Vertex v2 = { gp0_x(words[3]), gp0_y(words[3]), 0, 0 };
    ps1_video_draw_triangle(video, state, &v0, &v1, &v2,
                            rgb24, 0, 0, false, false, semi_transparent);
}

/* Dispatch a textured 3-point polygon.
 * Layout: [0] cmd+rgb, [1] x0/y0, [2] u0/v0+clut, [3] x1/y1,
 *         [4] u1/v1+tpage, [5] x2/y2, [6] u2/v2. */
static void exec_poly_ft3(Ps1Video *video, Ps1DrawState *state,
                          const uint32_t *words, size_t count,
                          bool raw_texture, bool semi_transparent) {
    if (count < 7) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    Ps1Vertex v0 = { gp0_x(words[1]), gp0_y(words[1]), words[2] & 0xFFu, (words[2] >> 8) & 0xFFu };
    unsigned clut = (words[2] >> 16) & 0xFFFFu;
    Ps1Vertex v1 = { gp0_x(words[3]), gp0_y(words[3]), words[4] & 0xFFu, (words[4] >> 8) & 0xFFu };
    unsigned tpage_word = (words[4] >> 16) & 0xFFFFu;
    Ps1Vertex v2 = { gp0_x(words[5]), gp0_y(words[5]), words[6] & 0xFFu, (words[6] >> 8) & 0xFFu };

    state->tpage = tpage_word & 0x1FFu;
    state->blend_mode = (tpage_word >> 5) & 3u;

    ps1_video_draw_triangle(video, state, &v0, &v1, &v2,
                            rgb24, state->tpage, clut,
                            true, raw_texture, semi_transparent);
}

/* Dispatch an untextured 4-point polygon.
 * Layout: [0] cmd+rgb, [1] x0/y0, [2] x1/y1, [3] x2/y2, [4] x3/y3. */
static void exec_poly_f4(Ps1Video *video, const Ps1DrawState *state,
                         const uint32_t *words, size_t count,
                         bool semi_transparent) {
    if (count < 5) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    Ps1Vertex v0 = { gp0_x(words[1]), gp0_y(words[1]), 0, 0 };
    Ps1Vertex v1 = { gp0_x(words[2]), gp0_y(words[2]), 0, 0 };
    Ps1Vertex v2 = { gp0_x(words[3]), gp0_y(words[3]), 0, 0 };
    Ps1Vertex v3 = { gp0_x(words[4]), gp0_y(words[4]), 0, 0 };
    ps1_video_draw_quad(video, state, &v0, &v1, &v2, &v3,
                        rgb24, 0, 0, false, false, semi_transparent);
}

/* Dispatch a textured 4-point polygon (POLY_FT4).
 * Layout: [0] cmd+rgb, [1] x0/y0, [2] u0/v0+clut, [3] x1/y1,
 *         [4] u1/v1+tpage, [5] x2/y2, [6] u2/v2, [7] x3/y3, [8] u3/v3. */
static void exec_poly_ft4(Ps1Video *video, Ps1DrawState *state,
                          const uint32_t *words, size_t count,
                          bool raw_texture, bool semi_transparent) {
    if (count < 9) return;
    uint32_t rgb24 = gp0_rgb24(words[0]);
    Ps1Vertex v0 = { gp0_x(words[1]), gp0_y(words[1]), words[2] & 0xFFu, (words[2] >> 8) & 0xFFu };
    unsigned clut = (words[2] >> 16) & 0xFFFFu;
    Ps1Vertex v1 = { gp0_x(words[3]), gp0_y(words[3]), words[4] & 0xFFu, (words[4] >> 8) & 0xFFu };
    unsigned tpage_word = (words[4] >> 16) & 0xFFFFu;
    Ps1Vertex v2 = { gp0_x(words[5]), gp0_y(words[5]), words[6] & 0xFFu, (words[6] >> 8) & 0xFFu };
    Ps1Vertex v3 = { gp0_x(words[7]), gp0_y(words[7]), words[8] & 0xFFu, (words[8] >> 8) & 0xFFu };

    state->tpage = tpage_word & 0x1FFu;
    state->blend_mode = (tpage_word >> 5) & 3u;

    ps1_video_draw_quad(video, state, &v0, &v1, &v2, &v3,
                        rgb24, state->tpage, clut,
                        true, raw_texture, semi_transparent);
}

/* Update draw state from GP0(E1): draw mode / texture page.
 * Bits 0-3: texture page X base (/64); bit 4: Y base (/256);
 * bits 5-6: semi-transparency mode; bits 7-8: texture depth;
 * bit 9: dithering; bit 10: draw to display area. */
static void exec_draw_mode(Ps1DrawState *state, uint32_t word) {
    /* Reconstruct the tpage field that ps1_video_sample_texel expects:
     * bits [3:0] = x64 base, bit [4] = y256 base, bits [8:7] = depth. */
    state->tpage = (unsigned)(word & 0x1FFu);  /* bits 0-8 */
    /* Semi-transparency mode lives in bits 5-6 of the GP0(E1) parameter. */
    state->blend_mode = (unsigned)((word >> 5) & 3u);
}

/* Update draw state from GP0(E3): set draw area top-left.
 * Bits 9-0: X, bits 18-10: Y (all in absolute VRAM coordinates). */
static void exec_draw_area_tl(Ps1DrawState *state, uint32_t word) {
    state->left = (int)(word & 0x3FFu);
    state->top  = (int)((word >> 10) & 0x1FFu);
}

/* Update draw state from GP0(E4): set draw area bottom-right. */
static void exec_draw_area_br(Ps1DrawState *state, uint32_t word) {
    state->right  = (int)(word & 0x3FFu);
    state->bottom = (int)((word >> 10) & 0x1FFu);
}

/* Update draw state from GP0(E5): set draw offset.
 * Bits 10-0: signed X offset; bits 21-11: signed Y offset. */
static void exec_draw_offset(Ps1DrawState *state, uint32_t word) {
    int x = (int)(word & 0x7FFu);
    if (x >= 0x400) x -= 0x800;
    int y = (int)((word >> 11) & 0x7FFu);
    if (y >= 0x400) y -= 0x800;
    state->offset_x = x;
    state->offset_y = y;
}

/* Update draw state from GP0(E6): mask bit setting.
 * Bit 0: force mask bit on drawn pixels; bit 1: check mask before drawing. */
static void exec_mask_setting(Ps1DrawState *state, uint32_t word) {
    state->force_mask  = (word & 1u) != 0;
    state->check_mask  = (word & 2u) != 0;
}

bool ps1_gpu_exec(Ps1Video *video, Ps1DrawState *state,
                  const uint32_t *words, size_t count) {
    if (video == NULL || state == NULL || words == NULL || count == 0) {
        return false;
    }

    unsigned code = gp0_code(words[0]);

    switch (code) {
    case 0x40: case 0x42: /* Flat line: color, XY0, XY1. */
        if (count >= 3)
            return ps1_video_draw_line(video, state, gp0_x(words[1]), gp0_y(words[1]),
                gp0_x(words[2]), gp0_y(words[2]), gp0_rgb24(words[0]), (code & 2) != 0);
        break;
    case 0x30: case 0x32: case 0x38: case 0x3A: {
        unsigned vertices = (code & 8) ? 4 : 3;
        if (count < vertices * 2) break;
        Ps1Vertex points[4];
        uint32_t colors[4];
        for (unsigned i = 0; i < vertices; ++i) {
            points[i] = (Ps1Vertex){gp0_x(words[i * 2 + 1]), gp0_y(words[i * 2 + 1]), 0, 0};
            colors[i] = gp0_rgb24(words[i * 2]);
        }
        if (!ps1_video_draw_gouraud_triangle(video, state, points, points + 1, points + 2,
                                             colors, (code & 2) != 0)) return false;
        if (vertices == 4)
            return ps1_video_draw_gouraud_triangle(video, state, points + 1, points + 2, points + 3,
                                                   colors + 1, (code & 2) != 0);
        break;
    }
    case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x3C: case 0x3D: case 0x3E: case 0x3F: {
        /* POLY_GT3 / POLY_GT4: textured Gouraud polygon.
         * Layout: v_i has [3*i] rgb, [3*i+1] xy, [3*i+2] uv(+clut/tpage). */
        unsigned vertices = (code & 8) ? 4 : 3;
        if (count < vertices * 3) break;
        Ps1Vertex points[4];
        for (unsigned i = 0; i < vertices; ++i) {
            points[i] = (Ps1Vertex){
                gp0_x(words[i * 3 + 1]),
                gp0_y(words[i * 3 + 1]),
                words[i * 3 + 2] & 0xFFu,
                (words[i * 3 + 2] >> 8) & 0xFFu
            };
        }
        unsigned clut = (words[2] >> 16) & 0xFFFFu;
        unsigned tpage_word = (words[5] >> 16) & 0xFFFFu;
        state->tpage = tpage_word & 0x1FFu;
        state->blend_mode = (tpage_word >> 5) & 3u;
        uint32_t rgb24 = gp0_rgb24(words[0]);
        bool raw_tex = (code & 1) != 0;
        bool semi_trans = (code & 2) != 0;
        if (!ps1_video_draw_triangle(video, state, &points[0], &points[1], &points[2],
                                     rgb24, state->tpage, clut,
                                     true, raw_tex, semi_trans)) return false;
        if (vertices == 4) {
            return ps1_video_draw_triangle(video, state, &points[1], &points[2], &points[3],
                                           rgb24, state->tpage, clut,
                                           true, raw_tex, semi_trans);
        }
        break;
    }
    case GP0_FILL:
        exec_fill(video, words, count);
        break;

    /* Untextured variable-size tile. */
    case GP0_TILE_VAR:
        exec_tile(video, state, words, count, 0, 0, false);
        break;
    case GP0_TILE_VAR_ST:
        exec_tile(video, state, words, count, 0, 0, true);
        break;

    /* Untextured single-pixel tile. */
    case GP0_TILE_1:
        exec_tile(video, state, words, count, 1, 1, false);
        break;
    case GP0_TILE_1_ST:
        exec_tile(video, state, words, count, 1, 1, true);
        break;

    /* Untextured 8x8 tile. */
    case GP0_TILE_8:
        exec_tile(video, state, words, count, 8, 8, false);
        break;
    case GP0_TILE_8_ST:
        exec_tile(video, state, words, count, 8, 8, true);
        break;

    /* Untextured 16x16 tile. */
    case GP0_TILE_16:
        exec_tile(video, state, words, count, 16, 16, false);
        break;
    case GP0_TILE_16_ST:
        exec_tile(video, state, words, count, 16, 16, true);
        break;

    /* Textured sprites (variable size). */
    case GP0_SPRITE_VAR:
        exec_sprite(video, state, words, count, 0, 0, false, false);
        break;
    case GP0_SPRITE_VAR_RAW:
        exec_sprite(video, state, words, count, 0, 0, true, false);
        break;
    case GP0_SPRITE_VAR_ST:
        exec_sprite(video, state, words, count, 0, 0, false, true);
        break;
    case GP0_SPRITE_VAR_RST:
        exec_sprite(video, state, words, count, 0, 0, true, true);
        break;

    /* Textured sprites (8x8). */
    case GP0_SPRITE_8:
        exec_sprite(video, state, words, count, 8, 8, false, false);
        break;
    case GP0_SPRITE_8_RAW:
        exec_sprite(video, state, words, count, 8, 8, true, false);
        break;
    case GP0_SPRITE_8_ST:
        exec_sprite(video, state, words, count, 8, 8, false, true);
        break;
    case GP0_SPRITE_8_RST:
        exec_sprite(video, state, words, count, 8, 8, true, true);
        break;

    /* Textured sprites (16x16). */
    case GP0_SPRITE_16:
        exec_sprite(video, state, words, count, 16, 16, false, false);
        break;
    case GP0_SPRITE_16_RAW:
        exec_sprite(video, state, words, count, 16, 16, true, false);
        break;
    case GP0_SPRITE_16_ST:
        exec_sprite(video, state, words, count, 16, 16, false, true);
        break;
    case GP0_SPRITE_16_RST:
        exec_sprite(video, state, words, count, 16, 16, true, true);
        break;

    /* 3-point polygons (triangles). */
    case GP0_POLY_F3:
        exec_poly_f3(video, state, words, count, false);
        break;
    case GP0_POLY_F3_ST:
        exec_poly_f3(video, state, words, count, true);
        break;
    case GP0_POLY_FT3:
        exec_poly_ft3(video, state, words, count, false, false);
        break;
    case GP0_POLY_FT3_RAW:
        exec_poly_ft3(video, state, words, count, true, false);
        break;
    case GP0_POLY_FT3_ST:
        exec_poly_ft3(video, state, words, count, false, true);
        break;
    case GP0_POLY_FT3_RST:
        exec_poly_ft3(video, state, words, count, true, true);
        break;

    /* 4-point polygons (quads). */
    case GP0_POLY_F4:
        exec_poly_f4(video, state, words, count, false);
        break;
    case GP0_POLY_F4_ST:
        exec_poly_f4(video, state, words, count, true);
        break;
    case GP0_POLY_FT4:
        exec_poly_ft4(video, state, words, count, false, false);
        break;
    case GP0_POLY_FT4_RAW:
        exec_poly_ft4(video, state, words, count, true, false);
        break;
    case GP0_POLY_FT4_ST:
        exec_poly_ft4(video, state, words, count, false, true);
        break;
    case GP0_POLY_FT4_RST:
        exec_poly_ft4(video, state, words, count, true, true);
        break;

    /* Draw environment commands. */
    case GP0_IMAGE_LOAD: {
        if (count < 3) return false;
        int x = gp0_x(words[1]);
        int y = gp0_y(words[1]);
        int w = gp0_x(words[2]);
        int h = gp0_y(words[2]);
        if (x < 0 || y < 0 || w <= 0 || h <= 0
            || w > PS1_VRAM_WIDTH - x || h > PS1_VRAM_HEIGHT - y) {
            return false;
        }
        size_t pixels = (size_t)w * (size_t)h;
        size_t data_words = (pixels + 1u) / 2u;
        if (count < 3u + data_words) return false;
        for (size_t i = 0; i < pixels; ++i) {
            uint32_t word = words[3u + (i >> 1)];
            uint16_t texel = (i & 1u) ? (uint16_t)(word >> 16) : (uint16_t)(word & 0xFFFFu);
            video->vram[y + (int)(i / (size_t)w)][x + (int)(i % (size_t)w)] = texel;
        }
        break;
    }
    case GP0_DRAW_MODE:
    case GP0_DRAW_AREA_TL:
    case GP0_DRAW_AREA_BR:
    case GP0_DRAW_OFFSET:
    case GP0_MASK_SETTING:
        for (size_t i = 0; i < count; ++i) {
            switch (gp0_code(words[i])) {
            case GP0_DRAW_MODE:    exec_draw_mode(state, words[i]); break;
            case GP0_DRAW_AREA_TL: exec_draw_area_tl(state, words[i]); break;
            case GP0_DRAW_AREA_BR: exec_draw_area_br(state, words[i]); break;
            case GP0_DRAW_OFFSET:  exec_draw_offset(state, words[i]); break;
            case GP0_MASK_SETTING: exec_mask_setting(state, words[i]); break;
            default: break;
            }
        }
        break;

    default:
        /* Unimplemented GP0 codes (polygons, lines, etc.) are silently skipped.
         * The caller may still return true; drawing the rest of the OT continues. */
        break;
    }

    return true;
}

typedef struct OtSubmitContext {
    Ps1Video *video;
    Ps1DrawState *state;
    const uint32_t *buffer;
    size_t buffer_words;
} OtSubmitContext;

static bool submit_ot_node(uint32_t packet_id, void *context) {
    OtSubmitContext *ctx = (OtSubmitContext *)context;
    if ((size_t)packet_id >= ctx->buffer_words) {
        return false;
    }
    uint32_t tag = ctx->buffer[packet_id];
    size_t len = (tag >> 24) & 0xFFu;
    if (len == 0) {
        return true;
    }
    if (len > ctx->buffer_words - 1 - (size_t)packet_id) {
        return false;
    }
    return ps1_gpu_exec(ctx->video, ctx->state, &ctx->buffer[packet_id + 1], len);
}

bool ps1_gpu_submit_ot(Ps1Video *video, Ps1DrawState *state,
                       const Ps1Ot *ot,
                       const uint32_t *packet_buffer, size_t buffer_words) {
    if (video == NULL || state == NULL || ot == NULL || packet_buffer == NULL) {
        return false;
    }
    OtSubmitContext ctx = {video, state, packet_buffer, buffer_words};
    return ps1_ot_walk(ot, submit_ot_node, &ctx);
}

bool ps1_gpu_submit_frame(Ps1Video *video, Ps1DrawState *state,
                          const Ps1Ot * const *ots, size_t ot_count,
                          const uint32_t *packet_buffer, size_t buffer_words) {
    if (video == NULL || state == NULL || packet_buffer == NULL) {
        return false;
    }
    if (ots == NULL && ot_count > 0) {
        return false;
    }
    for (size_t i = 0; i < ot_count; ++i) {
        if (ots[i] != NULL) {
            if (!ps1_gpu_submit_ot(video, state, ots[i], packet_buffer, buffer_words)) {
                return false;
            }
        }
    }
    return true;
}
