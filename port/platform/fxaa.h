#ifndef HMBTN_PLATFORM_FXAA_H
#define HMBTN_PLATFORM_FXAA_H
#include "compat/types.h"
#include <SDL3/SDL.h>
typedef struct hm_fxaa hm_fxaa;
hm_fxaa *hm_fxaa_create(SDL_GPUDevice *device);
void hm_fxaa_destroy(hm_fxaa *fxaa);
SDL_GPUTexture *hm_fxaa_encode(hm_fxaa *fxaa, SDL_GPUCommandBuffer *command,
                               SDL_GPUTexture *source, u32 width, u32 height);
#endif
