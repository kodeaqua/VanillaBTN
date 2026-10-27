#include "enhance/pause_menu/canvas.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static u8 frozen[1024 * 240 * 4], output[sizeof frozen], ui[320 * 240 * 4];
int main(void) {
    const u32 widths[] = {320, 428, 571, 1024};
    for (u32 i = 0; i < sizeof widths / sizeof widths[0]; ++i) {
        u32 w = widths[i], left = (w - 320) / 2;
        for (u32 y = 0; y < 240; ++y)
            for (u32 x = 0; x < w; ++x) {
                u32 at = (y * w + x) * 4;
                frozen[at] = (u8)x;
                frozen[at + 1] = (u8)y;
                frozen[at + 2] = 200;
                frozen[at + 3] = 255;
            }
        hm_pause_canvas_extract(ui, frozen, w);
        for (u32 y = 0; y < 240; ++y) {
            CHECK(!memcmp(ui + y * 320 * 4, frozen + (y * w + left) * 4, 320 * 4));
        }
        /* Stand-in for the existing 320x240 UI draw: a uniformly bright panel. */
        memset(ui, 250, sizeof ui);
        hm_pause_canvas_compose(output, frozen, w, ui);
        for (u32 y = 0; y < 240; ++y)
            for (u32 x = 0; x < w; ++x) {
                u32 at = (y * w + x) * 4;
                if (x >= left && x < left + 320) {
                    CHECK(!memcmp(output + at, ui + (y * 320 + x - left) * 4, 4));
                } else {
                    CHECK(output[at] == (u32)frozen[at] * 3 / 5);
                    CHECK(output[at + 1] == (u32)frozen[at + 1] * 3 / 5);
                    CHECK(output[at + 2] == 120 && output[at + 3] == 255);
                }
            }
        if (w == 320)
            CHECK(!memcmp(output, ui, sizeof ui));
    }
    return 0;
}
