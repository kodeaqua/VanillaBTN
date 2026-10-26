#ifndef HMBTN_GPU_PRESENT_H
#define HMBTN_GPU_PRESENT_H
#include "compat/types.h"
#include <SDL3/SDL.h>
/* Complete canvas presentation. Native content stays proportional; missing
 * horizontal background uses only its original edge columns. */
void hm_gpu_present_image(SDL_GPUCommandBuffer *command, SDL_GPUTexture *image, u32 image_width,
                          u32 image_height, SDL_GPUTexture *target, Uint32 width, Uint32 height,
                          int linear, int expanded, int widescreen);
#endif
