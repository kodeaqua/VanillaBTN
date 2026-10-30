#include "psx/vram.h"
#include <libgpu.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    hm_vram_reset();
    for (u32 row = 0; row < 32; ++row) {
        CHECK(hm_vram_dirty_row(row) == UINT64_MAX);
        hm_vram_acknowledge(row, UINT64_MAX);
    }
    RECT rect = {1023, 511, 2, 2};
    const u32 source[2] = {0x56781234, 0xDEF09ABC};
    CHECK(LoadImage(&rect, source) == 0 && DrawSync(1) == 0);
    CHECK(hm_vram_pixel(1023, 511) == 0x1234 && hm_vram_pixel(0, 511) == 0x5678);
    CHECK(hm_vram_pixel(1023, 0) == 0x9ABC && hm_vram_pixel(0, 0) == 0xDEF0);
    CHECK(hm_vram_pixel(1, 511) == 0);
    CHECK(hm_vram_dirty_row(0) == (UINT64_C(1) << 63 | 1));
    CHECK(hm_vram_dirty_row(31) == (UINT64_C(1) << 63 | 1));
    CHECK(hm_vram_dirty_row(1) == 0);
    u32 copy[2];
    CHECK(StoreImage(&rect, copy) == 0 && !memcmp(copy, source, sizeof(copy)));
    hm_vram_mask(1, 1);
    const u32 replace[2] = {0x11111111, 0x11111111};
    LoadImage(&rect, replace);
    CHECK(hm_vram_pixel(1023, 511) == 0x9111 && hm_vram_pixel(0, 511) == 0x9111);
    CHECK(hm_vram_pixel(1023, 0) == 0x9ABC && hm_vram_pixel(0, 0) == 0xDEF0);
    /* Fill ignores mask flags, rounds X down and width up to 16 words. */
    rect = (RECT){1023, 511, 17, 2};
    CHECK(ClearImage(&rect, 255, 128, 64) == 0);
    const u16 color = 31 | 16 << 5 | 8 << 10;
    CHECK(hm_vram_pixel(1008, 511) == color && hm_vram_pixel(15, 0) == color);
    CHECK(hm_vram_pixel(16, 0) == 0 && hm_vram_pixel(1007, 511) == 0);
    rect = (RECT){1008, 511, 0, 1};
    ClearImage(&rect, 0, 0, 0);
    CHECK(hm_vram_pixel(1008, 511) == color);
    rect = (RECT){1008, 511, 16, 512};
    ClearImage(&rect, 0, 0, 0);
    CHECK(hm_vram_pixel(1008, 511) == color);
    hm_vram_mask(0, 0);
    rect = (RECT){10, 10, 1, 1};
    const u16 single = 0xBEEF;
    LoadImage(&rect, (const u32 *)&single);
    u16 tight = 0;
    StoreImage(&rect, (u32 *)&tight);
    CHECK(tight == single);
    hm_vram_reset();
    rect = (RECT){1023, 511, 2, 2};
    LoadImage(&rect, source);
    CHECK(MoveImage(&rect, 15, 15) == 0);
    RECT target = {15, 15, 2, 2};
    StoreImage(&target, copy);
    CHECK(!memcmp(copy, source, sizeof(copy)));
    hm_vram_mask(1, 1);
    CHECK(MoveImage2(&rect, 15, 15) == 0);
    CHECK(hm_vram_pixel(15, 15) == (0x1234 | 0x8000));
    CHECK(hm_vram_pixel(15, 16) == 0x9ABC);
    /* Identical coordinates are valid and still apply mask settings. */
    CHECK(MoveImage(&target, 15, 15) == 0);
    CHECK(hm_vram_pixel(16, 15) == (0x5678 | 0x8000));
    CHECK(DrawSync(0) == 0);

    /* Partial overlap copies from a stable source snapshot in both axes. */
    hm_vram_reset();
    hm_vram_mask(0, 0);
    RECT overlap = {10, 10, 4, 3};
    const u32 overlap_source[6] = {0x10011000, 0x10031002, 0x10051004,
                                   0x10071006, 0x10091008, 0x100B100A};
    CHECK(LoadImage(&overlap, overlap_source) == 0);
    CHECK(MoveImage(&overlap, 11, 11) == 0);
    RECT overlap_target = {11, 11, 4, 3};
    u32 overlap_result[6] = {0};
    CHECK(StoreImage(&overlap_target, overlap_result) == 0);
    CHECK(!memcmp(overlap_result, overlap_source, sizeof(overlap_source)));

    /* Circular VRAM coordinates must retain the same behavior at the Y edge. */
    hm_vram_reset();
    RECT wrapped_overlap = {50, 510, 2, 4};
    const u32 wrapped_source[4] = {0x20012000, 0x20032002, 0x20052004, 0x20072006};
    CHECK(LoadImage(&wrapped_overlap, wrapped_source) == 0);
    CHECK(MoveImage(&wrapped_overlap, 50, 511) == 0);
    RECT wrapped_target = {50, 511, 2, 4};
    u32 wrapped_result[4] = {0};
    CHECK(StoreImage(&wrapped_target, wrapped_result) == 0);
    CHECK(!memcmp(wrapped_result, wrapped_source, sizeof(wrapped_source)));

    /* X wrapping uses the same overlap snapshot behavior. */
    hm_vram_reset();
    RECT wrapped_x = {1022, 400, 4, 1};
    const u32 wrapped_x_source[2] = {0x30013000, 0x30033002};
    CHECK(LoadImage(&wrapped_x, wrapped_x_source) == 0);
    CHECK(MoveImage(&wrapped_x, 1023, 400) == 0);
    RECT wrapped_x_target = {1023, 400, 4, 1};
    u32 wrapped_x_result[2] = {0};
    CHECK(StoreImage(&wrapped_x_target, wrapped_x_result) == 0);
    CHECK(!memcmp(wrapped_x_result, wrapped_x_source, sizeof(wrapped_x_source)));

    /* Mask checking and setting still apply after the overlap is staged. */
    hm_vram_reset();
    hm_vram_mask(0, 0);
    CHECK(LoadImage(&overlap, overlap_source) == 0);
    hm_vram_mask(1, 1);
    CHECK(MoveImage(&overlap, 11, 11) == 0);
    memset(overlap_result, 0, sizeof(overlap_result));
    CHECK(StoreImage(&overlap_target, overlap_result) == 0);
    for (u32 i = 0; i < 6; ++i) {
        u16 expected = (u16)(overlap_source[i / 2] >> ((i & 1) * 16));
        CHECK(((u16 *)overlap_result)[i] == (u16)(expected | 0x8000));
    }
    return 0;
}
