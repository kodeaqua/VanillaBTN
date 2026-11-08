#include "ps1_video.h"

#include <string.h>

_Static_assert(sizeof(uint16_t) == 2, "PS1 VRAM words must be 16 bits");
_Static_assert(sizeof(uint32_t) == 4, "ARGB pixels must be 32 bits");

static bool valid_rect(int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && width > 0 && height > 0
        && width <= PS1_VRAM_WIDTH - x && height <= PS1_VRAM_HEIGHT - y;
}

static bool valid_draw_state(const Ps1DrawState *state) {
    return state != NULL && state->left >= 0 && state->top >= 0
        && state->right < PS1_VRAM_WIDTH && state->bottom < PS1_VRAM_HEIGHT
        && state->left <= state->right && state->top <= state->bottom
        && state->blend_mode <= 3;
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
    /* tpage 0 = texture page X base 0, Y base 0, 4-bit depth, opaque mode. */
    *state = (Ps1DrawState){0, 0, PS1_VRAM_WIDTH - 1, PS1_VRAM_HEIGHT - 1,
                            0, 0, 0, 0, false, false};
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
    if (video == NULL || !valid_draw_state(state)) {
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

bool ps1_video_sample_texel(const Ps1Video *video, unsigned tpage, unsigned clut,
                            unsigned u, unsigned v, uint16_t *texel) {
    if (video == NULL || texel == NULL) return false;

    unsigned depth = (tpage >> 7) & 3u;
    if (depth == 3) return false; /* Reserved PS1 texture depth. */
    unsigned base_x = (tpage & 15u) * 64u;
    unsigned base_y = ((tpage >> 4) & 1u) * 256u;
    unsigned uv_x = u & 255u;
    unsigned uv_y = v & 255u;
    unsigned word_x = (base_x + uv_x / (4u >> depth))
                      & (PS1_VRAM_WIDTH - 1);
    uint16_t word = video->vram[(base_y + uv_y) & (PS1_VRAM_HEIGHT - 1)][word_x];

    if (depth == 2) {
        *texel = word;
        return true;
    }
    unsigned index = depth == 0 ? (word >> ((uv_x & 3u) * 4u)) & 15u
                                : (word >> ((uv_x & 1u) * 8u)) & 255u;
    unsigned clut_x = (((clut & 63u) * 16u) + index) & (PS1_VRAM_WIDTH - 1);
    unsigned clut_y = (clut >> 6) & (PS1_VRAM_HEIGHT - 1);
    *texel = video->vram[clut_y][clut_x];
    return true;
}

static unsigned modulate_channel(unsigned texel, unsigned color) {
    unsigned result = (texel * color) >> 7;
    return result > 31 ? 31 : result;
}

static uint16_t modulate_texel(uint16_t texel, uint32_t rgb24) {
    unsigned red = modulate_channel(texel & 31u, (rgb24 >> 16) & 255u);
    unsigned green = modulate_channel((texel >> 5) & 31u, (rgb24 >> 8) & 255u);
    unsigned blue = modulate_channel((texel >> 10) & 31u, rgb24 & 255u);
    return (uint16_t)(red | (green << 5) | (blue << 10) | (texel & 0x8000u));
}

bool ps1_video_draw_sprite(Ps1Video *video, const Ps1DrawState *state,
                           const Ps1Sprite *sprite) {
    if (video == NULL || !valid_draw_state(state) || sprite == NULL
        || ((sprite->tpage >> 7) & 3u) == 3u) {
        return false;
    }
    int origin_x = signed11(sprite->x) + signed11(state->offset_x);
    int origin_y = signed11(sprite->y) + signed11(state->offset_y);
    int left = origin_x;
    int top = origin_y;
    int right = left + (sprite->width & 0x3FF);
    int bottom = top + (sprite->height & 0x1FF);

    if (left < state->left) left = state->left;
    if (top < state->top) top = state->top;
    if (right > state->right + 1) right = state->right + 1;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;
    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            uint16_t texel;
            ps1_video_sample_texel(video, sprite->tpage, sprite->clut,
                                   sprite->u + (unsigned)(col - origin_x),
                                   sprite->v + (unsigned)(row - origin_y), &texel);
            if (texel == 0) continue; /* Zero texels leave the destination untouched. */
            uint16_t *pixel = &video->vram[row][col];
            if (state->check_mask && (*pixel & 0x8000u)) continue;
            uint16_t front = sprite->raw_texture
                ? texel : modulate_texel(texel, sprite->rgb24);
            if (sprite->semi_transparent && (texel & 0x8000u)) {
                front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
            }
            *pixel = front | (state->force_mask ? 0x8000u : 0);
        }
    }
    return true;
}

static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

static int max3(int a, int b, int c) {
    int m = a > b ? a : b;
    return m > c ? m : c;
}

bool ps1_video_draw_triangle(Ps1Video *video, const Ps1DrawState *state,
                             const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
                             uint32_t rgb24, unsigned tpage, unsigned clut,
                             bool textured, bool raw_texture, bool semi_transparent) {
    if (video == NULL || !valid_draw_state(state) || v0 == NULL || v1 == NULL || v2 == NULL) {
        return false;
    }
    if (textured && ((tpage >> 7) & 3u) == 3u) {
        return false;
    }
    int x0 = signed11(v0->x) + signed11(state->offset_x);
    int y0 = signed11(v0->y) + signed11(state->offset_y);
    int x1 = signed11(v1->x) + signed11(state->offset_x);
    int y1 = signed11(v1->y) + signed11(state->offset_y);
    int x2 = signed11(v2->x) + signed11(state->offset_x);
    int y2 = signed11(v2->y) + signed11(state->offset_y);

    int64_t u0 = v0->u, v_0 = v0->v;
    int64_t u1 = v1->u, v_1 = v1->v;
    int64_t u2 = v2->u, v_2 = v2->v;

    /* 2D cross product: (x1 - x0)*(y2 - y0) - (y1 - y0)*(x2 - x0) */
    int64_t det = (int64_t)(x1 - x0) * (y2 - y0) - (int64_t)(y1 - y0) * (x2 - x0);
    if (det == 0) {
        return true; /* Degenerate triangle, nothing to draw. */
    }
    if (det < 0) {
        /* Swap v1 and v2 so vertices are consistently oriented. */
        int tx = x1; x1 = x2; x2 = tx;
        int ty = y1; y1 = y2; y2 = ty;
        int64_t tu = u1; u1 = u2; u2 = tu;
        int64_t tv = v_1; v_1 = v_2; v_2 = tv;
        det = -det;
    }

    int left = min3(x0, x1, x2);
    int top = min3(y0, y1, y2);
    int right = max3(x0, x1, x2) + 1;
    int bottom = max3(y0, y1, y2) + 1;

    if (left < state->left) left = state->left;
    if (top < state->top) top = state->top;
    if (right > state->right + 1) right = state->right + 1;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;

    uint16_t flat_color = rgb15(rgb24);

    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            int64_t w0 = (int64_t)(x2 - x1) * (row - y1) - (int64_t)(y2 - y1) * (col - x1);
            int64_t w1 = (int64_t)(x0 - x2) * (row - y2) - (int64_t)(y0 - y2) * (col - x2);
            int64_t w2 = (int64_t)(x1 - x0) * (row - y0) - (int64_t)(y1 - y0) * (col - x0);

            /* Include top/left edges only: adjacent triangles share one owner.
             * In particular, a translucent quad must not blend its diagonal twice. */
            bool edge0 = y2 < y1 || (y2 == y1 && x2 > x1);
            bool edge1 = y0 < y2 || (y0 == y2 && x0 > x2);
            bool edge2 = y1 < y0 || (y1 == y0 && x1 > x0);
            if ((w0 > 0 || (w0 == 0 && edge0))
                && (w1 > 0 || (w1 == 0 && edge1))
                && (w2 > 0 || (w2 == 0 && edge2))) {
                uint16_t *pixel = &video->vram[row][col];
                if (state->check_mask && (*pixel & 0x8000u)) continue;

                uint16_t front;
                if (!textured) {
                    front = semi_transparent
                        ? blend_pixel(*pixel, flat_color, state->blend_mode)
                        : flat_color;
                } else {
                    unsigned u = (unsigned)((w0 * u0 + w1 * u1 + w2 * u2) / det);
                    unsigned v = (unsigned)((w0 * v_0 + w1 * v_1 + w2 * v_2) / det);
                    uint16_t texel;
                    if (!ps1_video_sample_texel(video, tpage, clut, u, v, &texel)) {
                        continue;
                    }
                    if (texel == 0) continue;

                    front = raw_texture ? texel : modulate_texel(texel, rgb24);
                    if (semi_transparent && (texel & 0x8000u)) {
                        front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
                    }
                }
                *pixel = front | (state->force_mask ? 0x8000u : 0);
            }
        }
    }
    return true;
}

bool ps1_video_draw_quad(Ps1Video *video, const Ps1DrawState *state,
                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                         uint32_t rgb24, unsigned tpage, unsigned clut,
                         bool textured, bool raw_texture, bool semi_transparent) {
    if (!ps1_video_draw_triangle(video, state, v0, v1, v2, rgb24, tpage, clut,
                                 textured, raw_texture, semi_transparent)) {
        return false;
    }
    return ps1_video_draw_triangle(video, state, v1, v2, v3, rgb24, tpage, clut,
                                   textured, raw_texture, semi_transparent);
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
