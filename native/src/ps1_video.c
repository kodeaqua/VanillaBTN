#include "ps1_video.h"
#include "ps1_pgxp.h"
#include "ps1_texture_cache.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static bool s_true_depth_buffer = true;
static bool s_near_plane_w_clipping = true;
static bool s_quad_subdivision = true;
static bool s_true_color_32bit = true;
static Ps1DitherMode s_dithering = PS1_DITHER_OFF;
static bool s_texture_bleeding_fix = true;
static bool s_alpha_halo_fix = true;
static bool s_mipmapping = true;
static Ps1TextureFilterMode s_texture_filter = PS1_FILTER_NEAREST;
static Ps1BackgroundFilterMode s_background_filter = PS1_BACKGROUND_BILINEAR;
static Ps1TextureReplacementProvider s_replacement_provider;
static void *s_replacement_userdata;
static Ps1TextureCache s_texture_cache;
static bool s_texture_cache_initialized;
/* xBRZ is a presentation enhancement, not permission to spend unbounded CPU
 * time in one game tick. The first deterministic primitives in a frame retain
 * the effect; later ones use the existing nearest host mirror. */
#define PS1_HOST_FILTER_SAMPLE_BUDGET 1000000u
#define PS1_HOST_FILTER_MAX_SOURCE_PIXELS 4096u
#define PS1_HOST_FILTER_SOURCE_BUDGET 32768u
static size_t s_host_filter_samples_remaining = PS1_HOST_FILTER_SAMPLE_BUDGET;
static size_t s_host_filter_source_remaining = PS1_HOST_FILTER_SOURCE_BUDGET;
static Ps1AntiAliasingMode s_anti_aliasing = PS1_AA_SMAA;
static bool s_smooth_fog = false;
static float s_fog_near = 1000.0f;
static float s_fog_far = 4000.0f;
static uint32_t s_fog_color = 0xFF808080u;
static bool s_oit_transparency = true;
static bool s_zero_fillrate_lag = true;
static bool s_hdr_tonemapping = true;
static bool s_atomic_clut = false;
static float s_widescreen_scale = 1.0f;
static float s_widescreen_center_x = 160.0f;
static bool s_2d_widescreen_expansion = false;
static bool s_2d_widescreen_repeat = true;
static bool s_world_primitive_context = false;

/* R3 panorama: the original 1024x512 VRAM remains the authority for texture,
 * CLUT and game-visible framebuffer operations. Only GTE-projected fragments
 * outside a 320-pixel drawing area go into this separate host-only sidecar.
 * Never store screen extensions in texture VRAM (x >= 320). */
static bool valid_rect(int x, int y, int width, int height);

typedef struct Ps1PanoramaPixel {
    uint16_t word;
    uint32_t color;
    float depth, transparent_depth;
    uint8_t overdraw, touched;
} Ps1PanoramaPixel;

typedef struct Ps1Panorama {
    /* Only the off-screen columns are allocated. The original 320 columns
     * already live in PS1 VRAM and must never be duplicated or overwritten. */
    Ps1PanoramaPixel *pixels;
    int width, left_extra, right_extra;
} Ps1Panorama;

static Ps1Panorama s_panorama;

/* This is a host presentation surface, never PS1 VRAM. Each projected
 * polygon gets new samples at the chosen scale; GPU texture/CLUT reads and
 * framebuffer downloads continue to use the unscaled PS1 arrays. */
typedef struct Ps1HiRes {
    Ps1PanoramaPixel *pixels;
    int width, scale;
} Ps1HiRes;
static Ps1HiRes s_hires;
static int s_requested_render_scale = 1;

/* Bound the host-only allocation rather than letting 16x ultrawide silently
 * reserve several gigabytes. Failed allocations leave the PS1 path intact. */
#define PS1_HIRES_PIXEL_LIMIT 12000000u
static bool hires_resize(void) {
    int base_width = s_panorama.pixels ? s_panorama.width : 320;
    int scale = s_requested_render_scale;
    while (scale > 1 && (uint64_t)base_width * PS1_VRAM_HEIGHT * scale * scale
                         > PS1_HIRES_PIXEL_LIMIT) --scale;
    if (scale <= 1) {
        free(s_hires.pixels);
        memset(&s_hires, 0, sizeof(s_hires));
        return true;
    }
    if (s_hires.pixels && s_hires.scale == scale
        && s_hires.width == base_width * scale) return true;
    for (; scale > 1; --scale) {
        size_t count = (size_t)base_width * PS1_VRAM_HEIGHT * scale * scale;
        Ps1PanoramaPixel *pixels = (Ps1PanoramaPixel *)calloc(count, sizeof(*pixels));
        if (pixels) {
            free(s_hires.pixels);
            s_hires = (Ps1HiRes){pixels, base_width * scale, scale};
            return true;
        }
    }
    free(s_hires.pixels);
    memset(&s_hires, 0, sizeof(s_hires));
    return true; /* no impact on the original 1x rasterizer */
}

void ps1_video_set_render_scale(int scale) {
    s_requested_render_scale = scale >= 1 && scale <= 16 ? scale : 1;
}

int ps1_video_get_render_scale(void) {
    return s_hires.pixels ? s_hires.scale : 1;
}

/* Copy a pixel modified by an original PS1 2D primitive to its host block.
 * High-resolution 3D samples are written separately, in the same OT order. */
static void hires_mirror(int row, int col, uint16_t word, uint32_t color,
                         float depth, float trans_depth, uint8_t overdraw) {
    if (!s_hires.pixels || row < 0 || row >= PS1_VRAM_HEIGHT) return;
    int x = col + (s_panorama.pixels ? s_panorama.left_extra : 0);
    int scale = s_hires.scale, base_width = s_hires.width / scale;
    if (x < 0 || x >= base_width) return;
    for (int sy = 0; sy < scale; ++sy) {
        Ps1PanoramaPixel *dst = s_hires.pixels
            + ((size_t)row * scale + sy) * s_hires.width + (size_t)x * scale;
        for (int sx = 0; sx < scale; ++sx) {
            dst[sx] = (Ps1PanoramaPixel){word, color, depth, trans_depth, overdraw, 1};
        }
    }
}

bool ps1_video_configure_panorama(int base_width, int base_height, float aspect_scale) {
    /* The original stepped screen fade temporarily switches the display to
     * 640x480 (screen_fade.c), then restores 320x240. This is not a user
     * widescreen toggle: preserve both panorama pages so the next 320x240
     * presentation does not lose already rendered world margins. This special
     * mode is presented using the last real 320x240 panoramic framebuffer
     * with its original fullscreen fade applied, not stretched PS1 content.
     * Explicit disable (0,0) and normal 4:3 remain unchanged. */
    if (base_width == 640 && base_height == 480 && isfinite(aspect_scale)
        && aspect_scale >= 1.0f) return true;
    /* This host path is for the PS1's 320x240 mode. Other video modes retain
     * their original safe 4:3 presentation rather than risking VRAM corruption. */
    if (base_width != 320 || base_height != 240 || !isfinite(aspect_scale)
        || aspect_scale <= 1.00001f) {
        free(s_panorama.pixels);
        memset(&s_panorama, 0, sizeof(s_panorama));
        return hires_resize();
    }
    if (aspect_scale > 4.0f) aspect_scale = 4.0f;
    int width = (int)lroundf((float)base_width * aspect_scale);
    if (width <= base_width) width = base_width + 1;
    if (s_panorama.pixels && s_panorama.width == width) return hires_resize();
    size_t extra_width = (size_t)(width - base_width);
    if (extra_width > SIZE_MAX / PS1_VRAM_HEIGHT / sizeof(Ps1PanoramaPixel)) return false;
    Ps1PanoramaPixel *pixels = (Ps1PanoramaPixel *)calloc(
        extra_width * PS1_VRAM_HEIGHT, sizeof(Ps1PanoramaPixel));
    if (!pixels) return false;
    free(s_panorama.pixels);
    s_panorama.pixels = pixels;
    s_panorama.width = width;
    s_panorama.left_extra = (width - base_width) / 2;
    s_panorama.right_extra = width - base_width - s_panorama.left_extra;
    return hires_resize();
}

int ps1_video_panorama_width(int base_width) {
    return s_panorama.pixels && base_width == 320 ? s_panorama.width : base_width;
}

void ps1_video_panorama_clear_rows(int first_row, int row_count) {
    if (!s_panorama.pixels || first_row < 0 || row_count <= 0
        || first_row >= PS1_VRAM_HEIGHT
        || row_count > PS1_VRAM_HEIGHT - first_row) return;
    size_t stride = (size_t)(s_panorama.left_extra + s_panorama.right_extra);
    memset(s_panorama.pixels + (size_t)first_row * stride, 0,
           (size_t)row_count * stride * sizeof(Ps1PanoramaPixel));
    /* Clearing the sidecar invalidates host samples, too. Leaving touched
     * black samples would hide the white-canvas fallback on menu fades. */
    if (s_hires.pixels) {
        size_t left = (size_t)s_panorama.left_extra * s_hires.scale;
        size_t right = (size_t)s_panorama.right_extra * s_hires.scale;
        size_t offset = left + 320u * (size_t)s_hires.scale;
        for (int row = first_row * s_hires.scale;
             row < (first_row + row_count) * s_hires.scale; ++row) {
            Ps1PanoramaPixel *p = s_hires.pixels + (size_t)row * s_hires.width;
            memset(p, 0, left * sizeof(*p));
            memset(p + offset, 0, right * sizeof(*p));
        }
    }
}


void ps1_video_panorama_fill_rows(int first_row, int row_count, uint32_t rgb24) {
    if (!s_panorama.pixels || first_row < 0 || row_count <= 0
        || first_row >= PS1_VRAM_HEIGHT
        || row_count > PS1_VRAM_HEIGHT - first_row) return;
    const size_t stride = (size_t)(s_panorama.left_extra + s_panorama.right_extra);
    const uint16_t word = (uint16_t)(((rgb24 >> 19) & 31u)
        | (((rgb24 >> 11) & 31u) << 5) | (((rgb24 >> 3) & 31u) << 10));
    const uint32_t color = 0xFF000000u | (rgb24 & 0xFFFFFFu);
    for (int y = first_row; y < first_row + row_count; ++y) {
        Ps1PanoramaPixel *pixels = s_panorama.pixels + (size_t)y * stride;
        for (size_t x = 0; x < stride; ++x) {
            pixels[x] = (Ps1PanoramaPixel){0};
            pixels[x].word = word;
            pixels[x].color = color;
            pixels[x].touched = 1;
            hires_mirror(y, x < (size_t)s_panorama.left_extra
                ? (int)x - s_panorama.left_extra : (int)x - s_panorama.left_extra + 320,
                word, color, 0.0f, 0.0f, 0);
        }
    }
}

void ps1_video_set_world_primitive_context(bool enabled) {
    s_world_primitive_context = enabled;
}

bool ps1_video_get_world_primitive_context(void) {
    return s_world_primitive_context;
}

void ps1_video_panorama_begin_frame(void) {
    /* Explicit full reset, used when the GPU itself is reset and by tests.
     * Do NOT call this on each game frame: one PS1 framebuffer is displayed
     * while the next is being drawn into the other half of VRAM. */
    ps1_video_panorama_clear_rows(0, PS1_VRAM_HEIGHT);
    s_host_filter_samples_remaining = PS1_HOST_FILTER_SAMPLE_BUDGET;
    s_host_filter_source_remaining = PS1_HOST_FILTER_SOURCE_BUDGET;
}

void ps1_video_begin_host_filter_frame(void) {
    s_host_filter_samples_remaining = PS1_HOST_FILTER_SAMPLE_BUDGET;
    s_host_filter_source_remaining = PS1_HOST_FILTER_SOURCE_BUDGET;
}

/* These pointers make the existing PS1 blending/depth rules work on extension
 * fragments without writing into invalid or texture-occupied VRAM columns. */
typedef struct Ps1RasterTarget {
    uint16_t *word;
    uint32_t *color;
    float *depth, *transparent_depth;
    uint8_t *overdraw, *touched;
} Ps1RasterTarget;

static bool panorama_can_extend(const Ps1DrawState *state, bool projected) {
    /* The game's native-only background packet marker is captured at OT
     * insertion (not at replay). This lets real authored 2D backdrop geometry
     * extend too, without misclassifying the HUD as a world primitive. */
    return (projected || s_2d_widescreen_expansion) && s_panorama.pixels
        && state->left == 0 && state->right == 319
        && state->bottom - state->top + 1 == 240;
}

static void panorama_clip_x(const Ps1DrawState *state, bool projected,
                            int *left, int *right_exclusive) {
    if (panorama_can_extend(state, projected)) {
        *left = -s_panorama.left_extra;
        *right_exclusive = 320 + s_panorama.right_extra;
    }
}

static Ps1RasterTarget raster_target(Ps1Video *video, int row, int col,
                                     bool extension) {
    if (!extension || (col >= 0 && col < 320)) {
        return (Ps1RasterTarget){ &video->vram[row][col], &video->color32[row][col],
            &video->zbuffer[row][col], &video->trans_z[row][col],
            &video->overdraw[row][col], NULL };
    }
    /* The caller extends the clip only for 320-wide, projected polygons. */
    int extra = s_panorama.left_extra + s_panorama.right_extra;
    int side_col = col < 0 ? col + s_panorama.left_extra
                           : s_panorama.left_extra + (col - 320);
    Ps1PanoramaPixel *p = &s_panorama.pixels[(size_t)row * extra + side_col];
    return (Ps1RasterTarget){ &p->word, &p->color, &p->depth,
                               &p->transparent_depth, &p->overdraw, &p->touched };
}

static void hires_mirror_target(int row, int col, Ps1RasterTarget dst) {
    hires_mirror(row, col, *dst.word, *dst.color, *dst.depth,
                 *dst.transparent_depth, *dst.overdraw);
}

static uint32_t ps1_video_pixel_argb(const Ps1Video *video, int x, int y) {
    if (s_true_color_32bit && video->color32[y][x] != 0) return video->color32[y][x];
    uint16_t p = video->vram[y][x];
    uint32_t r = p & 31u, g = (p >> 5) & 31u, b = (p >> 10) & 31u;
    return 0xFF000000u | ((r << 3 | r >> 2) << 16)
                       | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

/* A small number of authored 2D pages (e.g. the village map) use a plain
 * white canvas rather than an expandable background packet. When BOTH 4:3
 * borders are predominantly white and no real panoramic content was drawn,
 * carry the canvas color into the unused margins. Do not copy artwork or
 * apply this to any scene with real Hor+ geometry on its side edges. */
static bool panorama_has_white_canvas(const Ps1Video *video,
                                      int src_x, int src_y,
                                      int base_width, int height) {
    if (!s_panorama.pixels || base_width != 320 || height != 240) return false;
    const int extra = s_panorama.left_extra + s_panorama.right_extra;
    if (extra == 0) return false;
    int sampled = 0, both_white = 0, occupied = 0;
    for (int y = 0; y < height; y += 4) {
        const int vy = src_y + y;
        const uint32_t left = ps1_video_pixel_argb(video, src_x, vy);
        const uint32_t right = ps1_video_pixel_argb(video, src_x + base_width - 1, vy);
        const uint32_t l_r = (left >> 16) & 255u, l_g = (left >> 8) & 255u, l_b = left & 255u;
        const uint32_t r_r = (right >> 16) & 255u, r_g = (right >> 8) & 255u, r_b = right & 255u;
        both_white += l_r >= 235u && l_g >= 235u && l_b >= 235u &&
                      r_r >= 235u && r_g >= 235u && r_b >= 235u;
        const Ps1PanoramaPixel *row = s_panorama.pixels + (size_t)vy * extra;
        occupied += row[0].touched != 0 || row[extra - 1].touched != 0;
        ++sampled;
    }
    return occupied == 0 && both_white * 2 > sampled;
}

/* Projected geometry and explicitly flagged full-screen 2D backgrounds can
 * supply pixels outside the original PS1 drawing area. The latter include
 * authored scrolling title tiles and save/load backdrops; they are distinct
 * from HUD sprites. Untouched pixels remain transparent to the host's neutral
 * backdrop: never reflect, stretch or smear the edge of the 4:3 framebuffer. */
bool ps1_video_copy_panorama(const Ps1Video *video, int src_x, int src_y,
                             int base_width, int height,
                             uint32_t *target, size_t stride) {
    int width = ps1_video_panorama_width(base_width);
    if (!video || !target || !valid_rect(src_x, src_y, base_width, height)
        || stride < (size_t)width) return false;
    if (width == base_width) {
        return ps1_video_copy_argb(video, src_x, src_y, base_width, height, target, stride);
    }
    int margin = s_panorama.left_extra;
    const bool white_canvas = panorama_has_white_canvas(video, src_x, src_y,
                                                       base_width, height);
    for (int y = 0; y < height; ++y) {
        int vy = src_y + y;
        for (int x = 0; x < width; ++x) {
            int lx = x - margin;
            if (lx >= 0 && lx < base_width) {
                target[(size_t)y * stride + x] = ps1_video_pixel_argb(video, src_x + lx, vy);
            } else {
                int extra = s_panorama.left_extra + s_panorama.right_extra;
                int side_col = x < margin ? x : x - base_width;
                const Ps1PanoramaPixel *p = &s_panorama.pixels[(size_t)vy * extra + side_col];
                if (p->touched) {
                    if (s_true_color_32bit && p->color != 0) {
                        target[(size_t)y * stride + x] = p->color;
                    } else {
                        uint32_t r = p->word & 31u, g = (p->word >> 5) & 31u,
                                 b = (p->word >> 10) & 31u;
                        target[(size_t)y * stride + x] = 0xFF000000u
                            | ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8)
                            | (b << 3 | b >> 2);
                    }
                } else {
                    /* Keep untouched margins neutral. If a frame contains no
                     * real Hor+ geometry (for example title/menu screens), the
                     * result is a clean 4:3 image with black side bars rather
                     * than mirrored or scanline-smeared artwork. */
                    target[(size_t)y * stride + x] = white_canvas
                        ? 0xFFFFFFFFu : 0xFF000000u;
                }
            }
        }
    }
    return true;
}

/* The scaled surface contains independently shaded 3D fragments while
 * untouched pixels retain the original framebuffer or the panorama backdrop. */
bool ps1_video_copy_scaled_panorama(const Ps1Video *video, int src_x, int src_y,
    int base_width, int height, uint32_t *target, size_t stride) {
    if (!s_hires.pixels || !video || !target || base_width != 320
        || !valid_rect(src_x, src_y, base_width, height)) return false;
    int scale = s_hires.scale;
    int width = s_hires.width;
    if (stride < (size_t)width) return false;
    int margin = s_panorama.pixels ? s_panorama.left_extra : 0;
    bool white_canvas = s_panorama.pixels && panorama_has_white_canvas(
        video, src_x, src_y, base_width, height);
    for (int y = 0; y < height * scale; ++y) {
        int base_y = src_y + y / scale;
        for (int x = 0; x < width; ++x) {
            int logical_x = x / scale - margin;
            Ps1PanoramaPixel *p = s_hires.pixels
                + ((size_t)base_y * scale + y % scale) * width + x;
            if (p->touched) {
                target[(size_t)y * stride + x] = s_true_color_32bit && p->color
                    ? p->color : 0xFF000000u
                        | ((((p->word & 31u) << 3) | ((p->word & 31u) >> 2)) << 16)
                        | (((((p->word >> 5) & 31u) << 3) | (((p->word >> 5) & 31u) >> 2)) << 8)
                        | ((((p->word >> 10) & 31u) << 3) | (((p->word >> 10) & 31u) >> 2));
            } else if (logical_x >= 0 && logical_x < 320) {
                target[(size_t)y * stride + x] = ps1_video_pixel_argb(video, src_x + logical_x, base_y);
            } else {
                int extra = s_panorama.left_extra + s_panorama.right_extra;
                int col = logical_x < 0 ? logical_x + margin : margin + logical_x - 320;
                const Ps1PanoramaPixel *side = &s_panorama.pixels[(size_t)base_y * extra + col];
                target[(size_t)y * stride + x] = side->touched
                    ? (side->color ? side->color : 0xFF000000u)
                    : white_canvas ? 0xFFFFFFFFu : 0xFF000000u;
            }
        }
    }
    return true;
}

#define PS1_NEAR_PLANE_Z 8.0f

/* The host stretches the 320x240 presentation surface to the output aspect.
 * Compress screen-space 2D content before that stretch so HUD/UI keeps its
 * original aspect ratio and stays centered in the 4:3 safe area. GTE-projected
 * 3D vertices are already compensated in ps1_gte.c and must not be transformed
 * a second time here. */
static float widescreen_2d_x(float x) {
    if (s_widescreen_scale <= 1.00001f) return x;
    return (float)((double)s_widescreen_center_x
        + ((double)x - (double)s_widescreen_center_x) / (double)s_widescreen_scale);
}

static int widescreen_round_edge(float x) {
    return (int)floorf(x + 0.5f);
}

/* 2D HUD/menu content is normally compressed into the centered 4:3 safe area.
 * Full-width primitives (screen fades, full-screen images and horizontal
 * backgrounds) must remain full-width or the left/right 16:9 extension will
 * not receive the same image/effect. Explicit background mode also lets tiled
 * screens expand each small tile instead of compressing the whole tile grid. */
static bool widescreen_2d_should_compress(const Ps1DrawState *state,
                                         float min_x, float max_x) {
    if (s_widescreen_scale <= 1.00001f || s_2d_widescreen_expansion) {
        return false;
    }
    return !(min_x <= (float)state->left
        && max_x >= (float)(state->right + 1));
}

static void widescreen_2d_clip_x(const Ps1DrawState *state,
                                int *left, int *right_exclusive) {
    *left = state->left;
    *right_exclusive = state->right + 1;
    if (s_widescreen_scale <= 1.00001f || s_2d_widescreen_expansion) return;

    *left = widescreen_round_edge(widescreen_2d_x((float)state->left));
    *right_exclusive = widescreen_round_edge(widescreen_2d_x((float)(state->right + 1)));
    if (*right_exclusive <= *left) *right_exclusive = *left + 1;
}

static double edge_function(double ax, double ay, double bx, double by,
                            double px, double py) {
    /* Promote coordinates before subtraction. Subtracting in float first can
     * make reversed shared edges differ by ~1e-4 pixels, leaving black cracks
     * between otherwise watertight PGXP triangles. */
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static const int s_dither_matrix[4][4] = {
    { -4,  0, -3,  1 },
    {  2, -2,  3, -1 },
    { -1,  3, -2,  2 },
    {  1, -3,  0, -4 }
};

static uint32_t apply_atmospheric_fog(uint32_t color32, float z) {
    if (!s_smooth_fog || s_fog_far <= s_fog_near || s_fog_near >= 30000.0f || z <= s_fog_near) return color32;

    float f = (z - s_fog_near) / (s_fog_far - s_fog_near);
    if (f > 1.0f) f = 1.0f;

    uint32_t cr = (color32 >> 16) & 255u;
    uint32_t cg = (color32 >> 8) & 255u;
    uint32_t cb = color32 & 255u;

    uint32_t fr = (s_fog_color >> 16) & 255u;
    uint32_t fg = (s_fog_color >> 8) & 255u;
    uint32_t fb = s_fog_color & 255u;

    uint32_t r = (uint32_t)roundf((1.0f - f) * (float)cr + f * (float)fr);
    uint32_t g = (uint32_t)roundf((1.0f - f) * (float)cg + f * (float)fg);
    uint32_t b = (uint32_t)roundf((1.0f - f) * (float)cb + f * (float)fb);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

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
    case 1: /* Back + Front (with HDR soft-additive curve) */
        if (s_hdr_tonemapping) {
            r = br + fr - (br * fr + 127) / 255;
            g = bg + fg - (bg * fg + 127) / 255;
            b = bb + fb - (bb * fb + 127) / 255;
        } else {
            r = br + fr; if (r > 255) r = 255;
            g = bg + fg; if (g > 255) g = 255;
            b = bb + fb; if (b > 255) b = 255;
        }
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

/* Translucent HUD/fade overlays blend against every high-resolution world
 * sample, rather than replacing it with the already-blended 240p pixel. */
static void hires_blend_target(int row, int col, Ps1RasterTarget source,
                               uint32_t front, unsigned mode) {
    if (!s_hires.pixels) return;
    int x = col + (s_panorama.pixels ? s_panorama.left_extra : 0);
    int scale = s_hires.scale, base_width = s_hires.width / scale;
    if (row < 0 || row >= PS1_VRAM_HEIGHT || x < 0 || x >= base_width) return;
    for (int sy = 0; sy < scale; ++sy) {
        Ps1PanoramaPixel *dst = s_hires.pixels
            + ((size_t)row * scale + sy) * s_hires.width + (size_t)x * scale;
        for (int sx = 0; sx < scale; ++sx) {
            Ps1PanoramaPixel *p = dst + sx;
            if (p->touched) {
                p->color = blend_color32(p->color, front, mode);
                p->word = *source.word;
                p->overdraw = *source.overdraw;
            } else {
                *p = (Ps1PanoramaPixel){*source.word, *source.color,
                    *source.depth, *source.transparent_depth, *source.overdraw, 1};
            }
        }
    }
}

static uint32_t blend_color32_under(uint32_t current, uint32_t behind, unsigned blend_mode) {
    uint32_t cr = (current >> 16) & 255u;
    uint32_t cg = (current >> 8) & 255u;
    uint32_t cb = current & 255u;

    uint32_t br = (behind >> 16) & 255u;
    uint32_t bg = (behind >> 8) & 255u;
    uint32_t bb = behind & 255u;

    uint32_t r, g, b;
    switch (blend_mode) {
    case 0: /* 0.75 * current + 0.25 * behind */
        r = (cr * 3 + br) >> 2;
        g = (cg * 3 + bg) >> 2;
        b = (cb * 3 + bb) >> 2;
        break;
    case 1: /* Additive is commutative */
        if (s_hdr_tonemapping) {
            r = cr + br - (cr * br + 127) / 255;
            g = cg + bg - (cg * bg + 127) / 255;
            b = cb + bb - (cb * bb + 127) / 255;
        } else {
            r = cr + br; if (r > 255) r = 255;
            g = cg + bg; if (g > 255) g = 255;
            b = cb + bb; if (b > 255) b = 255;
        }
        break;
    case 2: /* Subtractive: current - behind */
        r = (cr > br) ? cr - br : 0;
        g = (cg > bg) ? cg - bg : 0;
        b = (cb > bb) ? cb - bb : 0;
        break;
    case 3: /* 0.25 addition */
        r = cr + (br >> 2); if (r > 255) r = 255;
        g = cg + (bg >> 2); if (g > 255) g = 255;
        b = cb + (bb >> 2); if (b > 255) b = 255;
        break;
    default:
        r = cr; g = cg; b = cb;
        break;
    }
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static uint32_t texel_to_color32(uint16_t texel, uint32_t rgb24, bool raw_texture) {
    uint32_t red5 = texel & 31u;
    uint32_t green5 = (texel >> 5) & 31u;
    uint32_t blue5 = (texel >> 10) & 31u;

    uint32_t r8 = (red5 << 3) | (red5 >> 2);
    uint32_t g8 = (green5 << 3) | (green5 >> 2);
    uint32_t b8 = (blue5 << 3) | (blue5 >> 2);

    if (raw_texture) {
        return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
    } else {
        uint32_t cr = (rgb24 >> 16) & 255u;
        uint32_t cg = (rgb24 >> 8) & 255u;
        uint32_t cb = rgb24 & 255u;

        r8 = (r8 * cr + 64u) / 128u; if (r8 > 255) r8 = 255;
        g8 = (g8 * cg + 64u) / 128u; if (g8 > 255) g8 = 255;
        b8 = (b8 * cb + 64u) / 128u; if (b8 > 255) b8 = 255;
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
        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
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
            video->trans_z[r][c] = 0.0f;
            video->overdraw[r][c] = 0;
            video->clut_vram[r][c] = color;
            if (c < 320) hires_mirror(r, c, color, c32, 0.0f, 0.0f, 0);
        }
    }
    /* ClearImage/GP0(02) is a VRAM operation, not an OT primitive. An actual
     * full-page clear must also clear its sidecar page with the same color,
     * otherwise the previous 3D frame persists through scene transitions. */
    if (start_x == 0 && fill_width >= 320 &&
        ((start_y == 0 && (fill_height == 240 || fill_height == 480)) ||
         (start_y == 240 && fill_height == 240))) {
        ps1_video_panorama_fill_rows(start_y, fill_height, rgb24);
    }
}

/* Shared by full-screen flat quads and tiles. Implemented below so native
 * 4:3 screen transitions receive identical blend operations in the sidecar. */
static void panorama_draw_fullscreen_color(Ps1Video *video,
        const Ps1DrawState *state, uint32_t rgb24, bool semi_transparent);

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
    /* GP0 TILE_VAR is another legitimate full-screen fade/clear primitive. */
    if (left == state->left && right == state->right + 1
        && top == state->top && bottom == state->bottom + 1) {
        panorama_draw_fullscreen_color(video, state, rgb24, semi_transparent);
    }
    bool compress_2d = tile_width > 0
        && widescreen_2d_should_compress(state, (float)left, (float)right);
    if (compress_2d) {
        left = widescreen_round_edge(widescreen_2d_x((float)left));
        right = widescreen_round_edge(widescreen_2d_x((float)right));
        if (right <= left) right = left + 1;
    }
    int clip_left = state->left;
    int clip_right = state->right + 1;
    if (compress_2d) widescreen_2d_clip_x(state, &clip_left, &clip_right);
    bool panorama_extension = panorama_can_extend(state, false);
    panorama_clip_x(state, false, &clip_left, &clip_right);
    uint16_t color = rgb15(rgb24);
    uint32_t c32_solid = 0xFF000000u | (rgb24 & 0xFFFFFFu);

    if (left < clip_left) left = clip_left;
    if (top < state->top) top = state->top;
    if (right > clip_right) right = clip_right;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;
    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            Ps1RasterTarget dst = raster_target(video, row, col, panorama_extension);
            if (semi_transparent && s_zero_fillrate_lag) {
                if (*dst.overdraw >= 32) continue;
                (*dst.overdraw)++;
            }
            if (state->check_mask && (*dst.word & 0x8000u)) continue;
            uint16_t front = semi_transparent
                ? blend_pixel(*dst.word, color, state->blend_mode) : color;
            *dst.word = front | (state->force_mask ? 0x8000u : 0);

            uint32_t c32 = semi_transparent
                ? blend_color32(*dst.color, c32_solid, state->blend_mode) : c32_solid;
            *dst.color = c32;
            if (dst.touched) *dst.touched = 1;
            if (semi_transparent) hires_blend_target(row, col, dst, c32_solid, state->blend_mode);
            else hires_mirror_target(row, col, dst);
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
    *texel = s_atomic_clut ? video->clut_vram[clut_y][clut_x] : video->vram[clut_y][clut_x];
    return true;
}

static bool sample_single_raw(const Ps1Video *video, unsigned tpage, unsigned clut,
                              int u, int v, uint16_t *texel) {
    if (u < 0) u = 0;
    if (u > 255) u = 255;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return ps1_video_sample_texel(video, tpage, clut, (unsigned)u, (unsigned)v, texel);
}

static bool sample_texel_advanced(const Ps1Video *video, unsigned tpage, unsigned clut,
                                  float uf, float vf, float lod,
                                  float u_min, float u_max, float v_min, float v_max,
                                  uint16_t *out_texel, uint32_t *out_c32, uint32_t shade, bool raw_texture) {
    if (s_texture_bleeding_fix) {
        if (uf < u_min) uf = u_min;
        if (uf > u_max) uf = u_max;
        if (vf < v_min) vf = v_min;
        if (vf > v_max) vf = v_max;
    }

    /* xBRZ is presentation-only. The authoritative PS1 raster retains its
     * nearest source texel; only the host sidecar reads the upscaled cache. */
    if (s_texture_filter != PS1_FILTER_BILINEAR && (!s_mipmapping || lod <= 2.0f)) {
        int iu = s_texture_bleeding_fix ? (int)floorf(uf + 1e-4f) : (int)(uf + 0.5f);
        int iv = s_texture_bleeding_fix ? (int)floorf(vf + 1e-4f) : (int)(vf + 0.5f);
        if (s_texture_bleeding_fix) {
            if (iu < (int)u_min) iu = (int)u_min;
            if (iu > (int)u_max) iu = (int)u_max;
            if (iv < (int)v_min) iv = (int)v_min;
            if (iv > (int)v_max) iv = (int)v_max;
        }
        uint16_t t;
        if (!sample_single_raw(video, tpage, clut, iu, iv, &t)) return false;
        *out_texel = t;
        if (out_c32) {
            *out_c32 = texel_to_color32(t, shade, raw_texture);
        }
        return true;
    }

    /* Mipmapping box-sampling or bilinear sampling */
    if (s_mipmapping && lod > 2.0f) {
        float offset = 0.25f * fminf(lod, 3.0f);
        float samples_u[4] = { uf - offset, uf + offset, uf - offset, uf + offset };
        float samples_v[4] = { vf - offset, vf - offset, vf + offset, vf + offset };

        uint16_t raw_t[4];
        float w[4] = { 0.25f, 0.25f, 0.25f, 0.25f };
        float total_w = 0.0f;
        float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
        bool has_mask = false;

        for (int i = 0; i < 4; ++i) {
            int su = (int)(samples_u[i] + 0.5f);
            int sv = (int)(samples_v[i] + 0.5f);
            if (s_texture_bleeding_fix) {
                if (su < (int)u_min) su = (int)u_min;
                if (su > (int)u_max) su = (int)u_max;
                if (sv < (int)v_min) sv = (int)v_min;
                if (sv > (int)v_max) sv = (int)v_max;
            }
            if (sample_single_raw(video, tpage, clut, su, sv, &raw_t[i]) && raw_t[i] != 0) {
                total_w += w[i];
                sum_r += w[i] * (float)(raw_t[i] & 31u);
                sum_g += w[i] * (float)((raw_t[i] >> 5) & 31u);
                sum_b += w[i] * (float)((raw_t[i] >> 10) & 31u);
                if (raw_t[i] & 0x8000u) has_mask = true;
            } else if (!s_alpha_halo_fix) {
                /* Authentic bug: transparent black is blended in, darkening the edges */
                total_w += w[i];
            }
        }

        if (total_w < 0.25f) {
            *out_texel = 0;
            if (out_c32) *out_c32 = 0;
            return true;
        }

        uint32_t r5 = (uint32_t)(sum_r / total_w + 0.5f);
        uint32_t g5 = (uint32_t)(sum_g / total_w + 0.5f);
        uint32_t b5 = (uint32_t)(sum_b / total_w + 0.5f);
        if (r5 > 31) r5 = 31;
        if (g5 > 31) g5 = 31;
        if (b5 > 31) b5 = 31;

        *out_texel = (uint16_t)(r5 | (g5 << 5) | (b5 << 10) | (has_mask ? 0x8000u : 0));
        if (out_c32) {
            *out_c32 = texel_to_color32(*out_texel, shade, raw_texture);
        }
        return true;
    }

    /* Bilinear filtering */
    int u0 = (int)floorf(uf);
    int v0 = (int)floorf(vf);
    int u1 = u0 + 1;
    int v1 = v0 + 1;
    if (s_texture_bleeding_fix) {
        if (u0 < (int)u_min) u0 = (int)u_min;
        if (u0 > (int)u_max) u0 = (int)u_max;
        if (u1 < (int)u_min) u1 = (int)u_min;
        if (u1 > (int)u_max) u1 = (int)u_max;
        if (v0 < (int)v_min) v0 = (int)v_min;
        if (v0 > (int)v_max) v0 = (int)v_max;
        if (v1 < (int)v_min) v1 = (int)v_min;
        if (v1 > (int)v_max) v1 = (int)v_max;
    }

    float frac_u = uf - floorf(uf);
    float frac_v = vf - floorf(vf);

    uint16_t t00 = 0, t10 = 0, t01 = 0, t11 = 0;
    sample_single_raw(video, tpage, clut, u0, v0, &t00);
    sample_single_raw(video, tpage, clut, u1, v0, &t10);
    sample_single_raw(video, tpage, clut, u0, v1, &t01);
    sample_single_raw(video, tpage, clut, u1, v1, &t11);

    float w00 = (1.0f - frac_u) * (1.0f - frac_v);
    float w10 = frac_u * (1.0f - frac_v);
    float w01 = (1.0f - frac_u) * frac_v;
    float w11 = frac_u * frac_v;

    uint16_t ts[4] = { t00, t10, t01, t11 };
    float ws[4] = { w00, w10, w01, w11 };

    float total_w = 0.0f;
    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    bool has_mask = false;

    for (int i = 0; i < 4; ++i) {
        if (ts[i] != 0) {
            total_w += ws[i];
            sum_r += ws[i] * (float)(ts[i] & 31u);
            sum_g += ws[i] * (float)((ts[i] >> 5) & 31u);
            sum_b += ws[i] * (float)((ts[i] >> 10) & 31u);
            if (ts[i] & 0x8000u) has_mask = true;
        } else if (!s_alpha_halo_fix) {
            /* Black transparent fringe */
            total_w += ws[i];
        }
    }

    if (total_w < 0.25f) {
        *out_texel = 0;
        if (out_c32) *out_c32 = 0;
        return true;
    }

    uint32_t r5 = (uint32_t)(sum_r / total_w + 0.5f);
    uint32_t g5 = (uint32_t)(sum_g / total_w + 0.5f);
    uint32_t b5 = (uint32_t)(sum_b / total_w + 0.5f);
    if (r5 > 31) r5 = 31;
    if (g5 > 31) g5 = 31;
    if (b5 > 31) b5 = 31;

    *out_texel = (uint16_t)(r5 | (g5 << 5) | (b5 << 10) | (has_mask ? 0x8000u : 0));
    if (out_c32) {
        *out_c32 = texel_to_color32(*out_texel, shade, raw_texture);
    }
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

typedef struct Ps1HostTexture {
    const uint32_t *pixels;
    int width, height, scale;
    size_t stride;
} Ps1HostTexture;

static void ensure_texture_cache(void) {
    if (s_texture_cache_initialized) return;
    ps1_texture_cache_init(&s_texture_cache, 64u * 1024u * 1024u);
    s_texture_cache_initialized = true;
}

static uint32_t raw_texel_argb(uint16_t texel) {
    if (texel == 0) return 0;
    unsigned r = texel & 31u, g = (texel >> 5) & 31u, b = (texel >> 10) & 31u;
    return 0xFF000000u | ((r << 3 | r >> 2) << 16)
        | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

static uint32_t *decode_texture_region(const Ps1Video *video, unsigned tpage,
    unsigned clut, unsigned u, unsigned v, int width, int height) {
    if (width <= 0 || height <= 0
        || (size_t)width > SIZE_MAX / (size_t)height / sizeof(uint32_t)) return NULL;
    uint32_t *pixels = (uint32_t *)malloc(
        (size_t)width * (size_t)height * sizeof(*pixels));
    if (!pixels) return NULL;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t texel = 0;
            if (!ps1_video_sample_texel(video, tpage, clut,
                    u + (unsigned)x, v + (unsigned)y, &texel)) {
                free(pixels);
                return NULL;
            }
            pixels[(size_t)y * width + x] = raw_texel_argb(texel);
        }
    }
    return pixels;
}

static bool reserve_host_filter_samples(int width, int height, int scale) {
    if (width <= 0 || height <= 0 || scale <= 1) return false;
    if ((size_t)width > SIZE_MAX / (size_t)height / (size_t)scale / (size_t)scale)
        return false;
    size_t required = (size_t)width * (size_t)height * (size_t)scale * (size_t)scale;
    if (required > s_host_filter_samples_remaining) return false;
    s_host_filter_samples_remaining -= required;
    return true;
}

static bool reserve_host_filter_source(int width, int height) {
    if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / (size_t)height)
        return false;
    size_t required = (size_t)width * (size_t)height;
    if (required > s_host_filter_source_remaining) return false;
    s_host_filter_source_remaining -= required;
    return true;
}

static bool replacement_dimensions_valid(const Ps1ReplacementImage *image,
                                          int source_width, int source_height) {
    if (!image || !image->pixels || image->width < source_width
        || image->height < source_height || image->stride < (size_t)image->width
        || image->width % source_width != 0 || image->height % source_height != 0)
        return false;
    int scale_x = image->width / source_width;
    int scale_y = image->height / source_height;
    return scale_x == scale_y && scale_x >= 1 && scale_x <= 16;
}

static bool prepare_host_texture(const uint32_t *source, int width, int height,
                                 int requested_scale, bool allow_xbrz,
                                 Ps1HostTexture *out) {
    if (!source || !out || width <= 0 || height <= 0) return false;
    ensure_texture_cache();
    uint64_t key = ps1_texture_content_key(source, width, height, (size_t)width);
    if (!key) return false;

    if (s_replacement_provider) {
        bool known_miss = ps1_texture_cache_find(&s_texture_cache, key,
            PS1_CACHE_REPLACEMENT_MISS, 0) != NULL;
        const Ps1CachedTexture *cached = ps1_texture_cache_find(
            &s_texture_cache, key, PS1_CACHE_REPLACEMENT, 0);
        if (!cached && !known_miss) {
            Ps1ReplacementImage image = {0};
            if (s_replacement_provider(key, width, height, &image,
                                       s_replacement_userdata)) {
                if (replacement_dimensions_valid(&image, width, height)) {
                    size_t count = (size_t)image.width * (size_t)image.height;
                    uint32_t *packed = (uint32_t *)malloc(count * sizeof(*packed));
                    if (packed) {
                        for (int y = 0; y < image.height; ++y)
                            memcpy(packed + (size_t)y * image.width,
                                image.pixels + (size_t)y * image.stride,
                                (size_t)image.width * sizeof(*packed));
                        (void)ps1_texture_cache_store(&s_texture_cache, key,
                            PS1_CACHE_REPLACEMENT, 0, packed,
                            image.width, image.height);
                        free(packed);
                    }
                }
                if (image.release) image.release(image.pixels,
                                                 image.release_userdata);
            }
            cached = ps1_texture_cache_find(&s_texture_cache, key,
                PS1_CACHE_REPLACEMENT, 0);
            if (!cached) {
                const uint32_t marker = 0;
                (void)ps1_texture_cache_store(&s_texture_cache, key,
                    PS1_CACHE_REPLACEMENT_MISS, 0, &marker, 1, 1);
            }
        }
        if (cached) {
            *out = (Ps1HostTexture){cached->pixels, cached->width, cached->height,
                cached->width / width, (size_t)cached->width};
            return true;
        }
    }

    if (!allow_xbrz) return false;
    int scale = requested_scale > 6 ? 6 : requested_scale;
    if (scale < 2) return false;
    const Ps1CachedTexture *cached = ps1_texture_cache_find(
        &s_texture_cache, key, PS1_CACHE_XBRZ, scale);
    if (!cached) {
        int scaled_width = width * scale, scaled_height = height * scale;
        if (scaled_width <= 0 || scaled_height <= 0
            || (size_t)scaled_width > SIZE_MAX / (size_t)scaled_height
                / sizeof(uint32_t)) return false;
        uint32_t *scaled = (uint32_t *)malloc(
            (size_t)scaled_width * scaled_height * sizeof(*scaled));
        if (!scaled) return false;
        bool ok = ps1_texture_scale_xbrz(source, width, height, (size_t)width,
            scale, scaled, (size_t)scaled_width)
            && ps1_texture_cache_store(&s_texture_cache, key, PS1_CACHE_XBRZ,
                scale, scaled, scaled_width, scaled_height);
        free(scaled);
        if (!ok) return false;
        cached = ps1_texture_cache_find(&s_texture_cache, key,
            PS1_CACHE_XBRZ, scale);
    }
    if (!cached) return false;
    *out = (Ps1HostTexture){cached->pixels, cached->width, cached->height,
        scale, (size_t)cached->width};
    return true;
}

static uint32_t modulate_argb(uint32_t color, uint32_t rgb24, bool raw) {
    if (raw || (color >> 24) == 0) return color;
    unsigned r = (((color >> 16) & 255u) * ((rgb24 >> 16) & 255u)) >> 7;
    unsigned g = (((color >> 8) & 255u) * ((rgb24 >> 8) & 255u)) >> 7;
    unsigned b = ((color & 255u) * (rgb24 & 255u)) >> 7;
    if (r > 255u) r = 255u;
    if (g > 255u) g = 255u;
    if (b > 255u) b = 255u;
    return (color & 0xFF000000u) | (r << 16) | (g << 8) | b;
}

static uint32_t source_over(uint32_t back, uint32_t front) {
    unsigned a = front >> 24;
    if (a == 255u) return front;
    if (a == 0) return back;
    unsigned inv = 255u - a;
    unsigned r = (((front >> 16) & 255u) * a + ((back >> 16) & 255u) * inv + 127u) / 255u;
    unsigned g = (((front >> 8) & 255u) * a + ((back >> 8) & 255u) * inv + 127u) / 255u;
    unsigned b = ((front & 255u) * a + (back & 255u) * inv + 127u) / 255u;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static float cubic_weight(float x) {
    x = fabsf(x);
    if (x <= 1.0f) return (1.5f * x - 2.5f) * x * x + 1.0f;
    if (x < 2.0f) return ((-0.5f * x + 2.5f) * x - 4.0f) * x + 2.0f;
    return 0.0f;
}

static uint32_t filtered_source_pixel(const uint32_t *source, int width, int height,
    float x, float y, Ps1BackgroundFilterMode mode) {
    if (mode == PS1_BACKGROUND_NEAREST) {
        int ix = (int)floorf(x + 0.5f), iy = (int)floorf(y + 0.5f);
        if (ix < 0) ix = 0; if (ix >= width) ix = width - 1;
        if (iy < 0) iy = 0; if (iy >= height) iy = height - 1;
        return source[(size_t)iy * width + ix];
    }
    int radius = mode == PS1_BACKGROUND_BICUBIC ? 2 : 1;
    int base_x = (int)floorf(x), base_y = (int)floorf(y);
    double sum_a = 0.0, sum_r = 0.0, sum_g = 0.0, sum_b = 0.0, weights = 0.0;
    for (int oy = 1 - radius; oy <= radius; ++oy) {
        int sy = base_y + oy;
        if (sy < 0) sy = 0; if (sy >= height) sy = height - 1;
        float wy = mode == PS1_BACKGROUND_BICUBIC
            ? cubic_weight(y - (float)(base_y + oy))
            : (oy == 0 ? 1.0f - (y - floorf(y)) : y - floorf(y));
        for (int ox = 1 - radius; ox <= radius; ++ox) {
            int sx = base_x + ox;
            if (sx < 0) sx = 0; if (sx >= width) sx = width - 1;
            float wx = mode == PS1_BACKGROUND_BICUBIC
                ? cubic_weight(x - (float)(base_x + ox))
                : (ox == 0 ? 1.0f - (x - floorf(x)) : x - floorf(x));
            double weight = (double)wx * wy;
            uint32_t c = source[(size_t)sy * width + sx];
            double a = (double)(c >> 24) / 255.0;
            weights += weight;
            sum_a += weight * a;
            sum_r += weight * a * ((c >> 16) & 255u);
            sum_g += weight * a * ((c >> 8) & 255u);
            sum_b += weight * a * (c & 255u);
        }
    }
    if (weights <= 1e-9 || sum_a <= 1e-9) return 0;
    double alpha = sum_a / weights;
    if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
    unsigned a = (unsigned)(alpha * 255.0 + 0.5);
    unsigned r = (unsigned)fmin(255.0, fmax(0.0, sum_r / sum_a + 0.5));
    unsigned g = (unsigned)fmin(255.0, fmax(0.0, sum_g / sum_a + 0.5));
    unsigned b = (unsigned)fmin(255.0, fmax(0.0, sum_b / sum_a + 0.5));
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void hires_draw_filtered_sprite(Ps1Video *video, const Ps1DrawState *state,
    const Ps1Sprite *sprite, bool is_projected, int shift_x, bool side_only,
    int origin_y, float dest_left, float dest_width,
    int left, int top, int right, int bottom) {
    if (!s_hires.pixels || s_hires.scale <= 1) return;
    int source_width = (int)(sprite->width & 0x3FF);
    int source_height = (int)(sprite->height & 0x1FF);
    uint32_t *source = decode_texture_region(video, sprite->tpage, sprite->clut,
        sprite->u, sprite->v, source_width, source_height);
    if (!source) return;

    const bool background = s_2d_widescreen_expansion && !is_projected;
    Ps1HostTexture texture = {source, source_width, source_height, 1,
                              (size_t)source_width};
    bool replacement = false;
    if (s_replacement_provider) {
        Ps1HostTexture candidate;
        replacement = prepare_host_texture(source, source_width, source_height,
            s_hires.scale, false, &candidate);
        if (replacement) texture = candidate;
    }
    if (!background && !replacement && s_texture_filter == PS1_FILTER_XBRZ) {
        Ps1HostTexture filtered;
        if (prepare_host_texture(source, source_width, source_height,
                                 s_hires.scale, true, &filtered)) texture = filtered;
    }
    int margin = s_panorama.pixels ? s_panorama.left_extra : 0;
    int scale = s_hires.scale;
    int scaled_left = left * scale, scaled_right = right * scale;
    int scaled_top = top * scale, scaled_bottom = bottom * scale;
    for (int sy = scaled_top; sy < scaled_bottom; ++sy) {
        float logical_y = ((float)sy + 0.5f) / scale;
        float source_y = logical_y - (float)origin_y;
        if (source_y < 0.0f || source_y >= (float)source_height) continue;
        for (int sx = scaled_left; sx < scaled_right; ++sx) {
            /* Negative panorama coordinates need floor division: truncating
             * -1/scale to zero drops the last side-only samples at the seam. */
            int logical_col = sx < 0 ? (sx - scale + 1) / scale : sx / scale;
            if (side_only && logical_col >= 0 && logical_col < 320) continue;
            float logical_x = ((float)sx + 0.5f) / scale;
            float source_x = (logical_x - dest_left)
                * (float)source_width / dest_width;
            if (source_x < 0.0f || source_x >= (float)source_width) continue;
            uint32_t color;
            if (background && !replacement) {
                color = filtered_source_pixel(source, source_width, source_height,
                    source_x - 0.5f, source_y - 0.5f, s_background_filter);
            } else {
                int tx = (int)(source_x * texture.scale);
                int ty = (int)(source_y * texture.scale);
                if (tx < 0) tx = 0;
                if (ty < 0) ty = 0;
                if (tx >= texture.width) tx = texture.width - 1;
                if (ty >= texture.height) ty = texture.height - 1;
                color = texture.pixels[(size_t)ty * texture.stride + tx];
            }
            if ((color >> 24) == 0) continue;
            color = modulate_argb(color, sprite->rgb24, sprite->raw_texture);
            size_t index = (size_t)sy * s_hires.width
                + (size_t)(sx + margin * scale);
            Ps1PanoramaPixel *dst = &s_hires.pixels[index];
            if (!dst->touched) {
                int base_x = logical_col;
                Ps1RasterTarget base = raster_target(video, sy / scale, base_x,
                    panorama_can_extend(state, is_projected));
                dst->word = *base.word;
                dst->color = *base.color;
                dst->depth = *base.depth;
                dst->transparent_depth = *base.transparent_depth;
                dst->overdraw = *base.overdraw;
                dst->touched = 1;
            }
            if (state->check_mask && (dst->word & 0x8000u)) continue;
            uint32_t back = dst->color ? dst->color : raw_texel_argb(dst->word);
            uint16_t nearest_texel = 0;
            (void)ps1_video_sample_texel(video, sprite->tpage, sprite->clut,
                sprite->u + (unsigned)source_x,
                sprite->v + (unsigned)source_y, &nearest_texel);
            if (sprite->semi_transparent && (nearest_texel & 0x8000u)) {
                unsigned coverage = color >> 24;
                uint32_t blended = blend_color32(back,
                    0xFF000000u | (color & 0xFFFFFFu), state->blend_mode);
                color = (coverage << 24) | (blended & 0xFFFFFFu);
            }
            dst->color = source_over(back, color);
            dst->word = rgb15(dst->color & 0xFFFFFFu);
            if (state->force_mask) dst->word |= 0x8000u;
        }
    }
    free(source);
    (void)shift_x;
}

/* A second, presentation-only raster pass evaluates covered 3D fragments
 * at sub-PS1-pixel centers. This pass precedes the original GP0 raster so
 * texture uploads and VRAM reads still see precisely the original target. */
static void hires_draw_projected_triangle(Ps1Video *video, const Ps1DrawState *state,
    const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
    uint32_t rgb24, unsigned tpage, unsigned clut, bool textured,
    bool raw_texture, bool semi_transparent, const uint32_t *colors) {
    if (!s_hires.pixels || !(v0->is_projected || v1->is_projected || v2->is_projected))
        return;
    const Ps1Vertex *vertices[3] = {v0, v1, v2};
    double x[3], y[3], w[3], u[3], v[3];
    uint32_t shades[3];
    const bool precise = ps1_pgxp_is_vertex_precision_enabled()
        && v0->has_subpixel && v1->has_subpixel && v2->has_subpixel;
    for (int i = 0; i < 3; ++i) {
        x[i] = (precise ? vertices[i]->sub_x : (float)signed11(vertices[i]->x))
            + signed11(state->offset_x);
        y[i] = (precise ? vertices[i]->sub_y : (float)signed11(vertices[i]->y))
            + signed11(state->offset_y);
        w[i] = vertices[i]->w > 0.0f ? vertices[i]->w : 1.0;
        u[i] = vertices[i]->u;
        v[i] = vertices[i]->v;
        shades[i] = colors ? colors[i] : rgb24;
    }
    double area = edge_function(x[0], y[0], x[1], y[1], x[2], y[2]);
    if (fabs(area) < 1e-5) return;
    if (area < 0.0) {
        double d;
        uint32_t c;
#define EXCHANGE_PAIR(a) do { d = a[1]; a[1] = a[2]; a[2] = d; } while (0)
        EXCHANGE_PAIR(x); EXCHANGE_PAIR(y); EXCHANGE_PAIR(w);
        EXCHANGE_PAIR(u); EXCHANGE_PAIR(v);
#undef EXCHANGE_PAIR
        c = shades[1]; shades[1] = shades[2]; shades[2] = c;
        area = -area;
    }
    const bool extension = panorama_can_extend(state, true);
    int clip_left = state->left, clip_right = state->right + 1;
    panorama_clip_x(state, true, &clip_left, &clip_right);
    const int scale = s_hires.scale;
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(x[i]) || !isfinite(y[i]) || !isfinite(w[i])) return;
    }
    /* Clip in floating point before converting to integer raster indices;
     * near-plane projection may have large off-screen coordinates. */
    double min_x = fmax(fmin(x[0], fmin(x[1], x[2])), (double)clip_left);
    double max_x = fmin(fmax(x[0], fmax(x[1], x[2])), (double)clip_right);
    double min_y = fmax(fmin(y[0], fmin(y[1], y[2])), (double)state->top);
    double max_y = fmin(fmax(y[0], fmax(y[1], y[2])), (double)(state->bottom + 1));
    if (!(min_x < max_x && min_y < max_y)) return;
    int left = (int)floor(min_x * scale);
    int right = (int)ceil(max_x * scale);
    int top = (int)floor(min_y * scale);
    int bottom = (int)ceil(max_y * scale);
    if (right <= left || bottom <= top) return;
    const bool edge0 = (y[2] < y[1]) || (y[2] == y[1] && x[2] > x[1]);
    const bool edge1 = (y[0] < y[2]) || (y[0] == y[2] && x[0] > x[2]);
    const bool edge2 = (y[1] < y[0]) || (y[1] == y[0] && x[1] > x[0]);
    const bool perspective = textured && ps1_pgxp_is_perspective_texture_enabled()
        && v0->w > 0.0f && v1->w > 0.0f && v2->w > 0.0f;
    const float u_min = (float)fmin(u[0], fmin(u[1], u[2]));
    const float u_max = (float)fmax(u[0], fmax(u[1], u[2]));
    const float v_min = (float)fmin(v[0], fmin(v[1], v[2]));
    const float v_max = (float)fmax(v[0], fmax(v[1], v[2]));
    /* UV derivatives are in output pixels, not PS1 pixels. */
    const double inv_area = 1.0 / area;
    const double du_dx = ((y[1]-y[2])*u[0] + (y[2]-y[0])*u[1]
        + (y[0]-y[1])*u[2]) * inv_area;
    const double dv_dx = ((y[1]-y[2])*v[0] + (y[2]-y[0])*v[1]
        + (y[0]-y[1])*v[2]) * inv_area;
    const double du_dy = ((x[2]-x[1])*u[0] + (x[0]-x[2])*u[1]
        + (x[1]-x[0])*u[2]) * inv_area;
    const double dv_dy = ((x[2]-x[1])*v[0] + (x[0]-x[2])*v[1]
        + (x[1]-x[0])*v[2]) * inv_area;
    float lod = (float)(fmax(hypot(du_dx, dv_dx), hypot(du_dy, dv_dy)) / scale);
    uint32_t *host_source = NULL;
    Ps1HostTexture host_texture = {0};
    bool host_filtered = false;
    int host_u = (int)floorf(u_min), host_v = (int)floorf(v_min);
    int host_source_width = (int)ceilf(u_max) - host_u + 1;
    int host_source_height = (int)ceilf(v_max) - host_v + 1;
    if (textured && (s_texture_filter == PS1_FILTER_XBRZ || s_replacement_provider)
        && host_source_width > 0 && host_source_width <= 256
        && host_source_height > 0 && host_source_height <= 256
        && (size_t)host_source_width * (size_t)host_source_height
            <= PS1_HOST_FILTER_MAX_SOURCE_PIXELS
        && reserve_host_filter_source(host_source_width, host_source_height)) {
        host_source = decode_texture_region(video, tpage, clut,
            (unsigned)host_u, (unsigned)host_v,
            host_source_width, host_source_height);
        host_filtered = host_source && prepare_host_texture(host_source,
            host_source_width, host_source_height, scale,
            s_texture_filter == PS1_FILTER_XBRZ, &host_texture);
    }
    int margin = s_panorama.pixels ? s_panorama.left_extra : 0;
    for (int sy = top; sy < bottom; ++sy) {
        const double py = ((double)sy + 0.5) / scale;
        for (int sx = left; sx < right; ++sx) {
            const double px = ((double)sx + 0.5) / scale;
            double e0 = edge_function(x[1], y[1], x[2], y[2], px, py);
            double e1 = edge_function(x[2], y[2], x[0], y[0], px, py);
            double e2 = edge_function(x[0], y[0], x[1], y[1], px, py);
            if (!((e0 > 0.0 || (e0 >= -1e-6 && edge0))
                && (e1 > 0.0 || (e1 >= -1e-6 && edge1))
                && (e2 > 0.0 || (e2 >= -1e-6 && edge2)))) continue;
            double l0 = e0 * inv_area, l1 = e1 * inv_area, l2 = e2 * inv_area;
            double depth = l0 * w[0] + l1 * w[1] + l2 * w[2];
            size_t index = (size_t)sy * s_hires.width + (size_t)(sx + margin * scale);
            Ps1PanoramaPixel *dst = &s_hires.pixels[index];
            /* Lazily copy the pre-polygon low-res background only for the
             * first fragment at this output sample. 3D never touches VRAM. */
            if (!dst->touched) {
                Ps1RasterTarget base = raster_target(video, sy / scale, (int)floor(px), extension);
                dst->word = *base.word;
                dst->color = *base.color;
                dst->depth = *base.depth;
                dst->transparent_depth = *base.transparent_depth;
                dst->overdraw = *base.overdraw;
                dst->touched = 1;
            }
            if (state->check_mask && (dst->word & 0x8000u)) continue;
            float test_depth = (float)depth;
            if (semi_transparent) test_depth = test_depth * 1.002f + 1e-5f;
            if (s_true_depth_buffer && test_depth < dst->depth - 1e-6f) continue;
            if (semi_transparent && s_zero_fillrate_lag && dst->overdraw >= 32) continue;
            uint32_t shade = rgb24;
            if (colors) {
                shade = 0;
                for (unsigned shift = 0; shift < 24; shift += 8) {
                    double component = l0 * ((shades[0] >> shift) & 255u)
                        + l1 * ((shades[1] >> shift) & 255u)
                        + l2 * ((shades[2] >> shift) & 255u);
                    if (component < 0.0) component = 0.0;
                    if (component > 255.0) component = 255.0;
                    shade |= (uint32_t)(component + 0.5) << shift;
                }
            }
            uint16_t front;
            uint32_t front32;
            bool blend = semi_transparent;
            if (textured) {
                float uf, vf;
                if (perspective && depth > 1e-9) {
                    uf = (float)((l0*u[0]*w[0] + l1*u[1]*w[1] + l2*u[2]*w[2]) / depth);
                    vf = (float)((l0*v[0]*w[0] + l1*v[1]*w[1] + l2*v[2]*w[2]) / depth);
                } else {
                    uf = (float)(l0*u[0] + l1*u[1] + l2*u[2]);
                    vf = (float)(l0*v[0] + l1*v[1] + l2*v[2]);
                }
                uint16_t texel;
                if (host_filtered) {
                    int tx = (int)((uf - host_u) * host_texture.scale);
                    int ty = (int)((vf - host_v) * host_texture.scale);
                    if (tx < 0) tx = 0;
                    if (ty < 0) ty = 0;
                    if (tx >= host_texture.width) tx = host_texture.width - 1;
                    if (ty >= host_texture.height) ty = host_texture.height - 1;
                    front32 = host_texture.pixels[(size_t)ty * host_texture.stride + tx];
                    if ((front32 >> 24) == 0) continue;
                    front32 = modulate_argb(front32, shade, raw_texture);
                    if (!ps1_video_sample_texel(video, tpage, clut,
                            (unsigned)uf, (unsigned)vf, &texel)) continue;
                    front = rgb15(front32 & 0xFFFFFFu) | (texel & 0x8000u);
                } else {
                    if (!sample_texel_advanced(video, tpage, clut, uf, vf, lod,
                            u_min, u_max, v_min, v_max, &texel, &front32,
                            shade, raw_texture) || !texel) continue;
                    front = raw_texture ? texel : modulate_texel(texel, shade);
                }
                blend = semi_transparent && (texel & 0x8000u);
            } else {
                front = quantize_rgb24(shade, sy / scale, sx / scale, state->dither);
                front32 = 0xFF000000u | (shade & 0xFFFFFFu);
            }
            if (blend) {
                front = blend_pixel(dst->word, front, state->blend_mode);
                if (s_oit_transparency && dst->transparent_depth > 0.0f
                    && depth < dst->transparent_depth - 1e-6f) {
                    front32 = blend_color32_under(dst->color, front32, state->blend_mode);
                } else {
                    front32 = blend_color32(dst->color, front32, state->blend_mode);
                    dst->transparent_depth = (float)depth;
                }
                if (s_zero_fillrate_lag) ++dst->overdraw;
            }
            if (s_smooth_fog && depth > 1e-9
                && (v0->w > 0.0f || v1->w > 0.0f || v2->w > 0.0f)) {
                front32 = apply_atmospheric_fog(front32, (float)(1.0 / depth));
            }
            dst->word = front | (state->force_mask ? 0x8000u : 0u);
            dst->color = front32;
            if (s_true_depth_buffer && !semi_transparent) dst->depth = (float)depth;
        }
    }
    free(host_source);
}

static bool draw_sprite_copy(Ps1Video *video,
                           const Ps1DrawState *state, const Ps1Sprite *sprite,
                           bool is_projected, int shift_x, bool side_only) {
    if (video == NULL || !valid_draw_state(state) || sprite == NULL
        || ((sprite->tpage >> 7) & 3u) == 3u) {
        return false;
    }
    int origin_x = signed11(sprite->x) + signed11(state->offset_x) + shift_x;
    int origin_y = signed11(sprite->y) + signed11(state->offset_y);
    int source_width = (int)(sprite->width & 0x3FF);
    int source_height = (int)(sprite->height & 0x1FF);
    if (source_width <= 0 || source_height <= 0) return true;

    bool compress_2d = !is_projected && widescreen_2d_should_compress(
        state, (float)origin_x, (float)(origin_x + source_width));
    bool panorama_extension = panorama_can_extend(state, is_projected);
    float dest_left = compress_2d
        ? widescreen_2d_x((float)origin_x) : (float)origin_x;
    float dest_right = compress_2d
        ? widescreen_2d_x((float)(origin_x + source_width))
        : (float)(origin_x + source_width);
    float dest_width = dest_right - dest_left;
    if (dest_width <= 0.0f) return true;
    int left = widescreen_round_edge(dest_left);
    int top = origin_y;
    int right = widescreen_round_edge(dest_right);
    int bottom = top + source_height;
    if (right <= left) right = left + 1;

    int clip_left = state->left;
    int clip_right = state->right + 1;
    if (compress_2d) widescreen_2d_clip_x(state, &clip_left, &clip_right);
    panorama_clip_x(state, is_projected, &clip_left, &clip_right);
    if (left < clip_left) left = clip_left;
    if (top < state->top) top = state->top;
    if (right > clip_right) right = clip_right;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;
    float u_min = (float)sprite->u;
    float u_max = (float)(sprite->u + (unsigned)source_width - 1u);
    float v_min = (float)sprite->v;
    float v_max = (float)(sprite->v + (unsigned)source_height - 1u);
    bool host_filtered = s_hires.pixels &&
        (s_texture_filter == PS1_FILTER_XBRZ || s_replacement_provider
            || (s_2d_widescreen_expansion && !is_projected));
    const bool background = s_2d_widescreen_expansion && !is_projected;
    /* Per-tile filtering clamps samples at authored tile borders and can
     * switch to nearest halfway through a panorama when the budget runs out.
     * Keep the baseline mirror until background filtering runs on a composed
     * surface with shared samples across the center and side columns. */
    if (background) host_filtered = false;
    if (host_filtered && !background
        && (size_t)source_width * (size_t)source_height
            > PS1_HOST_FILTER_MAX_SOURCE_PIXELS) {
        host_filtered = false;
    }
    if (host_filtered && !background && (s_texture_filter == PS1_FILTER_XBRZ
        || s_replacement_provider)
        && !reserve_host_filter_source(source_width, source_height)) {
        host_filtered = false;
    }
    if (host_filtered && !reserve_host_filter_samples(right - left, bottom - top,
                                                       s_hires.scale)) {
        host_filtered = false;
    }

    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            /* Background copies may extend a repeating authored pattern, but
             * must never modify the real 4:3 framebuffer or any HUD pixels. */
            if (side_only && col >= 0 && col < 320) continue;
            float source_x = ((float)col + 0.5f - dest_left)
                * (float)source_width / dest_width;
            if (source_x < 0.0f) source_x = 0.0f;
            if (source_x > (float)source_width) source_x = (float)source_width;
            float u_offset = s_texture_filter != PS1_FILTER_BILINEAR
                ? floorf(source_x) : source_x - 0.5f;
            if (u_offset < 0.0f) u_offset = 0.0f;
            if (u_offset > (float)(source_width - 1)) u_offset = (float)(source_width - 1);
            float uf = (float)sprite->u + u_offset;
            float vf = (float)(sprite->v + (unsigned)(row - origin_y));
            uint16_t texel;
            uint32_t front_c32;
            if (!sample_texel_advanced(video, sprite->tpage, sprite->clut, uf, vf, 1.0f,
                                       u_min, u_max, v_min, v_max,
                                       &texel, &front_c32, sprite->rgb24, sprite->raw_texture)) {
                continue;
            }
            if (texel == 0) continue; /* Zero texels leave the destination untouched. */
            Ps1RasterTarget dst = raster_target(video, row, col, panorama_extension);
            uint16_t *pixel = dst.word;
            if (state->check_mask && (*pixel & 0x8000u)) continue;
            uint16_t front = sprite->raw_texture
                ? texel : modulate_texel(texel, sprite->rgb24);
            if (sprite->semi_transparent && (texel & 0x8000u)) {
                if (s_zero_fillrate_lag) {
                    if (*dst.overdraw >= 32) continue;
                    (*dst.overdraw)++;
                }
                front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
            }
            *pixel = front | (state->force_mask ? 0x8000u : 0);

            uint32_t incoming_color = front_c32;
            bool blended = sprite->semi_transparent && (texel & 0x8000u);
            if (blended) {
                front_c32 = blend_color32(*dst.color, front_c32, state->blend_mode);
            }
            *dst.color = front_c32;
            if (dst.touched) *dst.touched = 1;
            if (!host_filtered) {
                if (blended) hires_blend_target(row, col, dst, incoming_color, state->blend_mode);
                else hires_mirror_target(row, col, dst);
            }
        }
    }
    if (host_filtered) {
        hires_draw_filtered_sprite(video, state, sprite, is_projected,
            shift_x, side_only, origin_y, dest_left, dest_width,
            left, top, right, bottom);
    }
    return true;
}

/* Explicitly marked 2D backgrounds can repeat into panorama-only columns.
 * Menu grids repeat every 96px, while title bucket-36 sprites already tile the
 * backdrop beyond 4:3; never draw those semi-transparent sprites twice. */
bool ps1_video_draw_sprite_with_provenance(Ps1Video *video,
                           const Ps1DrawState *state, const Ps1Sprite *sprite,
                           bool is_projected) {
    if (!draw_sprite_copy(video, state, sprite, is_projected, 0, false)) return false;
    if (!s_panorama.pixels || !s_2d_widescreen_expansion || is_projected
        || !s_2d_widescreen_repeat || !state
        || !panorama_can_extend(state, false)) return true;
    const int extra = s_panorama.left_extra > s_panorama.right_extra
        ? s_panorama.left_extra : s_panorama.right_extra;
    const int period = sprite->width == 24 && sprite->height == 24 ? 96 : 320;
    for (int shift = period; shift < extra + 320; shift += period) {
        if (!draw_sprite_copy(video, state, sprite, false, -shift, true) ||
            !draw_sprite_copy(video, state, sprite, false, shift, true)) return false;
    }
    return true;
}

bool ps1_video_draw_sprite(Ps1Video *video, const Ps1DrawState *state,
                           const Ps1Sprite *sprite) {
    return ps1_video_draw_sprite_with_provenance(video, state, sprite, false);
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
    out_v->is_projected = a->is_projected && b->is_projected;

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
        uint32_t bl = (uint32_t)roundf(bw_i / w_near);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (bl > 255) bl = 255;
        *out_c = (r << 16) | (g << 8) | bl;
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

    if (s_near_plane_w_clipping && v0->is_projected && v1->is_projected && v2->is_projected
        && v0->has_subpixel && v1->has_subpixel && v2->has_subpixel) {
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

    hires_draw_projected_triangle(video, state, v0, v1, v2, rgb24, tpage, clut,
                                  textured, raw_texture, semi_transparent, colors);

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

        bool is_2d = !v0->is_projected && !v1->is_projected && !v2->is_projected;
        int clip_left = state->left;
        int clip_right = state->right + 1;
        bool compress_2d = is_2d && widescreen_2d_should_compress(
            state, fminf(fx0, fminf(fx1, fx2)), fmaxf(fx0, fmaxf(fx1, fx2)));
        if (compress_2d) {
            fx0 = widescreen_2d_x(fx0);
            fx1 = widescreen_2d_x(fx1);
            fx2 = widescreen_2d_x(fx2);
            widescreen_2d_clip_x(state, &clip_left, &clip_right);
        }
        bool projected_world = v0->is_projected || v1->is_projected || v2->is_projected;
        bool panorama_extension = panorama_can_extend(state, projected_world);
        panorama_clip_x(state, projected_world, &clip_left, &clip_right);

        double fdet = edge_function((double)fx0, (double)fy0,
                                    (double)fx1, (double)fy1,
                                    (double)fx2, (double)fy2);
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

        if (left < clip_left) left = clip_left;
        if (top < state->top) top = state->top;
        if (right > clip_right) right = clip_right;
        if (bottom > state->bottom + 1) bottom = state->bottom + 1;

        bool use_perspective = textured && ps1_pgxp_is_perspective_texture_enabled()
                            && (v0->w > 0.0f && v1->w > 0.0f && v2->w > 0.0f);

        float pu0 = fu0 * fw0, pv0 = fv0 * fw0;
        float pu1 = fu1 * fw1, pv1 = fv1 * fw1;
        float pu2 = fu2 * fw2, pv2 = fv2 * fw2;

        uint16_t flat_color = rgb15(rgb24);
        double inv_det = 1.0 / fdet;

        float u_min = fminf(fu0, fminf(fu1, fu2));
        float u_max = fmaxf(fu0, fmaxf(fu1, fu2));
        float v_min = fminf(fv0, fminf(fv1, fv2));
        float v_max = fmaxf(fv0, fmaxf(fv1, fv2));

        double dl0_dx = -((double)fy2 - (double)fy1) * inv_det;
        double dl1_dx = -((double)fy0 - (double)fy2) * inv_det;
        double dl2_dx = -((double)fy1 - (double)fy0) * inv_det;

        double dl0_dy = ((double)fx2 - (double)fx1) * inv_det;
        double dl1_dy = ((double)fx0 - (double)fx2) * inv_det;
        double dl2_dy = ((double)fx1 - (double)fx0) * inv_det;

        float du_dx = (float)(dl0_dx * fu0 + dl1_dx * fu1 + dl2_dx * fu2);
        float dv_dx = (float)(dl0_dx * fv0 + dl1_dx * fv1 + dl2_dx * fv2);
        float du_dy = (float)(dl0_dy * fu0 + dl1_dy * fu1 + dl2_dy * fu2);
        float dv_dy = (float)(dl0_dy * fv0 + dl1_dy * fv1 + dl2_dy * fv2);

        float tri_lod = fmaxf(sqrtf(du_dx * du_dx + dv_dx * dv_dx), sqrtf(du_dy * du_dy + dv_dy * dv_dy));

        bool edge0 = (fy2 < fy1) || (fy2 == fy1 && fx2 > fx1);
        bool edge1 = (fy0 < fy2) || (fy0 == fy2 && fx0 > fx2);
        bool edge2 = (fy1 < fy0) || (fy1 == fy0 && fx1 > fx0);

        for (int row = top; row < bottom; ++row) {
            for (int col = left; col < right; ++col) {
                double px = (double)col + 0.5;
                double py = (double)row + 0.5;

                double w0 = edge_function((double)fx1, (double)fy1,
                                          (double)fx2, (double)fy2, px, py);
                double w1 = edge_function((double)fx2, (double)fy2,
                                          (double)fx0, (double)fy0, px, py);
                double w2 = edge_function((double)fx0, (double)fy0,
                                          (double)fx1, (double)fy1, px, py);

                if ((w0 > 0.0 || (w0 >= -1e-6 && edge0))
                    && (w1 > 0.0 || (w1 >= -1e-6 && edge1))
                    && (w2 > 0.0 || (w2 >= -1e-6 && edge2))) {
                    Ps1RasterTarget dst = raster_target(video, row, col, panorama_extension);
                    uint16_t *pixel = dst.word;
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
                        if (w_test < *dst.depth - 1e-6f) {
                            continue; /* Occluded by nearer geometry! */
                        }
                    }

                    if (semi_transparent && s_zero_fillrate_lag) {
                        if (*dst.overdraw >= 32) continue;
                        (*dst.overdraw)++;
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
                        if (semi_transparent) {
                            if (s_oit_transparency) {
                                float prev_w = *dst.transparent_depth;
                                if (prev_w > 0.0f && (float)interp_w < prev_w - 1e-6f) {
                                    front32 = blend_color32_under(*dst.color, c32_poly, state->blend_mode);
                                } else {
                                    front32 = blend_color32(*dst.color, c32_poly, state->blend_mode);
                                    *dst.transparent_depth = (float)interp_w;
                                }
                            } else {
                                front32 = blend_color32(*dst.color, c32_poly, state->blend_mode);
                            }
                        } else {
                            front32 = c32_poly;
                        }
                    } else {
                        float uf, vf;
                        if (use_perspective) {
                            if (interp_w > 1e-9) {
                                double inv_w = 1.0 / interp_w;
                                uf = (float)((l0 * (double)pu0 + l1 * (double)pu1 + l2 * (double)pu2) * inv_w);
                                vf = (float)((l0 * (double)pv0 + l1 * (double)pv1 + l2 * (double)pv2) * inv_w);
                            } else {
                                uf = (float)(l0 * (double)fu0 + l1 * (double)fu1 + l2 * (double)fu2);
                                vf = (float)(l0 * (double)fv0 + l1 * (double)fv1 + l2 * (double)fv2);
                            }
                        } else {
                            uf = (float)(l0 * (double)fu0 + l1 * (double)fu1 + l2 * (double)fu2);
                            vf = (float)(l0 * (double)fv0 + l1 * (double)fv1 + l2 * (double)fv2);
                        }

                        uint16_t texel;
                        if (!sample_texel_advanced(video, tpage, clut, uf, vf, tri_lod,
                                                   u_min, u_max, v_min, v_max,
                                                   &texel, &front32, shade, raw_texture)) {
                            continue;
                        }
                        if (texel == 0) continue;

                        front = raw_texture ? texel : modulate_texel(texel, shade);
                        if (semi_transparent && (texel & 0x8000u)) {
                            front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
                            if (s_zero_fillrate_lag) {
                                if (*dst.overdraw >= 32) continue;
                                (*dst.overdraw)++;
                            }
                            if (s_oit_transparency) {
                                float prev_w = *dst.transparent_depth;
                                if (prev_w > 0.0f && (float)interp_w < prev_w - 1e-6f) {
                                    front32 = blend_color32_under(*dst.color, front32, state->blend_mode);
                                } else {
                                    front32 = blend_color32(*dst.color, front32, state->blend_mode);
                                    *dst.transparent_depth = (float)interp_w;
                                }
                            } else {
                                front32 = blend_color32(*dst.color, front32, state->blend_mode);
                            }
                        }
                    }
                    if (s_smooth_fog && (v0->w > 0.0f || v1->w > 0.0f || v2->w > 0.0f) && interp_w > 1e-9) {
                        float cam_z = (float)(1.0 / interp_w);
                        front32 = apply_atmospheric_fog(front32, cam_z);
                    }
                    *pixel = front | (state->force_mask ? 0x8000u : 0);
                    *dst.color = front32;
                    if (dst.touched) *dst.touched = 1;
                    if (!projected_world) {
                        if (semi_transparent && !textured)
                            hires_blend_target(row, col, dst, 0xFF000000u | (shade & 0xFFFFFFu), state->blend_mode);
                        else hires_mirror_target(row, col, dst);
                    }
                    if (s_true_depth_buffer && !semi_transparent) {
                        *dst.depth = (float)interp_w;
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
    bool is_2d = !v0->is_projected && !v1->is_projected && !v2->is_projected;
    int clip_left = state->left;
    int clip_right = state->right + 1;
    bool compress_2d = is_2d && widescreen_2d_should_compress(
        state, (float)min3(x0, x1, x2), (float)max3(x0, x1, x2));
    if (compress_2d) {
        x0 = widescreen_round_edge(widescreen_2d_x((float)x0));
        x1 = widescreen_round_edge(widescreen_2d_x((float)x1));
        x2 = widescreen_round_edge(widescreen_2d_x((float)x2));
        widescreen_2d_clip_x(state, &clip_left, &clip_right);
    }
    bool projected_world = v0->is_projected || v1->is_projected || v2->is_projected;
    bool panorama_extension = panorama_can_extend(state, projected_world);
    panorama_clip_x(state, projected_world, &clip_left, &clip_right);

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

    if (left < clip_left) left = clip_left;
    if (top < state->top) top = state->top;
    if (right > clip_right) right = clip_right;
    if (bottom > state->bottom + 1) bottom = state->bottom + 1;

    uint16_t flat_color = rgb15(rgb24);
    float u_min = (float)min3((int)u0, (int)u1, (int)u2);
    float u_max = (float)max3((int)u0, (int)u1, (int)u2);
    float v_min = (float)min3((int)v_0, (int)v_1, (int)v_2);
    float v_max = (float)max3((int)v_0, (int)v_1, (int)v_2);

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
                Ps1RasterTarget dst = raster_target(video, row, col, panorama_extension);
                uint16_t *pixel = dst.word;
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
                        ? blend_color32(*dst.color, c32_poly, state->blend_mode)
                        : c32_poly;
                } else {
                    float uf = (float)((w0 * u0 + w1 * u1 + w2 * u2) / det);
                    float vf = (float)((w0 * v_0 + w1 * v_1 + w2 * v_2) / det);
                    uint16_t texel;
                    if (!sample_texel_advanced(video, tpage, clut, uf, vf, 1.0f,
                                               u_min, u_max, v_min, v_max,
                                               &texel, &front32, shade, raw_texture)) {
                        continue;
                    }
                    if (texel == 0) continue;

                    front = raw_texture ? texel : modulate_texel(texel, shade);
                    if (semi_transparent && (texel & 0x8000u)) {
                        front = blend_pixel(*pixel, front, state->blend_mode) | 0x8000u;
                    }
                    if (semi_transparent && (texel & 0x8000u)) {
                        front32 = blend_color32(*dst.color, front32, state->blend_mode);
                    }
                }
                *pixel = front | (state->force_mask ? 0x8000u : 0);
                *dst.color = front32;
                if (dst.touched) *dst.touched = 1;
                    if (!projected_world) {
                        if (semi_transparent && !textured)
                            hires_blend_target(row, col, dst, 0xFF000000u | (shade & 0xFFFFFFu), state->blend_mode);
                        else hires_mirror_target(row, col, dst);
                    }
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
    bool compress_2d = widescreen_2d_should_compress(
        state, (float)(x0 < x1 ? x0 : x1), (float)(x0 > x1 ? x0 : x1));
    if (compress_2d) {
        x0 = widescreen_round_edge(widescreen_2d_x((float)x0));
        x1 = widescreen_round_edge(widescreen_2d_x((float)x1));
    }
    int clip_left = state->left;
    int clip_right = state->right + 1;
    if (compress_2d) widescreen_2d_clip_x(state, &clip_left, &clip_right);
    bool panorama_extension = panorama_can_extend(state, false);
    panorama_clip_x(state, false, &clip_left, &clip_right);
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    uint16_t color = rgb15(rgb24);
    uint32_t c32_solid = 0xFF000000u | (rgb24 & 0xFFFFFFu);
    for (;;) {
        if (x0 >= clip_left && x0 < clip_right && y0 >= state->top && y0 <= state->bottom) {
            Ps1RasterTarget dst = raster_target(video, y0, x0, panorama_extension);
            if (!state->check_mask || !(*dst.word & 0x8000)) {
                if (semi_transparent && s_zero_fillrate_lag) {
                    if (*dst.overdraw >= 32) goto next_line_pixel;
                    (*dst.overdraw)++;
                }
                *dst.word = (semi_transparent ? blend_pixel(*dst.word, color, state->blend_mode) : color)
                    | (state->force_mask ? 0x8000 : 0);
                uint32_t c32 = semi_transparent
                    ? blend_color32(*dst.color, c32_solid, state->blend_mode) : c32_solid;
                *dst.color = c32;
                if (dst.touched) *dst.touched = 1;
                hires_mirror_target(y0, x0, dst);
            }
        }
next_line_pixel:
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
    out.is_projected = a->is_projected && b->is_projected;
    return out;
}

static uint32_t lerp_color(uint32_t c0, uint32_t c1, float t) {
    uint32_t r = (uint32_t)roundf((1.0f - t) * ((c0 >> 16) & 255) + t * ((c1 >> 16) & 255));
    uint32_t g = (uint32_t)roundf((1.0f - t) * ((c0 >> 8) & 255) + t * ((c1 >> 8) & 255));
    uint32_t b = (uint32_t)roundf((1.0f - t) * (c0 & 255) + t * (c1 & 255));
    return (r << 16) | (g << 8) | b;
}

static bool normalize_quad_vertex_provenance(const Ps1Vertex *vertices[4],
                                             Ps1Vertex normalized[4]) {
    bool any_projected = false;
    bool all_projected_subpixel = true;
    for (int i = 0; i < 4; ++i) {
        any_projected = any_projected || vertices[i]->is_projected;
        all_projected_subpixel = all_projected_subpixel
            && vertices[i]->is_projected && vertices[i]->has_subpixel;
    }
    if (!any_projected) return false;

    /* A GPU quad is one primitive. If one GTE vertex loses its PGXP cache entry,
     * keep the whole quad out of the 2D safe-area path and fall back to integer
     * coordinates for all four corners instead of mixing rasterization modes
     * across its diagonal. */
    for (int i = 0; i < 4; ++i) {
        normalized[i] = *vertices[i];
        normalized[i].is_projected = true;
        if (!all_projected_subpixel) normalized[i].has_subpixel = false;
    }
    return true;
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

/* Full-screen flat quads implement PS1 fades and wipes. Normal 320x240
 * quads affect one panorama page; screen_fade_step() uses a transient 640x480
 * quad spanning both pages. Mirror its color/blend operation onto both sidecar
 * pages in OT order so fade phases never snap or leave stale bright margins. */
static void panorama_draw_fullscreen_color(Ps1Video *video,
        const Ps1DrawState *state, uint32_t rgb24, bool semi_transparent) {
    if (!video || !valid_draw_state(state) || !s_panorama.pixels) return;
    const bool page = state->left == 0 && state->right == 319
        && state->bottom - state->top + 1 == 240;
    const bool full_fade = state->left == 0 && state->right == 639
        && state->top == 0 && state->bottom == 479;
    if (!page && !full_fade) return;
    const uint16_t front_word = rgb15(rgb24);
    const uint32_t front_color = 0xFF000000u | (rgb24 & 0xFFFFFFu);
    const size_t extra = (size_t)(s_panorama.left_extra + s_panorama.right_extra);
    for (int y = state->top; y <= state->bottom; ++y) {
        Ps1PanoramaPixel *row = s_panorama.pixels + (size_t)y * extra;
        for (size_t x = 0; x < extra; ++x) {
            Ps1PanoramaPixel *p = &row[x];
            if (state->check_mask && (p->word & 0x8000u)) continue;
            if (semi_transparent && s_zero_fillrate_lag) {
                if (p->overdraw >= 32) continue;
                ++p->overdraw;
            }
            p->word = (semi_transparent
                ? blend_pixel(p->word, front_word, state->blend_mode) : front_word)
                | (state->force_mask ? 0x8000u : 0);
            p->color = semi_transparent
                ? blend_color32(p->touched ? p->color : 0xFF000000u,
                                front_color, state->blend_mode) : front_color;
            p->touched = 1;
            int col = (int)x < s_panorama.left_extra
                ? (int)x - s_panorama.left_extra
                : (int)x - s_panorama.left_extra + 320;
            if (semi_transparent) {
                Ps1RasterTarget source = {&p->word, &p->color,
                    &p->depth, &p->transparent_depth, &p->overdraw, &p->touched};
                hires_blend_target(y, col, source, front_color, state->blend_mode);
            } else {
                hires_mirror(y, col, p->word, p->color,
                             p->depth, p->transparent_depth, p->overdraw);
            }
        }
    }
}

/* A full-screen flat polygon must have all four original screen corners;
 * checking only extrema would misclassify a diagonal/degenerate polygon. */
static void panorama_draw_fullscreen_flat_quad(Ps1Video *video,
        const Ps1DrawState *state, const Ps1Vertex *const vertices[4],
        uint32_t rgb24, bool semi_transparent) {
    if (!video || !valid_draw_state(state) || !s_panorama.pixels) return;
    const bool page = state->left == 0 && state->right == 319
        && state->bottom - state->top + 1 == 240;
    const bool full_fade = state->left == 0 && state->right == 639
        && state->top == 0 && state->bottom == 479;
    if (!page && !full_fade) return;
    unsigned corners = 0;
    for (int i = 0; i < 4; ++i) {
        const Ps1Vertex *v = vertices[i];
        if (!v || v->is_projected) return;
        const int x = signed11(v->x) + signed11(state->offset_x);
        const int y = signed11(v->y) + signed11(state->offset_y);
        if ((x != state->left && x != state->right + 1) ||
            (y != state->top && y != state->bottom + 1)) return;
        unsigned corner = (unsigned)(x != state->left) | ((unsigned)(y != state->top) << 1);
        if (corners & (1u << corner)) return;
        corners |= 1u << corner;
    }
    if (corners == 15u)
        panorama_draw_fullscreen_color(video, state, rgb24, semi_transparent);
}

bool ps1_video_draw_quad(Ps1Video *video, const Ps1DrawState *state,
                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                         uint32_t rgb24, unsigned tpage, unsigned clut,
                         bool textured, bool raw_texture, bool semi_transparent) {
    const Ps1Vertex *input_vertices[4] = {v0, v1, v2, v3};
    Ps1Vertex normalized_vertices[4];
    if (normalize_quad_vertex_provenance(input_vertices, normalized_vertices)) {
        v0 = &normalized_vertices[0];
        v1 = &normalized_vertices[1];
        v2 = &normalized_vertices[2];
        v3 = &normalized_vertices[3];
    }

    if (!textured) {
        const Ps1Vertex *screen_vertices[4] = {v0, v1, v2, v3};
        panorama_draw_fullscreen_flat_quad(video, state, screen_vertices,
                                            rgb24, semi_transparent);
    }

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
    const Ps1Vertex *input_vertices[4] = {v0, v1, v2, v3};
    Ps1Vertex normalized_vertices[4];
    if (normalize_quad_vertex_provenance(input_vertices, normalized_vertices)) {
        v0 = &normalized_vertices[0];
        v1 = &normalized_vertices[1];
        v2 = &normalized_vertices[2];
        v3 = &normalized_vertices[3];
    }

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
    const Ps1Vertex *input_vertices[4] = {v0, v1, v2, v3};
    Ps1Vertex normalized_vertices[4];
    if (normalize_quad_vertex_provenance(input_vertices, normalized_vertices)) {
        v0 = &normalized_vertices[0];
        v1 = &normalized_vertices[1];
        v2 = &normalized_vertices[2];
        v3 = &normalized_vertices[3];
    }

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
        if (!s_atomic_clut) {
            memcpy(&video->clut_vram[y + row][x], source + row * source_stride,
                   (size_t)width * sizeof(uint16_t));
        }
        for (int col = 0; col < width; ++col) {
            uint16_t pixel = source[row * source_stride + col];
            uint32_t red = pixel & 31u;
            uint32_t green = (pixel >> 5) & 31u;
            uint32_t blue = (pixel >> 10) & 31u;
            video->color32[y + row][x + col] = 0xFF000000u
                | ((red << 3 | red >> 2) << 16)
                | ((green << 3 | green >> 2) << 8)
                | (blue << 3 | blue >> 2);
            if (x + col < 320) hires_mirror(y + row, x + col, pixel,
                video->color32[y + row][x + col], 0.0f, 0.0f, 0);
        }
    }
    return true;
}

static inline float pixel_luma(uint32_t c) {
    float r = (float)((c >> 16) & 255u);
    float g = (float)((c >> 8) & 255u);
    float b = (float)(c & 255u);
    return 0.299f * r + 0.587f * g + 0.114f * b;
}

static void apply_subpixel_anti_aliasing(uint32_t *target, int width, int height, size_t stride) {
    if (width < 3 || height < 3) return;
    for (int row = 1; row < height - 1; ++row) {
        for (int col = 1; col < width - 1; ++col) {
            uint32_t c_center = target[row * stride + col];
            float l_c = pixel_luma(c_center);

            uint32_t c_up    = target[(row - 1) * stride + col];
            uint32_t c_down  = target[(row + 1) * stride + col];
            uint32_t c_left  = target[row * stride + (col - 1)];
            uint32_t c_right = target[row * stride + (col + 1)];

            float l_u = pixel_luma(c_up);
            float l_d = pixel_luma(c_down);
            float l_l = pixel_luma(c_left);
            float l_r = pixel_luma(c_right);

            float l_min = fminf(l_c, fminf(fminf(l_u, l_d), fminf(l_l, l_r)));
            float l_max = fmaxf(l_c, fmaxf(fmaxf(l_u, l_d), fmaxf(l_l, l_r)));

            float contrast = l_max - l_min;
            if (contrast < 24.0f) continue;

            float edge_h = fabsf((l_l + l_r) - 2.0f * l_c);
            float edge_v = fabsf((l_u + l_d) - 2.0f * l_c);

            uint32_t neighbor;
            if (edge_h >= edge_v) {
                neighbor = fabsf(l_l - l_c) > fabsf(l_r - l_c) ? c_left : c_right;
            } else {
                neighbor = fabsf(l_u - l_c) > fabsf(l_d - l_c) ? c_up : c_down;
            }

            uint32_t cr = (c_center >> 16) & 255u;
            uint32_t cg = (c_center >> 8) & 255u;
            uint32_t cb = c_center & 255u;

            uint32_t nr = (neighbor >> 16) & 255u;
            uint32_t ng = (neighbor >> 8) & 255u;
            uint32_t nb = neighbor & 255u;

            uint32_t out_r = (5u * cr + 3u * nr) >> 3;
            uint32_t out_g = (5u * cg + 3u * ng) >> 3;
            uint32_t out_b = (5u * cb + 3u * nb) >> 3;

            target[row * stride + col] = 0xFF000000u | (out_r << 16) | (out_g << 8) | out_b;
        }
    }
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
    if (s_anti_aliasing == PS1_AA_SMAA) {
        apply_subpixel_anti_aliasing(target, width, height, target_stride);
    }
    return true;
}

void ps1_video_set_true_depth_buffer(bool enabled) {
    s_true_depth_buffer = enabled;
}

void ps1_video_set_widescreen_scale(float scale, float center_x) {
    if (!isfinite(scale) || scale < 1.0f || scale > 4.0f) {
        scale = 1.0f;
    }
    if (!isfinite(center_x) || center_x < 0.0f || center_x > (float)PS1_VRAM_WIDTH) {
        center_x = 160.0f;
    }
    s_widescreen_scale = scale;
    s_widescreen_center_x = center_x;
}

float ps1_video_get_widescreen_scale(void) {
    return s_widescreen_scale;
}

void ps1_video_set_2d_widescreen_repeat(bool enabled) {
    s_2d_widescreen_repeat = enabled;
}

void ps1_video_set_2d_widescreen_expansion(bool enabled) {
    s_2d_widescreen_expansion = enabled;
}

bool ps1_video_get_2d_widescreen_expansion(void) {
    return s_2d_widescreen_expansion;
}

bool ps1_video_get_true_depth_buffer(void) {
    return s_true_depth_buffer;
}

void ps1_video_clear_zbuffer(Ps1Video *video) {
    if (video == NULL) return;
    memset(video->zbuffer, 0, sizeof(video->zbuffer));
    memset(video->trans_z, 0, sizeof(video->trans_z));
    memset(video->overdraw, 0, sizeof(video->overdraw));
    if (s_hires.pixels) {
        size_t count = (size_t)s_hires.width * PS1_VRAM_HEIGHT * s_hires.scale;
        for (size_t i = 0; i < count; ++i) {
            s_hires.pixels[i].depth = 0.0f;
            s_hires.pixels[i].transparent_depth = 0.0f;
            s_hires.pixels[i].overdraw = 0;
        }
    }
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

void ps1_video_set_texture_bleeding_fix(bool enabled) {
    s_texture_bleeding_fix = enabled;
}

bool ps1_video_get_texture_bleeding_fix(void) {
    return s_texture_bleeding_fix;
}

void ps1_video_set_alpha_halo_fix(bool enabled) {
    s_alpha_halo_fix = enabled;
}

bool ps1_video_get_alpha_halo_fix(void) {
    return s_alpha_halo_fix;
}

void ps1_video_set_mipmapping(bool enabled) {
    s_mipmapping = enabled;
}

bool ps1_video_get_mipmapping(void) {
    return s_mipmapping;
}

void ps1_video_set_texture_filter(Ps1TextureFilterMode mode) {
    s_texture_filter = mode >= PS1_FILTER_NEAREST && mode <= PS1_FILTER_XBRZ
        ? mode : PS1_FILTER_NEAREST;
}

Ps1TextureFilterMode ps1_video_get_texture_filter(void) {
    return s_texture_filter;
}

void ps1_video_set_background_filter(Ps1BackgroundFilterMode mode) {
    s_background_filter = mode >= PS1_BACKGROUND_NEAREST
        && mode <= PS1_BACKGROUND_BICUBIC ? mode : PS1_BACKGROUND_NEAREST;
}

Ps1BackgroundFilterMode ps1_video_get_background_filter(void) {
    return s_background_filter;
}

void ps1_video_set_texture_replacement_provider(
    Ps1TextureReplacementProvider provider, void *userdata) {
    s_replacement_provider = provider;
    s_replacement_userdata = userdata;
}

void ps1_video_set_anti_aliasing(Ps1AntiAliasingMode mode) {
    s_anti_aliasing = mode;
}

Ps1AntiAliasingMode ps1_video_get_anti_aliasing(void) {
    return s_anti_aliasing;
}

void ps1_video_set_smooth_fog(bool enabled) {
    s_smooth_fog = enabled;
}

bool ps1_video_get_smooth_fog(void) {
    return s_smooth_fog;
}

void ps1_video_set_fog_params(float near_z, float far_z, uint32_t fog_color) {
    s_fog_near = near_z;
    s_fog_far = far_z;
    s_fog_color = fog_color;
}

void ps1_video_get_fog_params(float *near_z, float *far_z, uint32_t *fog_color) {
    if (near_z) *near_z = s_fog_near;
    if (far_z) *far_z = s_fog_far;
    if (fog_color) *fog_color = s_fog_color;
}

void ps1_video_set_oit_transparency(bool enabled) {
    s_oit_transparency = enabled;
}

bool ps1_video_get_oit_transparency(void) {
    return s_oit_transparency;
}

void ps1_video_set_zero_fillrate_lag(bool enabled) {
    s_zero_fillrate_lag = enabled;
}

bool ps1_video_get_zero_fillrate_lag(void) {
    return s_zero_fillrate_lag;
}

void ps1_video_set_hdr_tonemapping(bool enabled) {
    s_hdr_tonemapping = enabled;
}

bool ps1_video_get_hdr_tonemapping(void) {
    return s_hdr_tonemapping;
}

void ps1_video_set_atomic_clut(bool enabled) {
    s_atomic_clut = enabled;
}

bool ps1_video_get_atomic_clut(void) {
    return s_atomic_clut;
}

void ps1_video_commit_clut(Ps1Video *video) {
    if (video != NULL) {
        memcpy(video->clut_vram, video->vram, sizeof(video->clut_vram));
    }
}

static bool s_extended_draw_distance = false;
static bool s_hd_font = false;

void ps1_video_set_extended_draw_distance(bool enabled) {
    s_extended_draw_distance = enabled;
    if (enabled) {
        ps1_video_set_fog_params(1500.0f, 8000.0f, 0x00A0A0A0);
    } else {
        ps1_video_set_fog_params(500.0f, 3000.0f, 0x00808080);
    }
}

bool ps1_video_get_extended_draw_distance(void) {
    return s_extended_draw_distance;
}

void ps1_video_set_hd_font(bool enabled) {
    s_hd_font = enabled;
}

bool ps1_video_get_hd_font(void) {
    return s_hd_font;
}
