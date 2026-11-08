#ifndef VANILLABTN_SDK_GPU_H
#define VANILLABTN_SDK_GPU_H

#include "ps1_video.h"

/* The original SDK exposes a single GPU. The host owns VRAM and its lifetime;
 * bind NULL before destroying it. Transfers currently complete synchronously. */
void sdk_gpu_bind(Ps1Video *video);

/* Frame-local mappings preserve 24-bit links without truncating host pointers.
 * Register each complete OT/packet arena before any primitive is linked.
 * Reset only after all draws using these arenas have completed. */
void sdk_gpu_reset_packets(void);
bool sdk_gpu_map(void *memory, size_t size);
bool sdk_gpu_ok(void);

#endif
