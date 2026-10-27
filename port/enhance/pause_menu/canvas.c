#include "enhance/pause_menu/canvas.h"
#include <string.h>
void hm_pause_canvas_extract(u8 *ui, const u8 *frozen, u32 width) {
    u32 left = (width - 320) / 2;
    for (u32 y = 0; y < 240; ++y)
        memcpy(ui + y * 320 * 4, frozen + (y * width + left) * 4, 320 * 4);
}
void hm_pause_canvas_compose(u8 *output, const u8 *frozen, u32 width, const u8 *ui) {
    u32 left = (width - 320) / 2;
    for (u32 y = 0; y < 240; ++y) {
        for (u32 x = 0; x < width; ++x) {
            u32 at = (y * width + x) * 4;
            for (u32 channel = 0; channel < 3; ++channel)
                output[at + channel] = (u8)((u32)frozen[at + channel] * 3 / 5);
            output[at + 3] = 255;
        }
        memcpy(output + (y * width + left) * 4, ui + y * 320 * 4, 320 * 4);
    }
}
