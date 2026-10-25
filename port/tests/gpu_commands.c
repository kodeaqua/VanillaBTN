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
    return 0;
}
