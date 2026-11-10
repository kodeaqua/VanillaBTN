#ifndef VANILLABTN_PS1_VIDEO_H
#define VANILLABTN_PS1_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS1_VRAM_WIDTH 1024
#define PS1_VRAM_HEIGHT 512

typedef enum Ps1DitherMode {
    PS1_DITHER_OFF = 0,
    PS1_DITHER_NATIVE = 1,
    PS1_DITHER_DEBAND = 2
} Ps1DitherMode;

typedef enum Ps1TextureFilterMode {
    PS1_FILTER_NEAREST = 0,
    PS1_FILTER_BILINEAR = 1
} Ps1TextureFilterMode;

typedef enum Ps1AntiAliasingMode {
    PS1_AA_OFF = 0,
    PS1_AA_MSAA_4X = 1,
    PS1_AA_SMAA = 2
} Ps1AntiAliasingMode;

typedef struct Ps1Video {
    uint16_t vram[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
    float zbuffer[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
    uint32_t color32[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
    float trans_z[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
    uint8_t overdraw[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
    uint16_t clut_vram[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
} Ps1Video;

typedef struct Ps1DrawState {
    int left, top, right, bottom; /* Inclusive drawing area in VRAM. */
    int offset_x, offset_y;       /* Signed 11-bit GP0 draw offset. */
    unsigned blend_mode;          /* GP0(E1) bits 5-6: average, add, subtract, add quarter. */
    unsigned tpage;               /* GP0(E1) bits 0-8: texture page (x64/y256 base, depth). */
    bool force_mask, check_mask;  /* GP0(E6) bits 0-1. */
    bool dither;                  /* GP0(E1) bit 9. */
} Ps1DrawState;

typedef struct Ps1Sprite {
    int x, y, width, height;
    unsigned u, v, tpage, clut;
    uint32_t rgb24;               /* 0xRRGGBB; 0x808080 is neutral modulation. */
    bool raw_texture, semi_transparent;
} Ps1Sprite;

typedef struct Ps1Vertex {
    int x, y;
    unsigned u, v;
    float sub_x, sub_y;
    float z, w;
    bool has_subpixel;
} Ps1Vertex;

void ps1_video_set_pgxp_vertex_precision(bool enabled);
void ps1_video_set_pgxp_perspective_texture(bool enabled);
bool ps1_video_get_pgxp_vertex_precision(void);
bool ps1_video_get_pgxp_perspective_texture(void);

void ps1_video_set_true_depth_buffer(bool enabled);
bool ps1_video_get_true_depth_buffer(void);
void ps1_video_clear_zbuffer(Ps1Video *video);

void ps1_video_set_near_plane_clipping(bool enabled);
bool ps1_video_get_near_plane_clipping(void);

void ps1_video_set_quad_subdivision(bool enabled);
bool ps1_video_get_quad_subdivision(void);

void ps1_video_set_true_color(bool enabled);
bool ps1_video_get_true_color(void);

void ps1_video_set_dithering(Ps1DitherMode mode);
Ps1DitherMode ps1_video_get_dithering(void);

void ps1_video_set_texture_bleeding_fix(bool enabled);
bool ps1_video_get_texture_bleeding_fix(void);

void ps1_video_set_alpha_halo_fix(bool enabled);
bool ps1_video_get_alpha_halo_fix(void);

void ps1_video_set_mipmapping(bool enabled);
bool ps1_video_get_mipmapping(void);

void ps1_video_set_texture_filter(Ps1TextureFilterMode mode);
Ps1TextureFilterMode ps1_video_get_texture_filter(void);

void ps1_video_set_anti_aliasing(Ps1AntiAliasingMode mode);
Ps1AntiAliasingMode ps1_video_get_anti_aliasing(void);

void ps1_video_set_smooth_fog(bool enabled);
bool ps1_video_get_smooth_fog(void);

void ps1_video_set_fog_params(float near_z, float far_z, uint32_t fog_color);
void ps1_video_get_fog_params(float *near_z, float *far_z, uint32_t *fog_color);

void ps1_video_set_oit_transparency(bool enabled);
bool ps1_video_get_oit_transparency(void);

void ps1_video_set_zero_fillrate_lag(bool enabled);
bool ps1_video_get_zero_fillrate_lag(void);

void ps1_video_set_hdr_tonemapping(bool enabled);
bool ps1_video_get_hdr_tonemapping(void);

void ps1_video_set_atomic_clut(bool enabled);
bool ps1_video_get_atomic_clut(void);
void ps1_video_commit_clut(Ps1Video *video);

/* Preserve raw PS1 BGR555 words, including the high mask/semitransparency bit. */
bool ps1_video_upload(Ps1Video *video, int x, int y, int width, int height,
                      const uint16_t *source, size_t source_stride);

/* Read a display rectangle as opaque ARGB8888; the high PS1 bit is not color. */
bool ps1_video_copy_argb(const Ps1Video *video, int x, int y, int width, int height,
                         uint32_t *target, size_t target_stride);

void ps1_draw_state_reset(Ps1DrawState *state);

/* GP0(02): fill VRAM directly, including its 16-pixel width rounding and wrapping. */
void ps1_video_quick_fill(Ps1Video *video, int x, int y, int width, int height,
                          uint32_t rgb24);

/* GP0(60/62/68/6A): untextured tile or one-pixel rectangle. */
bool ps1_video_draw_tile(Ps1Video *video, const Ps1DrawState *state, int x, int y,
                         int width, int height, uint32_t rgb24, bool semi_transparent);

/* Read one raw texel through the GP0 texture page and CLUT fields. Zero is transparent. */
bool ps1_video_sample_texel(const Ps1Video *video, unsigned tpage, unsigned clut,
                            unsigned u, unsigned v, uint16_t *texel);

/* Draw a textured PS1 rectangle with raw or modulated color. */
bool ps1_video_draw_sprite(Ps1Video *video, const Ps1DrawState *state,
                           const Ps1Sprite *sprite);

/* Draw a flat or textured PS1 triangle. */
bool ps1_video_draw_triangle(Ps1Video *video, const Ps1DrawState *state,
                             const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
                             uint32_t rgb24, unsigned tpage, unsigned clut,
                             bool textured, bool raw_texture, bool semi_transparent);

/* Gouraud colors are interpolated before texture modulation. */
bool ps1_video_draw_textured_gouraud_triangle(Ps1Video *video,
    const Ps1DrawState *state, const Ps1Vertex *v0, const Ps1Vertex *v1,
    const Ps1Vertex *v2, const uint32_t colors[3], unsigned tpage, unsigned clut,
    bool raw_texture, bool semi_transparent);

/* Flat lines include both endpoints. */
bool ps1_video_draw_line(Ps1Video *video, const Ps1DrawState *state,
                         int x0, int y0, int x1, int y1, uint32_t rgb24, bool semi_transparent);
bool ps1_video_draw_gouraud_triangle(Ps1Video *video, const Ps1DrawState *state,
    const Ps1Vertex *v0, const Ps1Vertex *v1, const Ps1Vertex *v2,
    const uint32_t colors[3], bool semi_transparent);

/* Draw a flat or textured PS1 quad (split into triangles v0-v1-v2 and v1-v2-v3, or subdivided). */
bool ps1_video_draw_quad(Ps1Video *video, const Ps1DrawState *state,
                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                         uint32_t rgb24, unsigned tpage, unsigned clut,
                         bool textured, bool raw_texture, bool semi_transparent);

bool ps1_video_draw_gouraud_quad(Ps1Video *video, const Ps1DrawState *state,
                                const Ps1Vertex *v0, const Ps1Vertex *v1,
                                const Ps1Vertex *v2, const Ps1Vertex *v3,
                                const uint32_t colors[4], bool semi_transparent);

bool ps1_video_draw_textured_gouraud_quad(Ps1Video *video, const Ps1DrawState *state,
                                         const Ps1Vertex *v0, const Ps1Vertex *v1,
                                         const Ps1Vertex *v2, const Ps1Vertex *v3,
                                         const uint32_t colors[4], unsigned tpage, unsigned clut,
                                         bool raw_texture, bool semi_transparent);

#endif
