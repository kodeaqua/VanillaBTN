#include <libgpu.h>

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(void) {
    CHECK(getClut(896, 511) == 0x7FF8);
    CHECK(GetClut(896, 511) == 0x7FF8);
    CHECK(getTPage(2, 3, 960, 256) == 0x017F);
    CHECK(GetTPage(0, 0, 896, 0) == 14);
    DR_TPAGE page = {.tag = 0x00ABCDEF};
    setDrawTPage(&page, 1, 1, 14);
    CHECK(page.tag == 0x01ABCDEF);
    CHECK(page.code[0] == 0xE100060E); /* Same draw mode used by the original text path. */
    DR_MODE mode = {0};
    RECT window = {192, 128, 64, 128};
    SetDrawMode(&mode, 0, 0, 0, &window);
    CHECK(mode.tag >> 24 == 2);
    CHECK(mode.code[0] == 0xE1000000 && mode.code[1] == 0xE2086218);
    SetDrawMode(&mode, 1, 1, 14, NULL);
    CHECK(mode.code[1] == 0xE2000000);
    DR_AREA clip = {0};
    RECT area = {10, 20, 30, 40};
    SetDrawArea(&clip, &area);
    CHECK(clip.code[0] == 0xE300500A && clip.code[1] == 0xE400EC27);
    DR_LOAD load = {.tag = 0x00123456};
    RECT palette = {256, 32, 16, 1};
    SetDrawLoad(&load, &palette);
    CHECK(load.tag == 0x0B123456);
    CHECK(load.code[0] == 0xA0000000 && load.code[1] == 0x00200100);
    CHECK(load.code[2] == 0x00010010);
    SPRT sprite = {0};
    setSprt(&sprite);
    CHECK(sprite.tag >> 24 == 4 && sprite.code == 0x64);
    setSemiTrans(&sprite, 1);
    setShadeTex(&sprite, 1);
    CHECK(sprite.code == 0x67);
    SetSemiTrans(&sprite, 0);
    setShadeTex(&sprite, 0);
    CHECK(sprite.code == 0x64);
    POLY_FT4 quad = {0};
    setPolyFT4(&quad);
    setXYWH(&quad, -1, 2, 8, 12);
    setUVWH(&quad, 3, 4, 7, 11);
    CHECK(quad.tag >> 24 == 9 && quad.code == 0x2C);
    CHECK(quad.x0 == -1 && quad.x1 == 7 && quad.y3 == 14);
    CHECK(quad.u1 == 10 && quad.v3 == 15);
    return 0;
}
