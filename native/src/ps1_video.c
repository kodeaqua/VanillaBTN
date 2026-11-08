#include "ps1_video.h"

#include <string.h>

_Static_assert(sizeof(uint16_t) == 2, "PS1 VRAM words must be 16 bits");
_Static_assert(sizeof(uint32_t) == 4, "ARGB pixels must be 32 bits");

static bool valid_rect(int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && width > 0 && height > 0
        && width <= PS1_VRAM_WIDTH - x && height <= PS1_VRAM_HEIGHT - y;
}

bool ps1_video_upload(Ps1Video *video, int x, int y, int width, int height,
                      const uint16_t *source, size_t source_stride) {
    if (video == NULL || source == NULL || !valid_rect(x, y, width, height)
        || source_stride < (size_t)width) {
        return false;
    }
    for (int row = 0; row < height; ++row) {
        memcpy(&video->vram[y + row][x], source + row * source_stride,
               (size_t)width * sizeof(uint16_t));
    }
    return true;
}

bool ps1_video_copy_argb(const Ps1Video *video, int x, int y, int width, int height,
                         uint32_t *target, size_t target_stride) {
    if (video == NULL || target == NULL || !valid_rect(x, y, width, height)
        || target_stride < (size_t)width) {
        return false;
    }
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            uint16_t pixel = video->vram[y + row][x + col];
            uint32_t red = pixel & 31u;
            uint32_t green = (pixel >> 5) & 31u;
            uint32_t blue = (pixel >> 10) & 31u;
            /* Expand five color bits to eight without touching the PS1 mask bit. */
            target[row * target_stride + col] = 0xFF000000u
                | ((red << 3 | red >> 2) << 16)
                | ((green << 3 | green >> 2) << 8)
                | (blue << 3 | blue >> 2);
        }
    }
    return true;
}
