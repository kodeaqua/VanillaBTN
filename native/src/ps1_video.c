#include "ps1_video.h"
#include "ps1_pgxp.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static bool s_true_depth_buffer = true;
static bool s_near_plane_w_clipping = true;
static bool s_quad_subdivision = true;
static bool s_true_color_32bit = true;
static Ps1DitherMode s_dithering = PS1_DITHER_OFF;

#define PS1_NEAR_PLANE_Z 8.0f

static const int s_dither_matrix[4][4] = {
    { -4,  0, -3,  1 },
    {  2, -2,  3, -1 },
    { -1,  3, -2,  2 },
    {  1, -3,  0, -4 }
};

static uint32_t blend_color32(uint32_t back, uint32_t front, unsigned blend_mode) {
    uint32_t br = (back >> 16) & 255u;
    uint32_t bg = (back >> 8) & 255u;
    uint32_t bb = back & 255u;

    uint32_t fr = (front >> 16) & 255u;
    uint32_t fg = (front >> 8) & 255u;
    uint32_t fb = front & 255u;

    uint32_t r, g, b;
    switch (blend_mode) {
    case 0: /* 0.5 * Back + 0.5 * Front */
        r = (br + fr) >> 1;
        g = (bg + fg) >> 1;
        b = (bb + fb) >> 1;
        break;
    case 1: /* Back + Front */
        r = br + fr; if (r > 255) r = 255;
        g = bg + fg; if (g > 255) g = 255;
        b = bb + fb; if (b > 255) b = 255;
        break;
    case 2: /* Back - Front */
        r = (br > fr) ? br - fr : 0;
        g = (bg > fg) ? bg - fg : 0;
        b = (bb > fb) ? bb - fb : 0;
        break;
    case 3: /* Back + 0.25 * Front */
        r = br + (fr >> 2); if (r > 255) r = 255;
        g = bg + (fg >> 2); if (g > 255) g = 255;
        b = bb + (fb >> 2); if (b > 255) b = 255;
        break;
    default:
        r = fr; g = fg; b = fb;
        break;
    }
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static uint32_t texel_to_color32(uint16_t texel, uint32_t rgb24, bool raw_texture) {
    uint32_t red5 = texel & 31u;
    uint32_t green5 = (texel >> 5) & 31u;
    uint32_t blue5 = (texel >> 10) & 31u;

    if (raw_texture) {
        uint32_t r8 = (red5 << 3) | (red5 >> 2);
        uint32_t g8 = (green5 << 3) | (green5 >> 2);
        uint32_t b8 = (blue5 << 3) | (blue5 >> 2);
        return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
    } else {
        uint32_t cr = (rgb24 >> 16) & 255u;
        uint32_t cg = (rgb24 >> 8) & 255u;
        uint32_t cb = rgb24 & 255u;

        uint32_t r8 = (red5 * cr) / 16u;
        uint32_t g8 = (green5 * cg) / 16u;
        uint32_t b8 = (blue5 * cb) / 16u;
        if (r8 > 255) r8 = 255;
        if (g8 > 255) g8 = 255;
        if (b8 > 255) b8 = 255;
        return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
    }
}

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

static uint16_t quantize_rgb24(uint32_t rgb24, int row, int col, bool dither_enabled) {
    if (dither_enabled && s_dithering == PS1_DITHER_NATIVE) {
        int d = s_dither_matrix[row & 3][col & 3];
        int r = (int)((rgb24 >> 16) & 255u) + d;
        int g = (int)((rgb24 >> 8) & 255u) + d;
        int b = (int)(rgb24 & 255u) + d;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        return (uint16_t)(((r >> 3) & 31u) | (((g >> 3) & 31u) << 5) | (((b >> 3) & 31u) << 10));
    }
    return rgb15(rgb24);
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
                            0, 0, 0, 0, false, false, false};
}

void ps1_video_quick_fill(Ps1Video *video, int x, int y, int width, int height,
                          uint32_t rgb24) {
    if (video == NULL) return;
    int start_x = x & 0x3F0;
    int start_y = y & 0x1FF;
    int fill_width = ((width & 0x3FF) + 15) & ~15;
    int fill_height = height & 0x1FF;
    uint16_t color = rgb15(rgb24);
    uint32_t c32 = 0xFF000000u | (rgb24 & 0xFFFFFFu);

    /* GP0(02) ignores the drawing area and mask setting; coordinates wrap in VRAM. */
    for (int row = 0; row < fill_height; ++row) {
        for (int col = 0; col < fill_width; ++col) {
            int r = (start_y + row) & 0x1FF;
            int c = (start_x + col) & 0x3FF;
            video->vram[r][c] = color;
            video->zbuffer[r][c] = 0.0f;
            video->color32[r][c] = c32;
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
    uint32_t c32_solid = 0xFF000000u | (rgb24 & 0xFFFFFFu);

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

            uint32_t c32 = semi_transparent
                ? blend_color32(video->color32[row][col], c32_solid, state->blend_mode) : c32_solid;
            video->color32[row][col] = c32;
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

            uint32_t front_c32 = texel_to_color32(texel, sprite->rgb24, sprite->raw_texture);
            if (sprite->semi_transparent && (texel & 0x8000u)) {
                front_c32 = blend_color32(video->color32[row][col], front_c32, state->blend_mode);
            }
            video->color32[row][col] = front_c32;
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

static void intersect_near_plane(const Ps1Vertex *a, const Ps1Vertex *b,
                                 uint32_t ca, uint32_t cb,
                                 Ps1Vertex *out_v, uint32_t *out_c) {
    float za = a->z;
    float zb = b->z;
    float denom = zb - za;
    float t = (fabsf(denom) > 1e-6f) ? (PS1_NEAR_PLANE_Z - za) / denom : 0.5f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float z_near = PS1_NEAR_PLANE_Z;
    float w_near = 1.0f / z_near;

    out_v->z = z_near;
    out_v->w = w_near;
    out_v->has_subpixel = true;

    float ofx = 160.0f;
    float ofy = 120.0f;
    float xh_a = (a->sub_x - ofx) * a->z;
    float yh_a = (a->sub_y - ofy) * a->z;
    float xh_b = (b->sub_x - ofx) * b->z;
    float yh_b = (b->sub_y - ofy) * b->z;

    float xh_i = (1.0f - t) * xh_a + t * xh_b;
    float yh_i = (1.0f - t) * yh_a + t * yh_b;

    out_v->sub_x = ofx + xh_i * w_near;
    out_v->sub_y = ofy + yh_i * w_near;

    if (out_v->sub_x < -1024.0f) out_v->sub_x = -1024.0f;
    else if (out_v->sub_x > 1023.0f) out_v->sub_x = 1023.0f;
    if (out_v->sub_y < -1024.0f) out_v->sub_y = -1024.0f;
    else if (out_v->sub_y > 1023.0f) out_v->sub_y = 1023.0f;

    out_v->x = (int)roundf(out_v->sub_x);
    out_v->y = (int)roundf(out_v->sub_y);

    float uw_a = (float)a->u * a->w;
    float vw_a = (float)a->v * a->w;
    float uw_b = (float)b->u * b->w;
    float vw_b = (float)b->v * b->w;

    float uw_i = (1.0f - t) * uw_a + t * uw_b;
    float vw_i = (1.0f - t) * vw_a + t * vw_b;

    float u_val = uw_i / w_near;
    float v_val = vw_i / w_near;
    out_v->u = (unsigned)(u_val < 0.0f ? 0.0f : (u_val > 255.0f ? 255.0f : roundf(u_val)));
    out_v->v = (unsigned)(v_val < 0.0f ? 0.0f : (v_val > 255.0f ? 255.0f : roundf(v_val)));

    if (out_c) {
        float rw_a = (float)((ca >> 16) & 255) * a->w;
        float gw_a = (float)((ca >> 8) & 255) * a->w;
        float bw_a = (float)(ca & 255) * a->w;

        float rw_b = (float)((cb >> 16) & 255) * b->w;
        float gw_b = (float)((cb >> 8) & 255) * b->w;
        float bw_b = (float)(cb & 255) * b->w;

        float rw_i = (1.0f - t) * rw_a + t * rw_b;
        float gw_i = (1.0f - t) * gw_a + t * gw_b;
        float bw_i = (1.0f - t) * bw_a + t * bw_b;

        uint32_t r = (uint32_t)roundf(rw_i / w_near);
        uint32_t g = (uint32_t)roundf(gw_i / w_near);
        uint32_t b = (uint32_t)roundf(bw_i / w_near);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        *out_c = (r << 16) | (g << 8) | b;
    }
}

static int clip_triangle_near_plane(const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
                                    const uint32_t *colors,
                                    Ps1Vertex out_v[2][3], uint32_t out_c[2][3]) {
    const Ps1Vertex *inv[3] = {v0, v1, v2};
    uint32_t inc[3] = {colors ? colors[0] : 0, colors ? colors[1] : 0, colors ? colors[2] : 0};

    bool inside[3] = {
        v0->z >= PS1_NEAR_PLANE_Z,
        v1->z >= PS1_NEAR_PLANE_Z,
        v2->z >= PS1_NEAR_PLANE_Z
    };
    int inside_count = (inside[0] ? 1 : 0) + (inside[1] ? 1 : 0) + (inside[2] ? 1 : 0);

    if (inside_count == 3) {
        out_v[0][0] = *v0; out_v[0][1] = *v1; out_v[0][2] = *v2;
        if (colors) { out_c[0][0] = inc[0]; out_c[0][1] = inc[1]; out_c[0][2] = inc[2]; }
        return 1;
    }
    if (inside_count == 0) {
        return 0;
    }

    if (inside_count == 1) {
        int i0 = inside[0] ? 0 : (inside[1] ? 1 : 2);
        int i1 = (i0 + 1) % 3;
        int i2 = (i0 + 2) % 3;

        Ps1Vertex p1, p2;
        uint32_t c1 = 0, c2 = 0;
        intersect_near_plane(inv[i0], inv[i1], inc[i0], inc[i1], &p1, colors ? &c1 : NULL);
        intersect_near_plane(inv[i2], inv[i0], inc[i2], inc[i0], &p2, colors ? &c2 : NULL);

        out_v[0][0] = *inv[i0];
        out_v[0][1] = p1;
        out_v[0][2] = p2;
        if (colors) {
            out_c[0][0] = inc[i0];
            out_c[0][1] = c1;
            out_c[0][2] = c2;
        }
        return 1;
    }

    int o = !inside[0] ? 0 : (!inside[1] ? 1 : 2);
    int i1 = (o + 1) % 3;
    int i2 = (o + 2) % 3;

    Ps1Vertex p1, p2;
    uint32_t c1 = 0, c2 = 0;
    intersect_near_plane(inv[i2], inv[o], inc[i2], inc[o], &p1, colors ? &c1 : NULL);
    intersect_near_plane(inv[o], inv[i1], inc[o], inc[i1], &p2, colors ? &c2 : NULL);

    out_v[0][0] = *inv[i1];
    out_v[0][1] = *inv[i2];
    out_v[0][2] = p1;
    if (colors) {
        out_c[0][0] = inc[i1];
        out_c[0][1] = inc[i2];
        out_c[0][2] = c1;
    }

    out_v[1][0] = *inv[i1];
    out_v[1][1] = p1;
    out_v[1][2] = p2;
    if (colors) {
        out_c[1][0] = inc[i1];
        out_c[1][1] = c1;
        out_c[1][2] = c2;
    }
    return 2;
}

static bool draw_triangle(Ps1Video *video, const Ps1DrawState *state,
                             const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
                             uint32_t rgb24, unsigned tpage, unsigned clut,
                             bool textured, bool raw_texture, bool semi_transparent,
                             const uint32_t *colors) {
    if (video == NULL || !valid_draw_state(state) || v0 == NULL || v1 == NULL || v2 == NULL) {
        return false;
    }
    if (textured && ((tpage >> 7) & 3u) == 3u) {
        return false;
    }

    if (s_near_plane_w_clipping && v0->has_subpixel && v1->has_subpixel && v2->has_subpixel) {
        if (v0->z < PS1_NEAR_PLANE_Z || v1->z < PS1_NEAR_PLANE_Z || v2->z < PS1_NEAR_PLANE_Z) {
            Ps1Vertex clipped_v[2][3];
            uint32_t clipped_c[2][3];
            int count = clip_triangle_near_plane(v0, v1, v2, colors, clipped_v, clipped_c);
            if (count == 0) return true;
            for (int i = 0; i < count; ++i) {
                if (!draw_triangle(video, state, &clipped_v[i][0], &clipped_v[i][1], &clipped_v[i][2],
                                   rgb24, tpage, clut, textured, raw_texture, semi_transparent,
                                   colors ? clipped_c[i] : NULL)) {
                    return false;
                }
            }
            return true;
        }
    }

    bool use_subpixel = ps1_pgxp_is_vertex_precision_enabled()
                     && v0->has_subpixel && v1->has_subpixel && v2->has_subpixel;

    if (use_subpixel) {
        float ox = (float)signed11(state->offset_x);
        float oy = (float)signed11(state->offset_y);
        float fx0 = v0->sub_x + ox;
        float fy0 = v0->sub_y + oy;
        float fx1 = v1->sub_x + ox;
        float fy1 = v1->sub_y + oy;
        float fx2 = v2->sub_x + ox;
        float fy2 = v2->sub_y + oy;

        float fu0 = (float)v0->u, fv0 = (float)v0->v;
        float fu1 = (float)v1->u, fv1 = (float)v1->v;
        float fu2 = (float)v2->u, fv2 = (float)v2->v;
        float fw0 = (v0->w > 0.0f) ? v0->w : 1.0f;
        float fw1 = (v1->w > 0.0f) ? v1->w : 1.0f;
        float fw2 = (v2->w > 0.0f) ? v2->w : 1.0f;
        uint32_t c0 = colors ? colors[0] : rgb24;
        uint32_t c1 = colors ? colors[1] : rgb24;
        uint32_t c2 = colors ? colors[2] : rgb24;

        double fdet = (double)(fx1 - fx0) * (double)(fy2 - fy0) - (double)(fy1 - fy0) * (double)(fx2 - fx0);
        if (fabs(fdet) < 1e-5) {
            return true;
        }
        if (fdet < 0.0) {
            float tx = fx1; fx1 = fx2; fx2 = tx;
            float ty = fy1; fy1 = fy2; fy2 = ty;
            float tu = fu1; fu1 = fu2; fu2 = tu;
            float tv = fv1; fv1 = fv2; fv2 = tv;
            float tw = fw1; fw1 = fw2; fw2 = tw;
            uint32_t tc = c1; c1 = c2; c2 = tc;
            fdet = -fdet;
        }

        int left = (int)floor(fmin(fx0, fmin(fx1, fx2)));
        int top = (int)floor(fmin(fy0, fmin(fy1, fy2)));
        int right = (int)ceil(fmax(fx0, fmax(fx1, fx2))) + 1;
        int bottom = (int)ceil(fmax(fy0, fmax(fy1, fy2))) + 1;

        if (left < state->left) left = state->left;
        if (top < state->top) top = state->top;
        if (right > state->right + 1) right = state->right + 1;
        if (bottom > state->bottom + 1) bottom = state->bottom + 1;

        bool use_perspective = textured && ps1_pgxp_is_perspective_texture_enabled()
                            && (v0->w > 0.0f && v1->w > 0.0f && v2->w > 0.0f);

        float pu0 = fu0 * fw0, pv0 = fv0 * fw0;
        float pu1 = fu1 * fw1, pv1 = fv1 * fw1;
        float pu2 = fu2 * fw2, pv2 = fv2 * fw2;

        uint16_t flat_color = rgb15(rgb24);
        double inv_det = 1.0 / fdet;

        bool edge0 = (fy2 < fy1) || (fy2 == fy1 && fx2 > fx1);
        bool edge1 = (fy0 < fy2) || (fy0 == fy2 && fx0 > fx2);
        bool edge2 = (fy1 < fy0) || (fy1 == fy0 && fx1 > fx0);

        for (int row = top; row < bottom; ++row) {
            for (int col = left; col < right; ++col) {
                double px = (double)col + 0.5;
                double py = (double)row + 0.5;

                double w0 = (double)(fx2 - fx1) * (py - (double)fy1) - (double)(fy2 - fy1) * (px - (double)fx1);
                double w1 = (double)(fx0 - fx2) * (py - (double)fy2) - (double)(fy0 - fy2) * (px - (double)fx2);
                double w2 = (double)(fx1 - fx0) * (py - (double)fy0) - (double)(fy1 - fy0) * (px - (double)fx0);

                if ((w0 > 0.0 || (w0 >= -1e-6 && edge0))
                    && (w1 > 0.0 || (w1 >= -1e-6 && edge1))
                    && (w2 > 0.0 || (w2 >= -1e-6 && edge2))) {
                    uint16_t *pixel = &video->vram[row][col];
                    if (state->check_mask && (*pixel & 0x8000u)) continue;

                    double l0 = w0 * inv_det;
                    double l1 = w1 * inv_det;
                    double l2 = w2 * inv_det;

                    double interp_w = l0 * (double)fw0 + l1 * (double)fw1 + l2 * (double)fw2;

                    if (s_true_depth_buffer) {
                        float w_test = (float)interp_w;
                        if (semi_transparent) {
                            /* Apply shadow / decal depth bias: slightly closer to camera (larger W) */
                            w_test = w_test * 1.002f + 1e-5f;
                        }
                        if (w_test < video->zbuffer[row][col] - 1e-6f) {
                            continue; /* Occluded by nearer geometry! */
                        }
                    }

                    uint32_t shade = rgb24;
                    if (colors) {
                        shade = 0;
                        for (unsigned shift = 0; shift < 24; shift += 8) {
                            uint32_t ch = (uint32_t)(l0 * (double)((c0 >> shift) & 255)
                                                   + l1 * (double)((c1 >> shift) & 255)
                                                   + l2 * (double)((c2 >> shift) & 255) + 0.5);
                            if (ch > 255) ch = 255;
                            shade |= ch << shift;
                        }
                    }

                    uint16_t front;
                    uint32_t front32;
                    if (!textured) {
                        uint16_t color = colors
                            ? quantize_rgb24(shade, row, col, state->dither)
                            : (state->dither && s_dithering == PS1_DITHER_NATIVE ? quantize_rgb24(rgb24, row, col, true) : flat_color);
                        front = semi_transparent
                            ? blend_pixel(*pixel, color, state->blend_mode)
                            : color;
                        uint32_t c32_poly = 0xFF000000u | (shade & 0xFFFFFFu);
                        front32 = semi_transparent
                            ? blend_color32(video->color32[row][col], c32_poly, state->blend_mode)
                            : c32_poly;
                    } else {
                        unsigned u, v;
                        if (use_perspective) {
                            if (interp_w > 1e-9) {
                                double inv_w = 1.0 / interp_w;
                                double u_p = (l0 * (double)pu0 + l1 * (double)pu1 + l2 * (double)pu2) * inv_w;
                                double v_p = (l0 * (double)pv0 + l1 * (double)pv1 + l2 * (double)pv2) * inv_w;
                                int iu = (int)(u_p + 0.5);
                                int iv = (int)(v_p + 0.5);
                                u = (iu < 0) ? 0 : (iu > 255) ? 255 : (unsigned)iu;
                                v = (iv < 0) ? 0 : (iv > 255) ? 255 : (unsigned)iv;
                            } else {
                                int iu = (int)(l0 * (double)fu0 + l1 * (double)fu1 + l2 * (double)fu2 + 0.5);
                                int iv = (int)(l0 * (double)fv0 + l1 * (double)fv1 + l2 * (double)fv2 + 0.5);
                                u = (iu < 0) ? 0 : (iu > 255) ? 255 : (unsigned)iu;
                                v = (iv < 0) ? 0 : (iv > 255) ? 255 : (unsigned)iv;
                            }
                        } else {
                            int iu = (int)(l0 * (double)fu0 + l1 * (double)fu1 + l2 * (double)fu2 + 0.5);
                            int iv = (int)(l0 * (double)fv0 + l1 * (double)fv1 + l2 * (double)fv2 + 0.5);
                            u = (iu < 0) ? 0 : (iu > 255) ? 255 : (unsigned)iu;
                            v = (iv < 0) ? 0 : (iv > 255) ? 255 : (unsigned)iv;
                        }

                        uint16_t texel;
                        if (!ps1_video_sample_texel(video, tpage, clut, u, v, &texel)) {
                            continue;
                        }
                        if (texel == 0) continue;

                        front = raw_texture ? texel : modulate_texel(texel, shade);
                        if (semi_transparent && (texel & 0x8000u)) {
                            front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
                        }

                        front32 = texel_to_color32(texel, shade, raw_texture);
                        if (semi_transparent && (texel & 0x8000u)) {
                            front32 = blend_color32(video->color32[row][col], front32, state->blend_mode);
                        }
                    }
                    *pixel = front | (state->force_mask ? 0x8000u : 0);
                    video->color32[row][col] = front32;
                    if (s_true_depth_buffer && !semi_transparent) {
                        video->zbuffer[row][col] = (float)interp_w;
                    }
                }
            }
        }
        return true;
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
    uint32_t c0 = colors ? colors[0] : rgb24;
    uint32_t c1 = colors ? colors[1] : rgb24;
    uint32_t c2 = colors ? colors[2] : rgb24;

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
        uint32_t tc = c1; c1 = c2; c2 = tc;
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

                /* Interpolate RGB before either direct output or texture modulation. */
                uint32_t shade = rgb24;
                if (colors) {
                    shade = 0;
                    for (unsigned shift = 0; shift < 24; shift += 8) {
                        uint32_t channel = (uint32_t)((w0 * ((c0 >> shift) & 255)
                            + w1 * ((c1 >> shift) & 255) + w2 * ((c2 >> shift) & 255)) / det);
                        shade |= channel << shift;
                    }
                }
                uint16_t front;
                uint32_t front32;
                if (!textured) {
                    uint16_t color = colors
                        ? quantize_rgb24(shade, row, col, state->dither)
                        : (state->dither && s_dithering == PS1_DITHER_NATIVE ? quantize_rgb24(rgb24, row, col, true) : flat_color);
                    front = semi_transparent
                        ? blend_pixel(*pixel, color, state->blend_mode)
                        : color;
                    uint32_t c32_poly = 0xFF000000u | (shade & 0xFFFFFFu);
                    front32 = semi_transparent
                        ? blend_color32(video->color32[row][col], c32_poly, state->blend_mode)
                        : c32_poly;
                } else {
                    unsigned u = (unsigned)((w0 * u0 + w1 * u1 + w2 * u2) / det);
                    unsigned v = (unsigned)((w0 * v_0 + w1 * v_1 + w2 * v_2) / det);
                    uint16_t texel;
                    if (!ps1_video_sample_texel(video, tpage, clut, u, v, &texel)) {
                        continue;
                    }
                    if (texel == 0) continue;

                    front = raw_texture ? texel : modulate_texel(texel, shade);
                    if (semi_transparent && (texel & 0x8000u)) {
                        front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
                    }

                    front32 = texel_to_color32(texel, shade, raw_texture);
                    if (semi_transparent && (texel & 0x8000u)) {
                        front32 = blend_color32(video->color32[row][col], front32, state->blend_mode);
                    }
                }
                *pixel = front | (state->force_mask ? 0x8000u : 0);
                video->color32[row][col] = front32;
            }
        }
    }
    return true;
}

void ps1_video_set_pgxp_vertex_precision(bool enabled) {
    ps1_pgxp_set_enabled(enabled, ps1_pgxp_is_perspective_texture_enabled());
}

void ps1_video_set_pgxp_perspective_texture(bool enabled) {
    ps1_pgxp_set_enabled(ps1_pgxp_is_vertex_precision_enabled(), enabled);
}

bool ps1_video_get_pgxp_vertex_precision(void) {
    return ps1_pgxp_is_vertex_precision_enabled();
}

bool ps1_video_get_pgxp_perspective_texture(void) {
    return ps1_pgxp_is_perspective_texture_enabled();
}

bool ps1_video_draw_triangle(Ps1Video *video, const Ps1DrawState *state,
    const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
    uint32_t rgb24, unsigned tpage, unsigned clut,
    bool textured, bool raw_texture, bool semi_transparent) {
    return draw_triangle(video, state, v0, v1, v2, rgb24, tpage, clut,
                         textured, raw_texture, semi_transparent, NULL);
}

bool ps1_video_draw_gouraud_triangle(Ps1Video *video, const Ps1DrawState *state,
    const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
    const uint32_t colors[3], bool semi_transparent) {
    if (!colors) return false;
    return draw_triangle(video, state, v0, v1, v2, 0, 0, 0, false, false, semi_transparent, colors);
}

bool ps1_video_draw_textured_gouraud_triangle(Ps1Video *video,
    const Ps1DrawState *state, const Ps1Vertex *v0, const Ps1Vertex *v1,
    const Ps1Vertex *v2, const uint32_t colors[3], unsigned tpage, unsigned clut,
    bool raw_texture, bool semi_transparent) {
    if (!colors) return false;
    return draw_triangle(video, state, v0, v1, v2, 0, tpage, clut,
        true, raw_texture, semi_transparent, colors);
}

bool ps1_video_draw_line(Ps1Video *video, const Ps1DrawState *state,
    int x0, int y0, int x1, int y1, uint32_t rgb24, bool semi_transparent) {
    if (!video || !valid_draw_state(state)) return false;
    x0 = signed11(x0) + signed11(state->offset_x);
    y0 = signed11(y0) + signed11(state->offset_y);
    x1 = signed11(x1) + signed11(state->offset_x);
    y1 = signed11(y1) + signed11(state->offset_y);
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    uint16_t color = rgb15(rgb24);
    uint32_t c32_solid = 0xFF000000u | (rgb24 & 0xFFFFFFu);
    for (;;) {
        if (x0 >= state->left && x0 <= state->right && y0 >= state->top && y0 <= state->bottom) {
            uint16_t *pixel = &video->vram[y0][x0];
            if (!state->check_mask || !(*pixel & 0x8000)) {
                *pixel = (semi_transparent ? blend_pixel(*pixel, color, state->blend_mode) : color)
                    | (state->force_mask ? 0x8000 : 0);
                uint32_t c32 = semi_transparent
                    ? blend_color32(video->color32[y0][x0], c32_solid, state->blend_mode) : c32_solid;
                video->color32[y0][x0] = c32;
            }
        }
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
    return true;
}

static Ps1Vertex lerp_vertex(const Ps1Vertex *a, const Ps1Vertex *b, float t) {
    Ps1Vertex out;
    out.sub_x = (1.0f - t) * a->sub_x + t * b->sub_x;
    out.sub_y = (1.0f - t) * a->sub_y + t * b->sub_y;
    out.x = (int)roundf(out.sub_x);
    out.y = (int)roundf(out.sub_y);
    out.u = (unsigned)roundf((1.0f - t) * (float)a->u + t * (float)b->u);
    out.v = (unsigned)roundf((1.0f - t) * (float)a->v + t * (float)b->v);
    out.z = (1.0f - t) * a->z + t * b->z;
    out.w = (1.0f - t) * a->w + t * b->w;
    out.has_subpixel = a->has_subpixel && b->has_subpixel;
    return out;
}

static uint32_t lerp_color(uint32_t c0, uint32_t c1, float t) {
    uint32_t r = (uint32_t)roundf((1.0f - t) * ((c0 >> 16) & 255) + t * ((c1 >> 16) & 255));
    uint32_t g = (uint32_t)roundf((1.0f - t) * ((c0 >> 8) & 255) + t * ((c1 >> 8) & 255));
    uint32_t b = (uint32_t)roundf((1.0f - t) * (c0 & 255) + t * (c1 & 255));
    return (r << 16) | (g << 8) | b;
}

static bool should_subdivide_quad(const Ps1Vertex *v0, const Ps1Vertex *v1,
                                  const Ps1Vertex *v2, const Ps1Vertex *v3) {
    if (!s_quad_subdivision) return false;
    if (!v0->has_subpixel || !v1->has_subpixel || !v2->has_subpixel || !v3->has_subpixel) {
        return false;
    }
    float min_x = fminf(fminf(v0->sub_x, v1->sub_x), fminf(v2->sub_x, v3->sub_x));
    float max_x = fmaxf(fmaxf(v0->sub_x, v1->sub_x), fmaxf(v2->sub_x, v3->sub_x));
    float min_y = fminf(fminf(v0->sub_y, v1->sub_y), fminf(v2->sub_y, v3->sub_y));
    float max_y = fmaxf(fmaxf(v0->sub_y, v1->sub_y), fmaxf(v2->sub_y, v3->sub_y));

    float w = max_x - min_x;
    float h = max_y - min_y;
    if (w > 24.0f || h > 24.0f) return true;

    float dz1 = fabsf(v0->z - v3->z);
    float dz2 = fabsf(v1->z - v2->z);
    return (dz1 > 15.0f || dz2 > 15.0f);
}

bool ps1_video_draw_quad(Ps1Video *video, const Ps1DrawState *state,
                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                         uint32_t rgb24, unsigned tpage, unsigned clut,
                         bool textured, bool raw_texture, bool semi_transparent) {
    if (should_subdivide_quad(v0, v1, v2, v3)) {
        Ps1Vertex m01 = lerp_vertex(v0, v1, 0.5f);
        Ps1Vertex m23 = lerp_vertex(v2, v3, 0.5f);
        Ps1Vertex m02 = lerp_vertex(v0, v2, 0.5f);
        Ps1Vertex m13 = lerp_vertex(v1, v3, 0.5f);
        Ps1Vertex mmid = lerp_vertex(&m01, &m23, 0.5f);

        const Ps1Vertex *quads[4][4] = {
            { v0,   &m01, &m02, &mmid },
            { &m01, v1,   &mmid, &m13 },
            { &m02, &mmid, v2,   &m23 },
            { &mmid, &m13, &m23, v3   }
        };
        for (int q = 0; q < 4; ++q) {
            if (!ps1_video_draw_triangle(video, state, quads[q][0], quads[q][1], quads[q][2],
                                         rgb24, tpage, clut, textured, raw_texture, semi_transparent))
                return false;
            if (!ps1_video_draw_triangle(video, state, quads[q][1], quads[q][2], quads[q][3],
                                         rgb24, tpage, clut, textured, raw_texture, semi_transparent))
                return false;
        }
        return true;
    }

    if (!ps1_video_draw_triangle(video, state, v0, v1, v2, rgb24, tpage, clut,
                                 textured, raw_texture, semi_transparent)) {
        return false;
    }
    return ps1_video_draw_triangle(video, state, v1, v2, v3, rgb24, tpage, clut,
                                   textured, raw_texture, semi_transparent);
}

bool ps1_video_draw_gouraud_quad(Ps1Video *video, const Ps1DrawState *state,
                                const Ps1Vertex *v0, const Ps1Vertex *v1,
                                const Ps1Vertex *v2, const Ps1Vertex *v3,
                                const uint32_t colors[4], bool semi_transparent) {
    if (!colors) return false;
    if (should_subdivide_quad(v0, v1, v2, v3)) {
        Ps1Vertex m01 = lerp_vertex(v0, v1, 0.5f);
        Ps1Vertex m23 = lerp_vertex(v2, v3, 0.5f);
        Ps1Vertex m02 = lerp_vertex(v0, v2, 0.5f);
        Ps1Vertex m13 = lerp_vertex(v1, v3, 0.5f);
        Ps1Vertex mmid = lerp_vertex(&m01, &m23, 0.5f);

        uint32_t c01 = lerp_color(colors[0], colors[1], 0.5f);
        uint32_t c23 = lerp_color(colors[2], colors[3], 0.5f);
        uint32_t c02 = lerp_color(colors[0], colors[2], 0.5f);
        uint32_t c13 = lerp_color(colors[1], colors[3], 0.5f);
        uint32_t cmid = lerp_color(c01, c23, 0.5f);

        const Ps1Vertex *quads[4][4] = {
            { v0,   &m01, &m02, &mmid },
            { &m01, v1,   &mmid, &m13 },
            { &m02, &mmid, v2,   &m23 },
            { &mmid, &m13, &m23, v3   }
        };
        uint32_t qcolors[4][4] = {
            { colors[0], c01, c02, cmid },
            { c01, colors[1], cmid, c13 },
            { c02, cmid, colors[2], c23 },
            { cmid, c13, c23, colors[3] }
        };
        for (int q = 0; q < 4; ++q) {
            uint32_t tri_c0[3] = { qcolors[q][0], qcolors[q][1], qcolors[q][2] };
            uint32_t tri_c1[3] = { qcolors[q][1], qcolors[q][2], qcolors[q][3] };
            if (!ps1_video_draw_gouraud_triangle(video, state, quads[q][0], quads[q][1], quads[q][2],
                                                 tri_c0, semi_transparent))
                return false;
            if (!ps1_video_draw_gouraud_triangle(video, state, quads[q][1], quads[q][2], quads[q][3],
                                                 tri_c1, semi_transparent))
                return false;
        }
        return true;
    }

    uint32_t c_first[3] = { colors[0], colors[1], colors[2] };
    uint32_t c_second[3] = { colors[1], colors[2], colors[3] };
    if (!ps1_video_draw_gouraud_triangle(video, state, v0, v1, v2, c_first, semi_transparent))
        return false;
    return ps1_video_draw_gouraud_triangle(video, state, v1, v2, v3, c_second, semi_transparent);
}

bool ps1_video_draw_textured_gouraud_quad(Ps1Video *video, const Ps1DrawState *state,
                                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                                         const uint32_t colors[4], unsigned tpage, unsigned clut,
                                         bool raw_texture, bool semi_transparent) {
    if (!colors) return false;
    if (should_subdivide_quad(v0, v1, v2, v3)) {
        Ps1Vertex m01 = lerp_vertex(v0, v1, 0.5f);
        Ps1Vertex m23 = lerp_vertex(v2, v3, 0.5f);
        Ps1Vertex m02 = lerp_vertex(v0, v2, 0.5f);
        Ps1Vertex m13 = lerp_vertex(v1, v3, 0.5f);
        Ps1Vertex mmid = lerp_vertex(&m01, &m23, 0.5f);

        uint32_t c01 = lerp_color(colors[0], colors[1], 0.5f);
        uint32_t c23 = lerp_color(colors[2], colors[3], 0.5f);
        uint32_t c02 = lerp_color(colors[0], colors[2], 0.5f);
        uint32_t c13 = lerp_color(colors[1], colors[3], 0.5f);
        uint32_t cmid = lerp_color(c01, c23, 0.5f);

        const Ps1Vertex *quads[4][4] = {
            { v0,   &m01, &m02, &mmid },
            { &m01, v1,   &mmid, &m13 },
            { &m02, &mmid, v2,   &m23 },
            { &mmid, &m13, &m23, v3   }
        };
        uint32_t qcolors[4][4] = {
            { colors[0], c01, c02, cmid },
            { c01, colors[1], cmid, c13 },
            { c02, cmid, colors[2], c23 },
            { cmid, c13, c23, colors[3] }
        };
        for (int q = 0; q < 4; ++q) {
            uint32_t tri_c0[3] = { qcolors[q][0], qcolors[q][1], qcolors[q][2] };
            uint32_t tri_c1[3] = { qcolors[q][1], qcolors[q][2], qcolors[q][3] };
            if (!ps1_video_draw_textured_gouraud_triangle(video, state, quads[q][0], quads[q][1], quads[q][2],
                                                          tri_c0, tpage, clut, raw_texture, semi_transparent))
                return false;
            if (!ps1_video_draw_textured_gouraud_triangle(video, state, quads[q][1], quads[q][2], quads[q][3],
                                                          tri_c1, tpage, clut, raw_texture, semi_transparent))
                return false;
        }
        return true;
    }

    uint32_t c_first[3] = { colors[0], colors[1], colors[2] };
    uint32_t c_second[3] = { colors[1], colors[2], colors[3] };
    if (!ps1_video_draw_textured_gouraud_triangle(video, state, v0, v1, v2, c_first, tpage, clut,
                                                  raw_texture, semi_transparent))
        return false;
    return ps1_video_draw_textured_gouraud_triangle(video, state, v1, v2, v3, c_second, tpage, clut,
                                                    raw_texture, semi_transparent);
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
        for (int col = 0; col < width; ++col) {
            uint16_t pixel = source[row * source_stride + col];
            uint32_t red = pixel & 31u;
            uint32_t green = (pixel >> 5) & 31u;
            uint32_t blue = (pixel >> 10) & 31u;
            video->color32[y + row][x + col] = 0xFF000000u
                | ((red << 3 | red >> 2) << 16)
                | ((green << 3 | green >> 2) << 8)
                | (blue << 3 | blue >> 2);
        }
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
            if (s_true_color_32bit && video->color32[y + row][x + col] != 0) {
                target[row * target_stride + col] = video->color32[y + row][x + col];
            } else {
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
    }
    return true;
}

void ps1_video_set_true_depth_buffer(bool enabled) {
    s_true_depth_buffer = enabled;
}

bool ps1_video_get_true_depth_buffer(void) {
    return s_true_depth_buffer;
}

void ps1_video_clear_zbuffer(Ps1Video *video) {
    if (video == NULL) return;
    memset(video->zbuffer, 0, sizeof(video->zbuffer));
}

void ps1_video_set_near_plane_clipping(bool enabled) {
    s_near_plane_w_clipping = enabled;
}

bool ps1_video_get_near_plane_clipping(void) {
    return s_near_plane_w_clipping;
}

void ps1_video_set_quad_subdivision(bool enabled) {
    s_quad_subdivision = enabled;
}

bool ps1_video_get_quad_subdivision(void) {
    return s_quad_subdivision;
}

void ps1_video_set_true_color(bool enabled) {
    s_true_color_32bit = enabled;
}

bool ps1_video_get_true_color(void) {
    return s_true_color_32bit;
}

void ps1_video_set_dithering(Ps1DitherMode mode) {
    s_dithering = mode;
}

Ps1DitherMode ps1_video_get_dithering(void) {
    return s_dithering;
}
