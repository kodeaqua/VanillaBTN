#ifndef VANILLABTN_SDK_GPU_H
#define VANILLABTN_SDK_GPU_H

#include "ps1_video.h"

/* The original SDK exposes a single GPU. The host owns VRAM and its lifetime;
 * bind NULL before destroying it. Transfers currently complete synchronously. */
void sdk_gpu_bind(Ps1Video *video);

/* Begin a new original-game frame. The game reuses its packet arenas, so PGXP
 * provenance from the previous frame must not survive into the next one. */
void sdk_gpu_begin_frame(void);

/* Temporarily draw a known 2D background in the full 16:9 presentation width
 * instead of the centered 4:3 HUD safe area. */
void sdk_gpu_set_2d_widescreen_expansion(bool enabled);

/* Frame-local mappings preserve 24-bit links without truncating host pointers.
 * Prefer registering complete packet arenas before primitives are linked.
 * SDK calls also map module-owned OTs/private static packets lazily.
 * Reset only after all draws using these arenas have completed. */
void sdk_gpu_reset_packets(void);
bool sdk_gpu_map(void *memory, size_t size);
bool sdk_gpu_ok(void);

typedef struct SdkDisplay {
    int x, y, width, height;
    bool enabled;
} SdkDisplay;
SdkDisplay sdk_gpu_display(void);

#endif
