#ifndef HMBTN_GPU_RENDERER_H
#define HMBTN_GPU_RENDERER_H
#include "gfx/gpu_scene.h"
#include <SDL3/SDL.h>
typedef struct hm_gpu_renderer hm_gpu_renderer;
hm_gpu_renderer *hm_gpu_renderer_create(SDL_GPUDevice *device);
void hm_gpu_renderer_destroy(hm_gpu_renderer *renderer);
/* Record upload/compute without a readback or game-tick wait. Caller owns the
 * command buffer and may immediately blit the returned texture. */
SDL_GPUTexture *hm_gpu_renderer_encode(hm_gpu_renderer *renderer, SDL_GPUCommandBuffer *command,
                                       const hm_gpu_scene_data *scene);
#endif
