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

/* Preserve raw PS1 BGR555 words, including the high mask/semitransparency bit. */
bool ps1_video_upload(Ps1Video *video, int x, int y, int width, int height,
                      const uint16_t *source, size_t source_stride);

/* Read a display rectangle as opaque ARGB8888; the high PS1 bit is not color. */
bool ps1_video_copy_argb(const Ps1Video *video, int x, int y, int width, int height,
                         uint32_t *target, size_t target_stride);

#endif
