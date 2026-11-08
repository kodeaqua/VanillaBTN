#include "ps1_video.h"

#include <string.h>

_Static_assert(sizeof(uint16_t) == 2, "PS1 VRAM words must be 16 bits");
_Static_assert(sizeof(uint32_t) == 4, "ARGB pixels must be 32 bits");

static bool valid_rect(int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && width > 0 && height > 0
        && width <= PS1_VRAM_WIDTH - x && height <= PS1_VRAM_HEIGHT - y;
}

static uint16_t rgb15(uint32_t rgb24) {
    return (uint16_t)(((rgb24 >> 19) & 31u) | ((rgb24 >> 6) & 0x3E0u)
                      | ((rgb24 << 7) & 0x7C00u));
}

static int signed11(int value) {
    value &= 0x7FF;
    return value >= 0x400 ? value - 0x800 : value;
}

static unsigned blend_channel(unsigned back, unsigned front, unsigned mode) {
    switch (mode) {
    case 0: return (back + front) >> 1;
    case 1: return back + front > 31 ? 31 : back + front;
    case 2: return back > front ? back - front : 0;
    default: return back + (front >> 2) > 31 ? 31 : back + (front >> 2);
    }
}

static uint16_t blend_pixel(uint16_t back, uint16_t front, unsigned mode) {
    unsigned red = blend_channel(back & 31u, front & 31u, mode);
    unsigned green = blend_channel((back >> 5) & 31u, (front >> 5) & 31u, mode);
    unsigned blue = blend_channel((back >> 10) & 31u, (front >> 10) & 31u, mode);
    return (uint16_t)(red | (green << 5) | (blue << 10));
}

void ps1_draw_state_reset(Ps1DrawState *state) {
    if (state == NULL) return;
    *state = (Ps1DrawState){0, 0, PS1_VRAM_WIDTH - 1, PS1_VRAM_HEIGHT - 1,
                            0, 0, 0, false, false};
}

void ps1_video_quick_fill(Ps1Video *video, int x, int y, int width, int height,
                          uint32_t rgb24) {
    if (video == NULL) return;
    int start_x = x & 0x3F0;
    int start_y = y & 0x1FF;
    int fill_width = ((width & 0x3FF) + 15) & ~15;
    int fill_height = height & 0x1FF;
    uint16_t color = rgb15(rgb24);

    /* GP0(02) ignores the drawing area and mask setting; coordinates wrap in VRAM. */
    for (int row = 0; row < fill_height; ++row) {
        for (int col = 0; col < fill_width; ++col) {
            video->vram[(start_y + row) & 0x1FF][(start_x + col) & 0x3FF] = color;
        }
    }
}

bool ps1_video_draw_tile(Ps1Video *video, const Ps1DrawState *state, int x, int y,
                         int width, int height, uint32_t rgb24, bool semi_transparent) {
    if (video == NULL || state == NULL || state->left < 0 || state->top < 0
        || state->right >= PS1_VRAM_WIDTH || state->bottom >= PS1_VRAM_HEIGHT
        || state->left > state->right || state->top > state->bottom
        || state->blend_mode > 3) {
        return false;
    }
    int tile_width = width & 0x3FF;
    int tile_height = height & 0x1FF;
    int left = signed11(x) + signed11(state->offset_x);
    int top = signed11(y) + signed11(state->offset_y);
    int right = left + tile_width;
    int bottom = top + tile_height;
    uint16_t color = rgb15(rgb24);

    if (left < state->left) left = state->left;
    if (top < state->top) top = state->top;
    if (right > state->right + 1) right = state->right + 1;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;
    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            uint16_t *pixel = &video->vram[row][col];
            if (state->check_mask && (*pixel & 0x8000u)) continue;
            uint16_t front = semi_transparent
                ? blend_pixel(*pixel, color, state->blend_mode) : color;
            *pixel = front | (state->force_mask ? 0x8000u : 0);
        }
    }
    return true;
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
