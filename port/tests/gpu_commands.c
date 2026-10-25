#include "psx/gpu_links.h"
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
    return 0;
}
