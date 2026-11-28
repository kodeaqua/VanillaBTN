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
    PIXEL_MASK_BIT = 0x8000,
    FORCE_MASK = 1,
    CHECK_MASK = 2,
    RGB888_MAX = 255,
    RGB888_SHIFT = 3,
    INDEXED_TEXTURE = 0,
    INDEXED_8_BIT_TEXTURE = 1,
    DIRECT_COLOR_TEXTURE = 2,
    TEXTURE_PAGE_SIZE = 256,
    VRAM_WIDTH = 1024,
    VRAM_HEIGHT = 512,
    LAST_TEXTURE_PAGE_X = VRAM_WIDTH - 64,
    LAST_CLUT_X = VRAM_WIDTH - 16,
    WORD_BLOCK_SIZE = 2,
};

static unsigned rgb555(unsigned r, unsigned g, unsigned b) { return r | (g << 5) | (b << 10); }

static void select_draw_mode(int tpage, int dither) {
    DR_MODE mode;
    SetDrawMode(&mode, 1, dither, tpage, NULL);
    termPrim(&mode);
    DrawOTag((OT_TYPE *)&mode);
}

static void select_mask(unsigned flags) {
    DR_ENV command = {0};
    setlen(&command, 1);
    command.code[0] = 0xe6000000u | flags;
    termPrim(&command);
    DrawOTag((OT_TYPE *)&command);
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

static int check_dither_pixels(int x, int y, int r, int g, int b, int enabled) {
    static const int offsets[TILE_SIZE][TILE_SIZE] = {
        {-4, 0, -3, 1},
        {2, -2, 3, -1},
        {-3, 1, -4, 0},
        {3, -1, 2, -2},
    };
    u16 pixels[TILE_PIXELS];
    read_tile(x, y, pixels);
    for (int row = 0; row < TILE_SIZE; ++row) {
        for (int col = 0; col < TILE_SIZE; ++col) {
            int delta = enabled ? offsets[(y + row) % TILE_SIZE][(x + col) % TILE_SIZE] : 0;
            unsigned expected = rgb555((r + delta) >> RGB888_SHIFT, (g + delta) >> RGB888_SHIFT,
                                       (b + delta) >> RGB888_SHIFT);
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

static int check_dither(int x, int y, int enabled) {
    enum { RED = 64, GREEN = 65, BLUE = 66 };
    select_draw_mode(0, enabled);
    draw_tile(x, y, RED, GREEN, BLUE, 0);
    return check_dither_pixels(x, y, RED, GREEN, BLUE, enabled);
}

static int check_flat_line(int vertical, int reverse) {
    enum { FIRST_PIXEL = 1, LAST_PIXEL = TILE_SIZE - 1, RED = 8 };
    const unsigned line_pixels[TILE_SIZE] = {0, RED, RED - 1, RED};
    const int start = TILE_ORIGIN + (reverse ? LAST_PIXEL : FIRST_PIXEL);
    const int end = TILE_ORIGIN + (reverse ? FIRST_PIXEL : LAST_PIXEL);
    LINE_F2 line = {0};
    SetLineF2(&line);
    setXY2(&line, vertical ? TILE_ORIGIN : start, vertical ? start : TILE_ORIGIN,
           vertical ? TILE_ORIGIN : end, vertical ? end : TILE_ORIGIN);
    setRGB0(&line, RED << RGB888_SHIFT, 0, 0);
    termPrim(&line);

    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 0);
    select_draw_mode(0, 1);
    DrawOTag((OT_TYPE *)&line);
    u16 pixels[TILE_PIXELS];
    read_tile(TILE_ORIGIN, TILE_ORIGIN, pixels);
    for (int row = 0; row < TILE_SIZE; ++row) {
        for (int col = 0; col < TILE_SIZE; ++col) {
            int along = vertical ? row : col;
            int across = vertical ? col : row;
            unsigned expected = across == 0 ? line_pixels[along] : 0;
            unsigned actual = pixels[row * TILE_SIZE + col] & COLOR_MASK;
            if (actual != expected) {
                fprintf(stderr, "line vertical=%d reverse=%d at (%d,%d): got %04x, expected %04x\n",
                        vertical, reverse, col, row, actual, expected);
                return 1;
            }
        }
    }
    return 0;
}

static int check_point_line(void) {
    enum { POINT_OFFSET = 1, POINT = TILE_ORIGIN + POINT_OFFSET };
    LINE_G2 line = {0};
    SetLineG2(&line);
    setXY2(&line, POINT, POINT, POINT, POINT);
    setRGB0(&line, RGB888_MAX, 0, 0);
    setRGB1(&line, 0, 0, RGB888_MAX);
    termPrim(&line);

    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 0);
    DrawOTag((OT_TYPE *)&line);
    u16 pixels[TILE_PIXELS];
    read_tile(TILE_ORIGIN, TILE_ORIGIN, pixels);
    for (int i = 0; i < TILE_PIXELS; ++i) {
        unsigned expected = i == POINT_OFFSET * (TILE_SIZE + 1) ? COLOR_MAX : 0;
        unsigned actual = pixels[i] & COLOR_MASK;
        if (actual != expected) {
            fprintf(stderr, "single-point Gouraud line at pixel %d: got %04x, expected %04x\n", i,
                    actual, expected);
            return 1;
        }
    }
    return 0;
}

static int check_large_line(int vertical, int oversized) {
    enum { WIDTH_LIMIT = 1023, HEIGHT_LIMIT = 511 };
    const int length = (vertical ? HEIGHT_LIMIT : WIDTH_LIMIT) + oversized;
    const int start = -(length + 1) / 2;
    const int end = start + length;
    LINE_F2 line = {0};
    SetLineF2(&line);
    setXY2(&line, vertical ? TILE_ORIGIN : start, vertical ? start : TILE_ORIGIN,
           vertical ? TILE_ORIGIN : end, vertical ? end : TILE_ORIGIN);
    setRGB0(&line, RGB888_MAX, 0, 0);
    termPrim(&line);

    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 0);
    DrawOTag((OT_TYPE *)&line);
    u16 pixels[TILE_PIXELS];
    read_tile(TILE_ORIGIN, TILE_ORIGIN, pixels);
    for (int i = 0; i < TILE_PIXELS; ++i) {
        int across = vertical ? i % TILE_SIZE : i / TILE_SIZE;
        unsigned expected = !oversized && across == 0 ? COLOR_MAX : 0;
        unsigned actual = pixels[i] & COLOR_MASK;
        if (actual != expected) {
            fprintf(stderr, "line length %d vertical=%d at pixel %d: got %04x, expected %04x\n",
                    length, vertical, i, actual, expected);
            return 1;
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

static void sample_texture(int x, int clut, int u, int v) {
    SPRT sprite = {0};
    SetSprt(&sprite);
    setXY0(&sprite, x, TILE_ORIGIN);
    setWH(&sprite, TILE_SIZE, TILE_SIZE);
    setUV0(&sprite, u, v);
    setShadeTex(&sprite, 1);
    sprite.clut = clut;
    termPrim(&sprite);
    DrawOTag((OT_TYPE *)&sprite);
}

static int check_uniform_tile(int x, unsigned expected, unsigned mask, const char *source) {
    u16 pixels[TILE_PIXELS];
    read_tile(x, TILE_ORIGIN, pixels);
    for (unsigned i = 0; i < TILE_PIXELS; ++i) {
        unsigned actual = pixels[i] & mask;
        if (actual != expected) {
            fprintf(stderr, "%s at pixel %u: got %04x, expected %04x\n", source, i, actual,
                    expected);
            return 1;
        }
    }
    return 0;
}

static int check_texture(int x, unsigned expected, const char *source) {
    return check_uniform_tile(x, expected, COLOR_MASK, source);
}

static int check_primitive_masks(void) {
    enum { NEXT_X = TILE_ORIGIN + TILE_SIZE, WORD_MASK = COLOR_MASK | PIXEL_MASK_BIT };
    select_draw_mode(0, 0);
    select_mask(0);
    draw_tile(NEXT_X, TILE_ORIGIN, 0, 0, 0, 0);
    select_mask(FORCE_MASK);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    select_mask(CHECK_MASK);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, RGB888_MAX, 0);
    draw_tile(NEXT_X, TILE_ORIGIN, 0, 0, RGB888_MAX, 0);

    int failures = check_uniform_tile(TILE_ORIGIN, PIXEL_MASK_BIT | COLOR_MAX, WORD_MASK,
                                      "mask protection follows the preceding draw");
    failures += check_uniform_tile(NEXT_X, rgb555(0, 0, COLOR_MAX), WORD_MASK,
                                   "unmasked draw leaves bit 15 clear");
    select_mask(FORCE_MASK | CHECK_MASK);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, RGB888_MAX, 0, 0);
    draw_tile(NEXT_X, TILE_ORIGIN, 0, RGB888_MAX, 0, 0);
    failures += check_uniform_tile(TILE_ORIGIN, PIXEL_MASK_BIT | COLOR_MAX, WORD_MASK,
                                   "force and check preserve a protected pixel");
    failures += check_uniform_tile(NEXT_X, PIXEL_MASK_BIT | rgb555(0, COLOR_MAX, 0), WORD_MASK,
                                   "force and check mark an unprotected pixel");
    select_mask(0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 1);
    failures += check_uniform_tile(TILE_ORIGIN, COLOR_MAX / 2, WORD_MASK,
                                   "untextured blend clears the previous mask bit");
    return failures;
}

static int check_vram_fill_mask(void) {
    enum { FILL_WIDTH = 16, WORD_MASK = COLOR_MASK | PIXEL_MASK_BIT };
    TILE fill = {0};
    SetTile(&fill);
    setcode(&fill, 0x02);
    setXY0(&fill, TILE_ORIGIN, TILE_ORIGIN);
    setWH(&fill, FILL_WIDTH, TILE_SIZE);
    setRGB0(&fill, 0, RGB888_MAX, 0);
    termPrim(&fill);

    select_draw_mode(0, 0);
    select_mask(FORCE_MASK);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    select_mask(FORCE_MASK | CHECK_MASK);
    DrawOTag((OT_TYPE *)&fill);
    int failures = check_uniform_tile(TILE_ORIGIN, rgb555(0, COLOR_MAX, 0), WORD_MASK,
                                      "VRAM fill ignores both mask settings");
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    failures += check_uniform_tile(TILE_ORIGIN, PIXEL_MASK_BIT | COLOR_MAX, WORD_MASK,
                                   "VRAM fill leaves the mask setting active");
    select_mask(0);
    return failures;
}

static int check_blend_precision(void) {
    enum {
        HALF = 0,
        ADD = 1,
        SUBTRACT = 2,
        QUARTER = 3,
        LOW_RED = 1 << RGB888_SHIFT,
        SECOND_X = TILE_ORIGIN + 2 * TILE_SIZE,
    };
    select_draw_mode(getTPage(0, HALF, 0, 0), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 0);
    draw_tile(SECOND_X, TILE_ORIGIN, 0, 0, 0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 5 << RGB888_SHIFT, 0, 0, 1);
    draw_tile(SECOND_X, TILE_ORIGIN, 7 << RGB888_SHIFT, 0, 0, 1);
    int failures = check_texture(TILE_ORIGIN, 2, "truncate odd half blend");
    failures += check_texture(SECOND_X, 3, "disjoint half blend");

    select_draw_mode(getTPage(0, ADD, 0, 0), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, 0, 0);
    for (int i = 0; i < COLOR_MAX; ++i) {
        draw_tile(TILE_ORIGIN, TILE_ORIGIN, LOW_RED, 0, 0, 1);
    }
    failures += check_texture(TILE_ORIGIN, COLOR_MAX, "add one 31 times");

    select_draw_mode(getTPage(0, SUBTRACT, 0, 0), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, LOW_RED, 0, 0, 1);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, LOW_RED, 0, 0, 1);
    select_draw_mode(getTPage(0, QUARTER, 0, 0), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 7 << RGB888_SHIFT, 0, 0, 1);
    failures += check_texture(TILE_ORIGIN, COLOR_MAX - 1, "mixed subtraction and quarter add");
    return failures;
}

static void load_striped_tile(int x, int y, const u16 row[TILE_SIZE]) {
    RECT rect = {x, y, TILE_SIZE, TILE_SIZE};
    u16 image[TILE_PIXELS];
    u_long transfer[TILE_PIXELS * sizeof(u16) / sizeof(u_long)];
    for (int i = 0; i < TILE_SIZE; ++i) {
        memcpy(&image[i * TILE_SIZE], row, TILE_SIZE * sizeof(*row));
    }
    memcpy(transfer, image, sizeof(image));
    LoadImage(&rect, transfer);
}

static int check_striped_tile(int x, const u16 expected[TILE_SIZE], const char *source) {
    u16 pixels[TILE_PIXELS];
    read_tile(x, TILE_ORIGIN, pixels);
    for (int i = 0; i < TILE_PIXELS; ++i) {
        if (pixels[i] != expected[i % TILE_SIZE]) {
            fprintf(stderr, "%s at pixel %d: got %04x, expected %04x\n", source, i, pixels[i],
                    expected[i % TILE_SIZE]);
            return 1;
        }
    }
    return 0;
}

static int check_masked_transfers(void) {
    enum { SOURCE_X = 448, SOURCE_Y = 256, ALL_MASKS = FORCE_MASK | CHECK_MASK };
    const u16 base[TILE_SIZE] = {PIXEL_MASK_BIT | 3, 6, 9, PIXEL_MASK_BIT | 12};
    const u16 source[TILE_SIZE] = {5, PIXEL_MASK_BIT | 7, 0, 11};
    RECT source_rect = {SOURCE_X, SOURCE_Y, TILE_SIZE, TILE_SIZE};
    RECT self = {TILE_ORIGIN, TILE_ORIGIN, TILE_SIZE, TILE_SIZE};
    select_mask(0);
    load_striped_tile(SOURCE_X, SOURCE_Y, source);

    int failures = 0;
    for (unsigned flags = 0; flags <= ALL_MASKS; ++flags) {
        u16 expected[TILE_SIZE];
        for (int i = 0; i < TILE_SIZE; ++i) {
            expected[i] = (flags & CHECK_MASK) && (base[i] & PIXEL_MASK_BIT)
                              ? base[i]
                              : source[i] | ((flags & FORCE_MASK) ? PIXEL_MASK_BIT : 0);
        }
        select_mask(0);
        load_striped_tile(TILE_ORIGIN, TILE_ORIGIN, base);
        select_mask(flags);
        load_striped_tile(TILE_ORIGIN, TILE_ORIGIN, source);
        failures += check_striped_tile(TILE_ORIGIN, expected, "mask on CPU to VRAM transfer");

        select_mask(0);
        load_striped_tile(TILE_ORIGIN, TILE_ORIGIN, base);
        select_mask(flags);
        MoveImage(&source_rect, TILE_ORIGIN, TILE_ORIGIN);
        failures += check_striped_tile(TILE_ORIGIN, expected, "mask on VRAM to VRAM transfer");
    }
    const u16 marked[TILE_SIZE] = {PIXEL_MASK_BIT | 3, PIXEL_MASK_BIT | 6, PIXEL_MASK_BIT | 9,
                                   PIXEL_MASK_BIT | 12};
    select_mask(0);
    load_striped_tile(TILE_ORIGIN, TILE_ORIGIN, base);
    select_mask(FORCE_MASK);
    MoveImage(&self, TILE_ORIGIN, TILE_ORIGIN);
    failures += check_striped_tile(TILE_ORIGIN, marked, "mask on self-copy");

    draw_tile(TILE_ORIGIN, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    select_mask(CHECK_MASK);
    MoveImage(&source_rect, TILE_ORIGIN, TILE_ORIGIN);
    failures += check_uniform_tile(TILE_ORIGIN, PIXEL_MASK_BIT | COLOR_MAX,
                                   PIXEL_MASK_BIT | COLOR_MASK, "copy sees a pending mask write");
    select_mask(0);
    return failures;
}

static int check_copy_overlap(int x, unsigned flags) {
    enum { COPY_WIDTH = 132, SHIFT = 2, PROTECTED = 4, FIRST_SPILLED = 130 };
    RECT region = {x, TILE_ORIGIN, COPY_WIDTH + SHIFT, 1};
    RECT source = {x, TILE_ORIGIN, COPY_WIDTH, 1};
    u16 words[COPY_WIDTH + SHIFT];
    u_long transfer[(sizeof(words) + sizeof(u_long) - 1) / sizeof(u_long)] = {0};
    for (int i = 0; i < COPY_WIDTH + SHIFT; ++i) {
        words[i] = i + 1;
    }
    words[PROTECTED] |= PIXEL_MASK_BIT;
    memcpy(transfer, words, sizeof(words));
    select_mask(0);
    LoadImage(&region, transfer);
    select_mask(flags);
    MoveImage(&source, x + SHIFT, TILE_ORIGIN);
    StoreImage(&region, transfer);
    DrawSync(0);
    select_mask(0);
    memcpy(words, transfer, sizeof(words));
    for (int i = SHIFT; i < COPY_WIDTH + SHIFT; ++i) {
        unsigned color = i - 1;
        if (i == PROTECTED && (flags & CHECK_MASK)) {
            color = 5;
        } else if (i == FIRST_SPILLED || i == FIRST_SPILLED + 1) {
            color -= SHIFT;
        }
        const unsigned expected = (flags & FORCE_MASK ? PIXEL_MASK_BIT : 0) | color |
                                  (i == PROTECTED + SHIFT ? PIXEL_MASK_BIT : 0);
        if (words[i] != expected) {
            fprintf(stderr, "overlapping copy from %d at pixel %d: got %04x, expected %04x\n", x, i,
                    words[i], expected);
            return 1;
        }
    }
    return 0;
}

enum { EDGE_SIZE = 3, EDGE_WORDS = EDGE_SIZE * EDGE_SIZE, TRANSFER_PAD = 0x55aa };

static void load_edge_words(RECT *rect, const u16 words[EDGE_WORDS]) {
    u_long data[(EDGE_WORDS * sizeof(u16) + sizeof(u_long) - 1) / sizeof(u_long)] = {0};
    memcpy(data, words, EDGE_WORDS * sizeof(u16));
    LoadImage(rect, data);
}

static int check_edge_words(RECT *rect, const u16 expected[EDGE_WORDS], const char *source) {
    u16 words[EDGE_WORDS + 1];
    u_long data[(sizeof(words) + sizeof(u_long) - 1) / sizeof(u_long)] = {0};
    words[EDGE_WORDS] = TRANSFER_PAD;
    memcpy((u8 *)data + EDGE_WORDS * sizeof(u16), &words[EDGE_WORDS], sizeof(u16));
    StoreImage(rect, data);
    DrawSync(0);
    memcpy(words, data, sizeof(words));
    for (int i = 0; i < EDGE_WORDS; ++i) {
        if (words[i] != expected[i]) {
            fprintf(stderr, "%s (%d,%d) at word %d: got %04x, expected %04x\n", source, rect->x,
                    rect->y, i, words[i], expected[i]);
            return 1;
        }
    }
    if (words[EDGE_WORDS] != TRANSFER_PAD) {
        fprintf(stderr, "%s overwrote the word after an odd-sized readback\n", source);
        return 1;
    }
    return 0;
}

static int check_edge_placement(const RECT *rect, const u16 expected[EDGE_WORDS]) {
    for (int i = 0; i < EDGE_WORDS; ++i) {
        RECT pixel = {(rect->x + i % EDGE_SIZE) % VRAM_WIDTH,
                      (rect->y + i / EDGE_SIZE) % VRAM_HEIGHT, 1, 1};
        u_long data = 0;
        u16 word;
        StoreImage(&pixel, &data);
        DrawSync(0);
        memcpy(&word, &data, sizeof(word));
        if (word != expected[i]) {
            fprintf(stderr, "physical VRAM (%d,%d): got %04x, expected %04x\n", pixel.x, pixel.y,
                    word, expected[i]);
            return 1;
        }
    }
    return 0;
}

static int check_transfer_edges(int x, int y) {
    const u16 words[EDGE_WORDS] = {1, 2, PIXEL_MASK_BIT | 3, 4, 5, 6, 7, 8, 9};
    const u16 zero[EDGE_WORDS] = {0};
    RECT edge = {x, y, EDGE_SIZE, EDGE_SIZE};
    RECT source = {128, 144, EDGE_SIZE, EDGE_SIZE};
    RECT dest = {192, 160, EDGE_SIZE, EDGE_SIZE};
    select_mask(0);
    load_edge_words(&edge, words);
    int failures = check_edge_words(&edge, words, "wrapped upload and readback");
    MoveImage(&edge, dest.x, dest.y);
    failures += check_edge_words(&dest, words, "wrapped copy source");

    load_edge_words(&source, words);
    load_edge_words(&edge, zero);
    MoveImage(&source, edge.x, edge.y);
    failures += check_edge_words(&edge, words, "wrapped copy destination");
    u16 protected[EDGE_WORDS] = {0};
    protected[EDGE_WORDS - 1] = PIXEL_MASK_BIT | 31;
    load_edge_words(&edge, protected);
    select_mask(FORCE_MASK | CHECK_MASK);
    MoveImage(&source, edge.x, edge.y);
    u16 masked[EDGE_WORDS];
    for (int i = 0; i < EDGE_WORDS - 1; ++i) {
        masked[i] = words[i] | PIXEL_MASK_BIT;
    }
    masked[EDGE_WORDS - 1] = protected[EDGE_WORDS - 1];
    failures += check_edge_words(&edge, masked, "mask follows wrapped destination");
    select_mask(0);
    load_edge_words(&edge, protected);
    select_mask(FORCE_MASK | CHECK_MASK);
    load_edge_words(&edge, words);
    failures += check_edge_words(&edge, masked, "mask follows wrapped upload");
    failures += check_edge_placement(&edge, masked);
    select_mask(0);
    return failures;
}

static int check_textured_blend(void) {
    enum { PAGE_X = 448, PAGE_Y = 256, SUBTRACT = 2, DST_RED = 17, SRC_RED = 7 };
    const u16 texels[TILE_SIZE] = {0, SRC_RED, PIXEL_MASK_BIT | SRC_RED, PIXEL_MASK_BIT};
    const u16 expected[TILE_SIZE] = {DST_RED, SRC_RED, PIXEL_MASK_BIT | (DST_RED - SRC_RED),
                                     PIXEL_MASK_BIT | DST_RED};
    load_striped_tile(PAGE_X, PAGE_Y, texels);

    SPRT sprite = {0};
    SetSprt(&sprite);
    setXY0(&sprite, TILE_ORIGIN, TILE_ORIGIN);
    setWH(&sprite, TILE_SIZE, TILE_SIZE);
    setUV0(&sprite, 0, 0);
    setShadeTex(&sprite, 1);
    setSemiTrans(&sprite, 1);
    termPrim(&sprite);
    select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, SUBTRACT, PAGE_X, PAGE_Y), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, DST_RED << RGB888_SHIFT, 0, 0, 0);
    DrawOTag((OT_TYPE *)&sprite);

    return check_striped_tile(TILE_ORIGIN, expected, "textured subtraction");
}

static int check_blend_channel_range(void) {
    enum {
        COLOR_COUNT = COLOR_MAX + 1,
        PIXELS = COLOR_COUNT * COLOR_COUNT,
        PAGE_X = 448,
        PAGE_Y = 256,
        SEMI = 0x8000,
        MODE_COUNT = 4
    };
    RECT source = {PAGE_X, PAGE_Y, COLOR_COUNT, COLOR_COUNT};
    RECT dest = {TILE_ORIGIN, TILE_ORIGIN, COLOR_COUNT, COLOR_COUNT};
    u16 words[PIXELS];
    u_long transfer[PIXELS * sizeof(u16) / sizeof(u_long)];
    for (int i = 0; i < PIXELS; ++i) {
        words[i] = SEMI | (i % COLOR_COUNT);
    }
    memcpy(transfer, words, sizeof(words));
    LoadImage(&source, transfer);

    SPRT sprite = {0};
    SetSprt(&sprite);
    setXY0(&sprite, dest.x, dest.y);
    setWH(&sprite, COLOR_COUNT, COLOR_COUNT);
    setUV0(&sprite, 0, 0);
    setShadeTex(&sprite, 1);
    setSemiTrans(&sprite, 1);
    termPrim(&sprite);

    for (int mode = 0; mode < MODE_COUNT; ++mode) {
        for (int i = 0; i < PIXELS; ++i) {
            words[i] = i / COLOR_COUNT;
        }
        memcpy(transfer, words, sizeof(words));
        LoadImage(&dest, transfer);
        select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, mode, PAGE_X, PAGE_Y), 0);
        DrawOTag((OT_TYPE *)&sprite);
        StoreImage(&dest, transfer);
        DrawSync(0);
        memcpy(words, transfer, sizeof(words));
        for (int i = 0; i < PIXELS; ++i) {
            const int b = i / COLOR_COUNT;
            const int f = i % COLOR_COUNT;
            const int mixed[MODE_COUNT] = {(b + f) / 2, b + f, b - f, b + f / 4};
            const int value = mixed[mode];
            const int expected = value < 0 ? 0 : value > COLOR_MAX ? COLOR_MAX : value;
            const int actual = words[i] & COLOR_MASK;
            if (actual != expected) {
                fprintf(stderr, "blend %d, B=%d F=%d: got %04x, expected %04x\n", mode, b, f,
                        actual, expected);
                return 1;
            }
        }
    }
    return 0;
}

static int check_scaled_canvas_blend(void) {
    enum { DISPLAY_HEIGHT = 240, CANVAS_WIDTH = 480, CANVAS_HEIGHT = 272, SCALE = 2 };
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, DRAW_SIZE, DISPLAY_HEIGHT);
    PutDispEnv(&display);
    if (Psyz_VideoSetResolution((PsyzSize){CANVAS_WIDTH, CANVAS_HEIGHT}) != 0 ||
        Psyz_VideoSetInternalResolution(SCALE) != 0 || Psyz_VideoGetInternalResolution() != SCALE) {
        fputs("Could not configure the 2x native canvas\n", stderr);
        return 1;
    }
    return check_blend_precision() + check_textured_blend() + check_primitive_masks() +
           check_masked_transfers() + check_transfer_edges(VRAM_WIDTH - 2, VRAM_HEIGHT - 1);
}

static int check_clear_draw_order(void) {
    enum {
        FILL_WIDTH = 16,
        AFTER_X = TILE_ORIGIN + TILE_SIZE,
        OUTSIDE_X = TILE_ORIGIN + FILL_WIDTH
    };
    RECT fill = {TILE_ORIGIN, TILE_ORIGIN, FILL_WIDTH, TILE_SIZE};
    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    draw_tile(OUTSIDE_X, TILE_ORIGIN, RGB888_MAX, 0, 0, 0);
    ClearImage(&fill, 0, RGB888_MAX, 0);
    draw_tile(AFTER_X, TILE_ORIGIN, 0, 0, RGB888_MAX, 0);

    int failures = check_texture(TILE_ORIGIN, rgb555(0, COLOR_MAX, 0), "clear after polygon");
    failures += check_texture(AFTER_X, rgb555(0, 0, COLOR_MAX), "polygon after clear");
    failures += check_texture(OUTSIDE_X, COLOR_MAX, "polygon outside clear");
    return failures;
}

static int check_copy_visibility(void) {
    enum { COPY_X = 64, COPY_Y = 256, SAMPLE_X = TILE_ORIGIN + TILE_SIZE };
    RECT source = {TILE_ORIGIN, TILE_ORIGIN, TILE_SIZE, TILE_SIZE};
    select_draw_mode(0, 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, COLOR_MAX << RGB888_SHIFT, 0, 0, 0);
    MoveImage(&source, COPY_X, COPY_Y);
    select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, 0, COPY_X, COPY_Y), 0);
    sample_texture(SAMPLE_X, 0, 0, 0);
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
    sample_texture(TILE_ORIGIN, getClut(CLUT_X, TEXTURE_Y), 0, 0);

    const unsigned new_color = rgb555(0, COLOR_MAX, 0);
    palette[CLUT_INDEX] = (u16)new_color;
    memcpy(palette_data, palette, sizeof(palette));
    LoadImage(&clut, palette_data);
    sample_texture(TILE_ORIGIN + TILE_SIZE, getClut(CLUT_X, TEXTURE_Y), 0, 0);
    int failures = check_texture(TILE_ORIGIN, COLOR_MAX, "original CLUT");
    failures += check_texture(TILE_ORIGIN + TILE_SIZE, new_color, "updated CLUT");
    return failures;
}

static void load_word_block(int x, int y, u16 word) {
    enum { BLOCK_PIXELS = WORD_BLOCK_SIZE * WORD_BLOCK_SIZE };
    RECT block = {x, y, WORD_BLOCK_SIZE, WORD_BLOCK_SIZE};
    const u16 pixels[BLOCK_PIXELS] = {word, word, word, word};
    u_long transfer[BLOCK_PIXELS * sizeof(u16) / sizeof(u_long)];
    memcpy(transfer, pixels, sizeof(pixels));
    LoadImage(&block, transfer);
}

static int check_textured_polygon_dither(void) {
    enum {
        PAGE_X = 256,
        PAGE_Y = 256,
        UV_END = TILE_SIZE - 1,
        NEUTRAL = 128,
        RED = 8,
        GREEN = 9,
        BLUE = 10,
        ADDITIVE_BLEND = 1,
        SAMPLE_X = TILE_ORIGIN + TILE_SIZE,
        OVERSIZED_LEFT = -512,
        OVERSIZED_RIGHT = 512,
    };
    const u16 texel = rgb555(RED, GREEN, BLUE);
    for (int y = 0; y < TILE_SIZE; y += WORD_BLOCK_SIZE) {
        for (int x = 0; x < TILE_SIZE; x += WORD_BLOCK_SIZE) {
            load_word_block(PAGE_X + x, PAGE_Y + y, texel);
        }
    }

    POLY_GT4 quad = {0};
    const int x = TILE_ORIGIN;
    const int end = x + TILE_SIZE;
    SetPolyGT4(&quad);
    setShadeTex(&quad, 1);
    setXY4(&quad, x, x, end, x, x, end, end, end);
    setUV4(&quad, 0, 0, UV_END, 0, 0, UV_END, UV_END, UV_END);
    quad.tpage = getTPage(DIRECT_COLOR_TEXTURE, ADDITIVE_BLEND, PAGE_X, PAGE_Y);
    termPrim(&quad);
    select_draw_mode(0, 1);
    DrawOTag((OT_TYPE *)&quad);
    int failures = check_texture(x, texel, "raw Gouraud polygon with dithering enabled");
    draw_tile(SAMPLE_X, TILE_ORIGIN, 0, 0, 0, 0);
    sample_texture(SAMPLE_X, 0, 0, 0);
    failures += check_texture(SAMPLE_X, texel, "texture page inherited from polygon");

    draw_tile(SAMPLE_X, TILE_ORIGIN, 0, 0, 0, 0);
    draw_tile(SAMPLE_X, TILE_ORIGIN, RGB888_MAX, 0, 0, 1);
    failures += check_texture(SAMPLE_X, COLOR_MAX, "blend mode inherited from polygon");

    setShadeTex(&quad, 0);
    setRGB0(&quad, NEUTRAL, NEUTRAL, NEUTRAL);
    setRGB1(&quad, NEUTRAL, NEUTRAL, NEUTRAL);
    setRGB2(&quad, NEUTRAL, NEUTRAL, NEUTRAL);
    setRGB3(&quad, NEUTRAL, NEUTRAL, NEUTRAL);
    DrawOTag((OT_TYPE *)&quad);
    failures += check_dither_pixels(x, x, RED << RGB888_SHIFT, GREEN << RGB888_SHIFT,
                                    BLUE << RGB888_SHIFT, 1);

    select_draw_mode(0, 0);
    draw_tile(SAMPLE_X, TILE_ORIGIN, 0, 0, 0, 0);
    setXY4(&quad, OVERSIZED_LEFT, x, OVERSIZED_RIGHT, x, OVERSIZED_LEFT, end, OVERSIZED_RIGHT, end);
    DrawOTag((OT_TYPE *)&quad);
    failures += check_dither_pixels(x, x, RED << RGB888_SHIFT, GREEN << RGB888_SHIFT,
                                    BLUE << RGB888_SHIFT, 1);
    sample_texture(SAMPLE_X, 0, 0, 0);
    failures += check_texture(SAMPLE_X, texel, "texture page inherited from rejected polygon");
    return failures;
}

static int check_sprite_uv_wrap(void) {
    enum { PAGE_X = 512, PAGE_Y = 256, UV = TEXTURE_PAGE_SIZE - TILE_SIZE / 2 };
    const u16 colors[] = {COLOR_MAX, rgb555(0, COLOR_MAX, 0), rgb555(0, 0, COLOR_MAX),
                          rgb555(COLOR_MAX, COLOR_MAX, 0)};
    u16 pixels[TILE_PIXELS];
    load_word_block(PAGE_X + UV, PAGE_Y + UV, colors[0]);
    load_word_block(PAGE_X, PAGE_Y + UV, colors[1]);
    load_word_block(PAGE_X + UV, PAGE_Y, colors[2]);
    load_word_block(PAGE_X, PAGE_Y, colors[3]);
    select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, 0, PAGE_X, PAGE_Y), 0);
    sample_texture(TILE_ORIGIN, 0, UV, UV);
    read_tile(TILE_ORIGIN, TILE_ORIGIN, pixels);
    for (int row = 0; row < TILE_SIZE; ++row) {
        for (int col = 0; col < TILE_SIZE; ++col) {
            unsigned quadrant = (row >= TILE_SIZE / 2) * 2 + (col >= TILE_SIZE / 2);
            unsigned actual = pixels[row * TILE_SIZE + col] & COLOR_MASK;
            if (actual != colors[quadrant]) {
                fprintf(stderr, "sprite UV wrap at (%d,%d): got %04x, expected %04x\n", col, row,
                        actual, colors[quadrant]);
                return 1;
            }
        }
    }
    return 0;
}

static int check_direct_page_wrap(void) {
    enum { UV = VRAM_WIDTH - LAST_TEXTURE_PAGE_X };
    select_draw_mode(getTPage(DIRECT_COLOR_TEXTURE, 0, LAST_TEXTURE_PAGE_X, 0), 0);
    draw_tile(0, 0, 0, 0, COLOR_MAX << RGB888_SHIFT, 0);
    DrawSync(0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, COLOR_MAX << RGB888_SHIFT, 0);
    draw_tile(0, 0, COLOR_MAX << RGB888_SHIFT, 0, 0, 0);
    sample_texture(TILE_ORIGIN, 0, UV, 0);
    return check_texture(TILE_ORIGIN, COLOR_MAX, "same-frame direct page wrap");
}

static int check_indexed_page_and_clut_wrap(void) {
    enum {
        PAGE_Y = 256,
        CLUT_Y = 64,
        TEXELS_PER_WORD = 2,
        UV = (VRAM_WIDTH - LAST_TEXTURE_PAGE_X) * TEXELS_PER_WORD,
        CLUT_INDEX = VRAM_WIDTH - LAST_CLUT_X,
        INDICES = CLUT_INDEX | (CLUT_INDEX << 8)
    };
    load_word_block(0, PAGE_Y, INDICES);
    load_word_block(0, PAGE_Y + WORD_BLOCK_SIZE, INDICES);
    select_draw_mode(getTPage(INDEXED_8_BIT_TEXTURE, 0, LAST_TEXTURE_PAGE_X, PAGE_Y), 0);
    draw_tile(TILE_ORIGIN, TILE_ORIGIN, 0, 0, COLOR_MAX << RGB888_SHIFT, 0);
    draw_tile(0, CLUT_Y, 0, COLOR_MAX << RGB888_SHIFT, 0, 0);
    sample_texture(TILE_ORIGIN, getClut(LAST_CLUT_X, CLUT_Y), UV, 0);
    return check_texture(TILE_ORIGIN, rgb555(0, COLOR_MAX, 0),
                         "same-frame 8-bit page and CLUT wrap");
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
    failures += check_flat_line(0, 0);
    failures += check_flat_line(0, 1);
    failures += check_flat_line(1, 0);
    failures += check_flat_line(1, 1);
    failures += check_point_line();
    failures += check_large_line(0, 0);
    failures += check_large_line(0, 1);
    failures += check_large_line(1, 0);
    failures += check_large_line(1, 1);
    failures += check_blend(0, rgb555(6, 15, 18));
    failures += check_blend(1, rgb555(12, 30, COLOR_MAX));
    failures += check_blend(2, rgb555(0, 8, 19));
    failures += check_blend(3, rgb555(6, 21, 30));
    failures += check_blend_precision();
    failures += check_textured_blend();
    failures += check_blend_channel_range();
    failures += check_primitive_masks();
    failures += check_vram_fill_mask();
    failures += check_masked_transfers();
    failures += check_copy_overlap(64, FORCE_MASK | CHECK_MASK);
    failures += check_copy_overlap(VRAM_WIDTH - 64, FORCE_MASK | CHECK_MASK);
    failures += check_copy_overlap(VRAM_WIDTH - 64, 0);
    failures += check_transfer_edges(VRAM_WIDTH - 2, 60);
    failures += check_transfer_edges(64, VRAM_HEIGHT - 1);
    failures += check_transfer_edges(VRAM_WIDTH - 2, VRAM_HEIGHT - 1);
    failures += check_clear_draw_order();
    failures += check_copy_visibility();
    failures += check_clut_visibility();
    failures += check_textured_polygon_dither();
    failures += check_sprite_uv_wrap();
    failures += check_direct_page_wrap();
    failures += check_indexed_page_and_clut_wrap();
    failures += check_scaled_canvas_blend();
    return failures != 0;
}
