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

int main(void) {
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
    CHECK(DrawSync(0) == 0);
    return 0;
}
