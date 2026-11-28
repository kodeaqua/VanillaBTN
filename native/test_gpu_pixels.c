#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include <stdio.h>
#include <string.h>

enum {
    DRAW_SIZE = 256,
    TILE_SIZE = 4,
    TILE_PIXELS = TILE_SIZE * TILE_SIZE,
    TILE_ORIGIN = 32,
    COLOR_MAX = 31,
    COLOR_MASK = 0x7fff,
    RGB888_SHIFT = 3,
    INDEXED_TEXTURE = 0,
    DIRECT_COLOR_TEXTURE = 2,
};

static unsigned rgb555(unsigned r, unsigned g, unsigned b) { return r | (g << 5) | (b << 10); }

static void select_draw_mode(int tpage, int dither) {
    DR_MODE mode;
    SetDrawMode(&mode, 1, dither, tpage, NULL);
    termPrim(&mode);
    DrawOTag((OT_TYPE *)&mode);
}

static void draw_tile(int x, int y, unsigned r, unsigned g, unsigned b, int semi_transparent) {
    POLY_G4 quad = {0};
    SetPolyG4(&quad);
    setXY4(&quad, x, y, x + TILE_SIZE, y, x, y + TILE_SIZE, x + TILE_SIZE, y + TILE_SIZE);
    setRGB0(&quad, r, g, b);
    setRGB1(&quad, r, g, b);
    setRGB2(&quad, r, g, b);
    setRGB3(&quad, r, g, b);
    setSemiTrans(&quad, semi_transparent);
    termPrim(&quad);
    DrawOTag((OT_TYPE *)&quad);
}

static void read_tile(int x, int y, u16 pixels[TILE_PIXELS]) {
    RECT rect = {x, y, TILE_SIZE, TILE_SIZE};
    u_long transfer[TILE_PIXELS * sizeof(u16) / sizeof(u_long)] = {0};
    StoreImage(&rect, transfer);
    DrawSync(0);
    memcpy(pixels, transfer, sizeof(transfer));
}

static int check_dither(int x, int y, int enabled) {
    static const int offsets[TILE_SIZE][TILE_SIZE] = {
        {-4, 0, -3, 1},
        {2, -2, 3, -1},
        {-3, 1, -4, 0},
        {3, -1, 2, -2},
    };
    enum { RED = 64, GREEN = 65, BLUE = 66 };
    u16 pixels[TILE_PIXELS];
    select_draw_mode(0, enabled);
    draw_tile(x, y, RED, GREEN, BLUE, 0);
    read_tile(x, y, pixels);
    for (int row = 0; row < TILE_SIZE; ++row) {
        for (int col = 0; col < TILE_SIZE; ++col) {
            int delta = enabled ? offsets[(y + row) % TILE_SIZE][(x + col) % TILE_SIZE] : 0;
            unsigned expected =
                rgb555((RED + delta) >> RGB888_SHIFT, (GREEN + delta) >> RGB888_SHIFT,
                       (BLUE + delta) >> RGB888_SHIFT);
            unsigned actual = pixels[row * TILE_SIZE + col] & COLOR_MASK;
            if (actual != expected) {
                fprintf(stderr, "dither %d at (%d,%d): got %04x, expected %04x\n", enabled, x + col,
                        y + row, actual, expected);
                return 1;
            }
        }
    }
    return 0;
}

static int check_blend(int blend, unsigned expected) {
    u16 pixels[TILE_PIXELS];
    select_draw_mode(getTPage(0, blend, 0, 0), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 5 << RGB888_SHIFT, 19 << RGB888_SHIFT, 28 << RGB888_SHIFT,
              0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 7 << RGB888_SHIFT, 11 << RGB888_SHIFT, 9 << RGB888_SHIFT,
              1);
    read_tile(TILE_ORIGIN, TILE_ORIGIN, pixels);
    for (unsigned i = 0; i < TILE_PIXELS; ++i) {
        unsigned actual = pixels[i] & COLOR_MASK;
        if (actual != expected) {
            fprintf(stderr, "blend %d at pixel %u: got %04x, expected %04x\n", blend, i, actual,
                    expected);
            return 1;
        }
    }
    return 0;
}

static void sample_texture(int x, int clut) {
    SPRT sprite = {0};
    SetSprt(&sprite);
    setXY0(&sprite, x, TILE_ORIGIN);
    setWH(&sprite, TILE_SIZE, TILE_SIZE);
    setUV0(&sprite, 0, 0);
    setShadeTex(&sprite, 1);
    sprite.clut = clut;
    termPrim(&sprite);
    DrawOTag((OT_TYPE *)&sprite);
}

static int check_texture(int x, unsigned expected, const char *source) {
    u16 pixels[TILE_PIXELS];
    read_tile(x, TILE_ORIGIN, pixels);
    for (unsigned i = 0; i < TILE_PIXELS; ++i) {
        unsigned actual = pixels[i] & COLOR_MASK;
        if (actual != expected) {
            fprintf(stderr, "%s at pixel %u: got %04x, expected %04x\n", source, i, actual,
                    expected);
            return 1;
        }
    }
    return 0;
}

static int check_copy_visibility(void) {
    enum { COPY_X = 64, COPY_Y = 256, SAMPLE_X = TILE_ORIGIN + TILE_SIZE };
    RECT source = {TILE_ORIGIN, TILE_ORIGIN, TILE_SIZE, TILE_SIZE};
    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, COLOR_MAX << RGB888_SHIFT, 0, 0, 0);
    MoveImage(&source, COPY_X, COPY_Y);
    select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, 0, COPY_X, COPY_Y), 0);
    sample_texture(SAMPLE_X, 0);
    return check_texture(SAMPLE_X, rgb555(COLOR_MAX, 0, 0), "same-frame copy");
}

static int check_clut_visibility(void) {
    enum {
        TEXTURE_X = 128,
        TEXTURE_Y = 256,
        CLUT_X = 512,
        CLUT_COLORS = 16,
        CLUT_INDEX = 1,
        INDEXED_TEXELS = 0x1111,
    };
    RECT texture = {TEXTURE_X, TEXTURE_Y, 1, TILE_SIZE};
    RECT clut = {CLUT_X, TEXTURE_Y, CLUT_COLORS, 1};
    const u16 indices[TILE_SIZE] = {INDEXED_TEXELS, INDEXED_TEXELS, INDEXED_TEXELS, INDEXED_TEXELS};
    u_long texture_data[TILE_SIZE * sizeof(u16) / sizeof(u_long)];
    u16 palette[CLUT_COLORS] = {[CLUT_INDEX] = COLOR_MAX};
    u_long palette_data[CLUT_COLORS * sizeof(u16) / sizeof(u_long)];
    memcpy(texture_data, indices, sizeof(indices));
    memcpy(palette_data, palette, sizeof(palette));
    LoadImage(&texture, texture_data);
    LoadImage(&clut, palette_data);
    select_draw_mode(getTPage(INDEXED_TEXTURE, 0, TEXTURE_X, TEXTURE_Y), 0);
    sample_texture(TILE_ORIGIN, getClut(CLUT_X, TEXTURE_Y));

    const unsigned new_color = rgb555(0, COLOR_MAX, 0);
    palette[CLUT_INDEX] = (u16)new_color;
    memcpy(palette_data, palette, sizeof(palette));
    LoadImage(&clut, palette_data);
    sample_texture(TILE_ORIGIN + TILE_SIZE, getClut(CLUT_X, TEXTURE_Y));
    int failures = check_texture(TILE_ORIGIN, COLOR_MAX, "original CLUT");
    failures += check_texture(TILE_ORIGIN + TILE_SIZE, new_color, "updated CLUT");
    return failures;
}

int main(void) {
    DRAWENV env;
    ResetGraph(0);
    VSync(0);
    SetDefDrawEnv(&env, 0, 0, DRAW_SIZE, DRAW_SIZE);
    PutDrawEnv(&env);

    int failures = check_dither(TILE_ORIGIN, TILE_ORIGIN, 0);
    failures += check_dither(TILE_ORIGIN, TILE_ORIGIN, 1);
    failures += check_dither(TILE_ORIGIN + 1, TILE_ORIGIN + 2, 1);
    failures += check_blend(0, rgb555(6, 15, 18));
    failures += check_blend(1, rgb555(12, 30, COLOR_MAX));
    failures += check_blend(2, rgb555(0, 8, 19));
    failures += check_blend(3, rgb555(6, 21, 30));
    failures += check_copy_visibility();
    failures += check_clut_visibility();
    return failures != 0;
}
