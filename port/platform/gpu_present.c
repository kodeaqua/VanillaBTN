#include "platform/gpu_present.h"

#ifdef __ANDROID__
static Uint32 hm_present_last_w, hm_present_last_h, hm_present_last_dst_w, hm_present_last_dst_h;
static int hm_present_clear_frames = 8;
#endif

void hm_gpu_present_invalidate(void) {
#ifdef __ANDROID__
    hm_present_clear_frames = 8;
#endif
}

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

    SDL_GPULoadOp load_op = (destination_width == width && destination_height == height)
                                ? SDL_GPU_LOADOP_DONT_CARE
                                : SDL_GPU_LOADOP_CLEAR;
#ifdef __ANDROID__
    /* Adreno 6xx workaround: SDL_BlitGPUTexture with SDL_GPU_LOADOP_CLEAR starts and
     * ends an empty full-screen RenderPass before vkCmdBlitImage, forcing a full GMEM
     * tile-cache resolve to system RAM and a pipeline barrier every frame. Once all
     * swapchain ring buffers have had their pillarbox borders cleared, use DONT_CARE. */
    if (width != hm_present_last_w || height != hm_present_last_h ||
        destination_width != hm_present_last_dst_w || destination_height != hm_present_last_dst_h) {
        hm_present_last_w = width;
        hm_present_last_h = height;
        hm_present_last_dst_w = destination_width;
        hm_present_last_dst_h = destination_height;
        hm_present_clear_frames = 8;
    }
    if (hm_present_clear_frames > 0) {
        --hm_present_clear_frames;
    } else {
        load_op = SDL_GPU_LOADOP_DONT_CARE;
    }
#endif

    SDL_GPUBlitInfo blit = {.source = {.texture = image, .w = image_width, .h = image_height},
                            .destination = {.texture = swapchain,
                                            .x = (width - destination_width) / 2,
                                            .y = (height - destination_height) / 2,
                                            .w = destination_width,
                                            .h = destination_height},
                            .load_op = load_op,
                            .clear_color = {0, 0, 0, 1},
                            .filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST};
    SDL_BlitGPUTexture(command, &blit);
}
