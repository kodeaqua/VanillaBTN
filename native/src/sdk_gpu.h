#ifndef VANILLABTN_SDK_GPU_H
#define VANILLABTN_SDK_GPU_H

#include "ps1_video.h"

/* The original SDK exposes a single GPU. The host owns VRAM and its lifetime;
 * bind NULL before destroying it. Transfers currently complete synchronously. */
void sdk_gpu_bind(Ps1Video *video);

#endif
