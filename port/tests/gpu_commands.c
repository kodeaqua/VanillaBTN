#include "psx/gpu_links.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <libgpu.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    hm_vram_reset();
    hm_gpu_links_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 32, 32);
    draw.dfe = 1;
    PutDrawEnv(&draw);
    u32 ot[4];
    ClearOTagR(ot, 4);
    POLY_F4 quad = {0};
    SetPolyF4(&quad);
    setRGB0(&quad, 255, 0, 0);
    setXYWH(&quad, 2, 3, 4, 3);
    addPrim(ot + 3, &quad);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 3) == 31 && hm_vram_pixel(5, 5) == 31);
    CHECK(hm_vram_pixel(6, 3) == 0 && hm_vram_pixel(2, 6) == 0);
    ClearOTagR(ot, 4);
    setRGB0(&quad, 0, 0, 255);
    setSemiTrans(&quad, 1);
    addPrim(ot + 3, &quad);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 3) == (15 | (15 << 10)));

    RECT texture = {640, 0, 1, 1};
    u32 packed = 0x3210;
    LoadImage(&texture, &packed);
    RECT palette = {0, 100, 4, 1};
    u32 colors[2] = {0x001F0000, 0x7C0083E0};
    LoadImage(&palette, colors);
    ClearOTagR(ot, 4);
    SPRT sprite = {0};
    setSprt(&sprite);
    setShadeTex(&sprite, 1);
    setXY0(&sprite, 10, 10);
    setWH(&sprite, 4, 1);
    sprite.clut = GetClut(0, 100);
    DR_MODE mode;
    SetDrawMode(&mode, 1, 0, GetTPage(0, 0, 640, 0), NULL);
    addPrim(ot + 3, &sprite);
    addPrim(ot + 3, &mode);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(10, 10) == 0);
    CHECK(hm_vram_pixel(11, 10) == 31);
    CHECK(hm_vram_pixel(12, 10) == 0x83E0);
    CHECK(hm_vram_pixel(13, 10) == 0x7C00);
    CHECK(hm_vram_pixel(14, 10) == 0);
    /* A raw textured quad uses its own page and affine UVs, including the
     * final texel. Its shared triangle edge must not blend twice. */
    u32 direct[2] = {0x03E0001F, 0x7FFF7C00};
    texture.w = 4;
    LoadImage(&texture, direct);
    POLY_FT4 textured = {0};
    setPolyFT4(&textured);
    setShadeTex(&textured, 1);
    setXYWH(&textured, 10, 15, 4, 1);
    setUVWH(&textured, 0, 0, 3, 0);
    textured.tpage = GetTPage(2, 0, 640, 0);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, &textured);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(10, 15) == 31 && hm_vram_pixel(11, 15) == 0x03E0);
    CHECK(hm_vram_pixel(12, 15) == 0x7C00 && hm_vram_pixel(13, 15) == 0x7FFF);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    u32 shaded[7] = {0, 0x300000FF, 0x00010001, 0x0000FF00, 0x00010007, 0x00FF0000, 0x00070001};
    setlen(shaded, 6);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, shaded);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(1, 1) == (26 | (2 << 5) | (2 << 10)));
    CHECK(hm_vram_pixel(7, 7) == 0);
    u32 color = shaded[3], xy = shaded[4];
    shaded[3] = shaded[5];
    shaded[4] = shaded[6];
    shaded[5] = color;
    shaded[6] = xy;
    ClearOTagR(ot, 4);
    addPrim(ot + 3, shaded);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(1, 1) == (26 | (2 << 5) | (2 << 10)));
    draw.dtd = 1;
    PutDrawEnv(&draw);
    shaded[1] = 0x30808080;
    shaded[3] = shaded[5] = 0x00808080;
    ClearOTagR(ot, 4);
    addPrim(ot + 3, shaded);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(1, 1) == 0x3DEF && hm_vram_pixel(2, 1) == 0x4210);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    u32 line[4] = {0, 0x400000FF, 0x00140002, 0x00140006};
    setlen(line, 3);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, line);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 20) == 31 && hm_vram_pixel(6, 20) == 31);
    CHECK(hm_vram_pixel(7, 20) == 0);
    u32 point[5] = {0, 0x500000FF, 0x00150002, 0x00FF0000, 0x00150002};
    setlen(point, 4);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, point);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 21) == 31); /* Coincident vertices use first color. */
    u32 chain[6] = {0, 0x4800FF00, 0x00160002, 0x00160004, 0x00180004, 0x55555555};
    setlen(chain, 5);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, chain);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 22) == 0x03E0 && hm_vram_pixel(4, 24) == 0x03E0);
    /* Both quad triangles touch the diagonal. A semi-transparent primitive
     * must blend there once, exactly as at every other covered pixel. */
    RECT background = {20, 20, 4, 4};
    ClearImage(&background, 0, 0, 0);
    setRGB0(&quad, 128, 0, 0);
    setXYWH(&quad, 20, 20, 4, 4);
    setSemiTrans(&quad, 1);
    draw.tpage = GetTPage(0, 1, 0, 0);
    PutDrawEnv(&draw);
    ClearOTagR(ot, 4);
    addPrim(ot + 3, &quad);
    DrawOTag(ot + 3);
    for (u32 y = 20; y < 24; ++y)
        for (u32 x = 20; x < 24; ++x)
            CHECK(hm_vram_pixel(x, y) == 16);

    /* Game scenes draw polygons into the opposite framebuffer with dfe off,
     * while displaying the other half of VRAM. */
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, 320, 240);
    PutDispEnv(&display);
    SetDispMask(1);
    SetDefDrawEnv(&draw, 0, 240, 320, 240);
    PutDrawEnv(&draw);
    ClearOTagR(ot, 4);
    SetPolyF4(&quad);
    setRGB0(&quad, 255, 0, 0);
    setXYWH(&quad, 2, 3, 4, 3);
    addPrim(ot + 3, &quad);
    DrawOTag(ot + 3);
    CHECK(hm_vram_pixel(2, 243) == 31);
    CHECK(hm_vram_pixel(2, 3) != 31);

    /* GP0(80h) copies wrapped regions and preserves overlapping sources. */
    hm_vram_reset();
    const u16 wrapped_source[2][4] = {{0x001f, 0x03e0, 0x7c00, 0x4210},
                                      {0x1234, 0x2345, 0x3456, 0x4567}};
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 4; ++x)
            hm_vram_draw_pixel(1022 + x, 511 + y, wrapped_source[y][x]);
    const u32 wrapped_copy[] = {0x80000000, 1022u | (511u << 16), 5u | (8u << 16),
                                4u | (2u << 16)};
    hm_gpu_execute_words(wrapped_copy, 4);
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 4; ++x)
            CHECK(hm_vram_pixel(5 + x, 8 + y) == wrapped_source[y][x]);

    hm_vram_reset();
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 4; ++x)
            hm_vram_draw_pixel(1008 + x, 510 + y, wrapped_source[y][x]);
    const u32 wrapped_destination[] = {0x80000000, 1008u | (510u << 16),
                                       1022u | (511u << 16), 4u | (2u << 16)};
    hm_gpu_execute_words(wrapped_destination, 4);
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 4; ++x)
            CHECK(hm_vram_pixel((1022 + x) & 1023, (511 + y) & 511) == wrapped_source[y][x]);

    hm_vram_reset();
    for (u32 x = 0; x < 6; ++x)
        hm_vram_draw_pixel(20 + x, 30, (u16)(x + 1));
    const u32 overlapping_copy[] = {0x80000000, 20u | (30u << 16), 22u | (30u << 16),
                                    4u | (1u << 16)};
    hm_gpu_execute_words(overlapping_copy, 4);
    for (u32 x = 0; x < 4; ++x)
        CHECK(hm_vram_pixel(22 + x, 30) == x + 1);

    /* E6 mask-check and mask-set state applies to GP0(80h) destinations. */
    hm_vram_reset();
    hm_vram_draw_pixel(30, 40, 0x1234);
    hm_vram_draw_pixel(31, 40, 0x4000);
    hm_vram_draw_pixel(50, 40, 0x8001);
    hm_vram_draw_pixel(51, 40, 0x0002);
    const u32 check_mask_copy[] = {0xe6000002, 0x80000000, 30u | (40u << 16),
                                   50u | (40u << 16), 2u | (1u << 16)};
    hm_gpu_execute_words(check_mask_copy, 5);
    CHECK(hm_vram_pixel(50, 40) == 0x8001);
    CHECK(hm_vram_pixel(51, 40) == 0x4000);
    const u32 set_mask_copy[] = {0xe6000001, 0x80000000, 30u | (40u << 16),
                                 52u | (40u << 16), 2u | (1u << 16)};
    hm_gpu_execute_words(set_mask_copy, 5);
    CHECK(hm_vram_pixel(52, 40) == 0x9234);
    CHECK(hm_vram_pixel(53, 40) == 0xc000);

    /* GP0(A0h) uploads share the destination mask controls. */
    hm_vram_reset();
    hm_vram_draw_pixel(60, 50, 0x8001);
    hm_vram_draw_pixel(61, 50, 0x0002);
    const u32 check_mask_load[] = {0xe6000002, 0xa0000000, 60u | (50u << 16),
                                   2u | (1u << 16), 0x45671234};
    hm_gpu_execute_words(check_mask_load, 5);
    CHECK(hm_vram_pixel(60, 50) == 0x8001);
    CHECK(hm_vram_pixel(61, 50) == 0x4567);
    const u32 set_mask_load[] = {0xe6000001, 0xa0000000, 62u | (50u << 16),
                                 2u | (1u << 16), 0x45671234};
    hm_gpu_execute_words(set_mask_load, 5);
    CHECK(hm_vram_pixel(62, 50) == 0x9234);
    CHECK(hm_vram_pixel(63, 50) == 0xc567);

    /* GP0 memory-transfer commands ignore the low five opcode bits. */
    for (u32 opcode = 0x80; opcode <= 0x9f; ++opcode) {
        hm_vram_reset();
        hm_vram_draw_pixel(7, 9, 0x4d2b);
        const u32 copy_alias[] = {opcode << 24, 7u | (9u << 16), 20u | (30u << 16),
                                  1u | (1u << 16)};
        hm_gpu_execute_words(copy_alias, 4);
        CHECK(hm_vram_pixel(20, 30) == 0x4d2b);
    }
    for (u32 opcode = 0xa0; opcode <= 0xbf; ++opcode) {
        hm_vram_reset();
        const u32 upload_alias[] = {opcode << 24, 20u | (30u << 16), 1u | (1u << 16),
                                    0x00004d2b};
        hm_gpu_execute_words(upload_alias, 4);
        CHECK(hm_vram_pixel(20, 30) == 0x4d2b);
    }

    /* Private materialization writes only tiles that later texture reads use. */
    ResetGraph(0);
    DRAWENV filter_draw;
    SetDefDrawEnv(&filter_draw, 0, 0, 32, 32);
    filter_draw.dfe = 1;
    PutDrawEnv(&filter_draw);
    hm_vram_reset();
    u64 materialization_tiles[HM_VRAM_DIRTY_ROWS] = {0};
    materialization_tiles[0] = 1;
    hm_vram_write_filter_begin(materialization_tiles);
    CHECK(hm_vram_write_filter_intersects(0, 0, 16, 16));
    CHECK(!hm_vram_write_filter_intersects(16, 0, 32, 16));
    CHECK(!hm_vram_write_filter_intersects(1024, 0, 1040, 16));
    const u32 filtered_rectangle[] = {0x600000ff, 2u | (2u << 16), 20u | (1u << 16)};
    hm_gpu_execute_words(filtered_rectangle, 3);
    hm_vram_write_filter_end();
    CHECK(hm_vram_pixel(2, 2) == 31 && hm_vram_pixel(15, 2) == 31);
    CHECK(hm_vram_pixel(16, 2) == 0 && hm_vram_pixel(21, 2) == 0);

    hm_vram_write_filter_end();
    hm_vram_reset();
    const u32 filtered_triangle[] = {0x200000ff, 0x00020002, 0x001d0004, 0x0004001d};
    hm_gpu_execute_words(filtered_triangle, 4);
    u16 expected_triangle[32 * 32];
    u32 allowed_triangle_pixels = 0;
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            expected_triangle[y * 32 + x] = hm_vram_pixel(x, y);
            if (x < 16 && y < 16 && expected_triangle[y * 32 + x])
                ++allowed_triangle_pixels;
        }
    CHECK(allowed_triangle_pixels > 0);
    hm_vram_reset();
    hm_vram_write_filter_begin(materialization_tiles);
    hm_gpu_execute_words(filtered_triangle, 4);
    hm_vram_write_filter_end();
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            CHECK(hm_vram_pixel(x, y) == (x < 16 && y < 16 ? expected_triangle[y * 32 + x] : 0));

    /* Sparse feedback masks rasterize only their selected horizontal tile
     * spans while preserving the same pixels as the unfiltered triangle. */
    ResetGraph(0);
    DRAWENV sparse_draw;
    SetDefDrawEnv(&sparse_draw, 0, 0, 64, 32);
    sparse_draw.dfe = 1;
    PutDrawEnv(&sparse_draw);
    const u32 sparse_triangle[] = {0x200000ff, 0x00020002, 0x0002003d, 0x001d0002};
    hm_vram_reset();
    hm_gpu_execute_words(sparse_triangle, 4);
    u16 expected_sparse[64 * 32];
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 64; ++x)
            expected_sparse[y * 64 + x] = hm_vram_pixel(x, y);
    hm_vram_reset();
    materialization_tiles[0] = (UINT64_C(1) << 0) | (UINT64_C(1) << 2);
    hm_vram_write_filter_begin(materialization_tiles);
    hm_gpu_execute_words(sparse_triangle, 4);
    hm_vram_write_filter_end();
    u32 first_span_pixels = 0, second_span_pixels = 0;
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 64; ++x) {
            int selected = y < 16 && (x < 16 || (x >= 32 && x < 48));
            CHECK(hm_vram_pixel(x, y) == (selected ? expected_sparse[y * 64 + x] : 0));
            if (selected && expected_sparse[y * 64 + x]) {
                if (x < 16)
                    ++first_span_pixels;
                else
                    ++second_span_pixels;
            }
        }
    CHECK(first_span_pixels > 0 && second_span_pixels > 0);

    /* Rectangle and sprite replay uses the same sparse horizontal mask. */
    const u32 sparse_rectangle[] = {0x600000ff, 0x00020002, 60u | (30u << 16)};
    hm_vram_reset();
    hm_gpu_execute_words(sparse_rectangle, 3);
    u16 expected_sparse_rectangle[64 * 32];
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 64; ++x)
            expected_sparse_rectangle[y * 64 + x] = hm_vram_pixel(x, y);
    hm_vram_reset();
    hm_vram_write_filter_begin(materialization_tiles);
    hm_gpu_execute_words(sparse_rectangle, 3);
    hm_vram_write_filter_end();
    first_span_pixels = second_span_pixels = 0;
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 64; ++x) {
            int selected = y < 16 && (x < 16 || (x >= 32 && x < 48));
            CHECK(hm_vram_pixel(x, y) == (selected ? expected_sparse_rectangle[y * 64 + x] : 0));
            if (selected && expected_sparse_rectangle[y * 64 + x]) {
                if (x < 16)
                    ++first_span_pixels;
                else
                    ++second_span_pixels;
            }
        }
    CHECK(first_span_pixels > 0 && second_span_pixels > 0);

    hm_vram_reset();
    materialization_tiles[0] = 1;
    hm_vram_write_filter_begin(materialization_tiles);
    RECT partial_upload = {14, 4, 4, 1};
    const u32 partial_upload_pixels[] = {0x22221111, 0x44443333};
    LoadImage(&partial_upload, partial_upload_pixels);
    hm_vram_write_filter_end();
    CHECK(hm_vram_pixel(14, 4) == 0x1111 && hm_vram_pixel(15, 4) == 0x2222);
    CHECK(hm_vram_pixel(16, 4) == 0 && hm_vram_pixel(17, 4) == 0);
    return 0;
}
