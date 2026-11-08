#ifndef VANILLABTN_PS1_VIDEO_H
#define VANILLABTN_PS1_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS1_VRAM_WIDTH 1024
#define PS1_VRAM_HEIGHT 512

typedef struct Ps1Video {
    uint16_t vram[PS1_VRAM_HEIGHT][PS1_VRAM_WIDTH];
} Ps1Video;

typedef struct Ps1DrawState {
    int left, top, right, bottom; /* Inclusive drawing area in VRAM. */
    int offset_x, offset_y;       /* Signed 11-bit GP0 draw offset. */
    unsigned blend_mode;          /* GP0(E1) bits 5-6: average, add, subtract, add quarter. */
    bool force_mask, check_mask;  /* GP0(E6) bits 0-1. */
} Ps1DrawState;

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

#endif
