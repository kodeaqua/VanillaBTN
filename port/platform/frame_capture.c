#include "platform/frame_capture.h"
#include "psx/gpu_environment.h"
#include "psx/vram.h"
#include <stdio.h>

static void hm_capture_word(u8 *out, u32 word) {
    for (u32 i = 0; i < 4; ++i)
        out[i] = (u8)(word >> (i * 8));
}
int hm_frame_capture(const char *path) {
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
    if (!path || state->display.isrgb24 || area->w <= 0 || area->w > 1024 || area->h <= 0 ||
        area->h > 512)
        return 0;
    u32 width = (u32)area->w, height = (u32)area->h;
    u32 stride = (width * 3 + 3) & ~3u;
    u8 header[54] = {'B', 'M'}, row[1024 * 3];
    hm_capture_word(header + 2, 54 + stride * height);
    hm_capture_word(header + 10, 54);
    hm_capture_word(header + 14, 40);
    hm_capture_word(header + 18, width);
    hm_capture_word(header + 22, height);
    header[26] = 1;
    header[28] = 24;
    hm_capture_word(header + 34, stride * height);
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    int ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    for (u32 y = height; y > 0 && ok; --y) {
        for (u32 i = 0; i < stride; ++i)
            row[i] = 0;
        for (u32 x = 0; x < width; ++x) {
            u16 pixel =
                state->display_enabled ? hm_vram_pixel((u32)area->x + x, (u32)area->y + y - 1) : 0;
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 component = (pixel >> (channel * 5)) & 31;
                row[x * 3 + 2 - channel] = (u8)((component << 3) | (component >> 2));
            }
        }
        ok = fwrite(row, 1, stride, file) == stride;
    }
    return fclose(file) == 0 && ok;
}
