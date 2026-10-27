#include "platform/gpu_present.h"
void hm_gpu_present_image(SDL_GPUCommandBuffer *command, SDL_GPUTexture *image, u32 image_width,
                          u32 image_height, SDL_GPUTexture *swapchain, Uint32 width, Uint32 height,
                          int linear, int wide, int widescreen) {
    Uint32 destination_width = width, destination_height = height;
    if (!wide && (u64)width * 3 > (u64)height * 4)
        destination_width = height * 4 / 3;
    else if (!wide)
        destination_height = width * 3 / 4;
    /* An unavailable private frame has no side pixels to display. Never invent
     * them by stretching one edge column during scene fades or loading. */
    (void)widescreen;

    SDL_GPUBlitInfo blit = {.source = {.texture = image, .w = image_width, .h = image_height},
                            .destination = {.texture = swapchain,
                                            .x = (width - destination_width) / 2,
                                            .y = (height - destination_height) / 2,
                                            .w = destination_width,
                                            .h = destination_height},
                            .load_op = SDL_GPU_LOADOP_CLEAR,
                            .clear_color = {0, 0, 0, 1},
                            .filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST};
    SDL_BlitGPUTexture(command, &blit);
}
