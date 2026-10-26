/* Hardware-backed SDL_GPU upload/blit/readback spike. No game assets or game
 * window are impersonated by this diagnostic. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

enum { HM_WIDTH = 320, HM_HEIGHT = 240, HM_BYTES = HM_WIDTH * HM_HEIGHT * 4 };
static Uint8 hm_pixels[HM_BYTES];

int main(void) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL video: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("HMBTN native graphics validation", 640, 480, 0);
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false,
        NULL);
    if (!window || !device || !SDL_ClaimWindowForGPUDevice(device, window) ||
        !SDL_SetGPUAllowedFramesInFlight(device, 2)) {
        fprintf(stderr, "SDL GPU: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("SDL_GPU backend: %s\n", SDL_GetGPUDeviceDriver(device));
    SDL_GPUTextureCreateInfo texture_info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                             .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                             .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                                      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                                             .width = HM_WIDTH,
                                             .height = HM_HEIGHT,
                                             .layer_count_or_depth = 1,
                                             .num_levels = 1};
    SDL_GPUTexture *source = SDL_CreateGPUTexture(device, &texture_info);
    SDL_GPUTexture *target = SDL_CreateGPUTexture(device, &texture_info);
    SDL_GPUTransferBufferCreateInfo upload_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                   .size = HM_BYTES};
    SDL_GPUTransferBufferCreateInfo download_info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                     .size = HM_BYTES};
    SDL_GPUTransferBuffer *upload = SDL_CreateGPUTransferBuffer(device, &upload_info);
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(device, &download_info);
    if (!source || !target || !upload || !download) {
        fprintf(stderr, "GPU resource creation: %s\n", SDL_GetError());
        return 1;
    }
    for (Uint32 y = 0; y < HM_HEIGHT; ++y)
        for (Uint32 x = 0; x < HM_WIDTH; ++x) {
            Uint8 *p = hm_pixels + (y * HM_WIDTH + x) * 4;
            p[0] = (Uint8)x;
            p[1] = (Uint8)y;
            p[2] = (x / 8 + y / 8) & 1 ? 255 : 0;
            p[3] = 255;
        }
    void *mapped = SDL_MapGPUTransferBuffer(device, upload, false);
    if (!mapped)
        return 1;
    memcpy(mapped, hm_pixels, HM_BYTES);
    SDL_UnmapGPUTransferBuffer(device, upload);
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    if (!command)
        return 1;
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    const SDL_GPUTextureTransferInfo from_cpu = {
        .transfer_buffer = upload, .pixels_per_row = HM_WIDTH, .rows_per_layer = HM_HEIGHT};
    const SDL_GPUTextureRegion source_region = {
        .texture = source, .w = HM_WIDTH, .h = HM_HEIGHT, .d = 1};
    SDL_UploadToGPUTexture(copy, &from_cpu, &source_region, false);
    SDL_EndGPUCopyPass(copy);
    const SDL_GPUBlitInfo test_blit = {
        .source = {.texture = source, .w = HM_WIDTH, .h = HM_HEIGHT},
        .destination = {.texture = target, .w = HM_WIDTH, .h = HM_HEIGHT},
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .filter = SDL_GPU_FILTER_NEAREST};
    SDL_BlitGPUTexture(command, &test_blit);
    copy = SDL_BeginGPUCopyPass(command);
    const SDL_GPUTextureRegion target_region = {
        .texture = target, .w = HM_WIDTH, .h = HM_HEIGHT, .d = 1};
    const SDL_GPUTextureTransferInfo to_cpu = {
        .transfer_buffer = download, .pixels_per_row = HM_WIDTH, .rows_per_layer = HM_HEIGHT};
    SDL_DownloadFromGPUTexture(copy, &target_region, &to_cpu);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!fence || !SDL_WaitForGPUFences(device, true, &fence, 1))
        return 1;
    SDL_ReleaseGPUFence(device, fence);
    mapped = SDL_MapGPUTransferBuffer(device, download, false);
    if (!mapped || memcmp(mapped, hm_pixels, HM_BYTES)) {
        fprintf(stderr, "GPU blit readback differs from the upload\n");
        return 1;
    }
    SDL_UnmapGPUTransferBuffer(device, download);
    for (int frame = 0; frame < 8; ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                frame = 8;
        }
        command = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUTexture *swapchain = NULL;
        Uint32 width, height;
        if (!command ||
            !SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height))
            return 1;
        if (swapchain) {
            SDL_GPUBlitInfo present = test_blit;
            present.destination.texture = swapchain;
            present.destination.w = width;
            present.destination.h = height;
            SDL_BlitGPUTexture(command, &present);
        }
        if (!SDL_SubmitGPUCommandBuffer(command))
            return 1;
    }
    SDL_WaitForGPUIdle(device);
    SDL_ReleaseGPUTransferBuffer(device, download);
    SDL_ReleaseGPUTransferBuffer(device, upload);
    SDL_ReleaseGPUTexture(device, target);
    SDL_ReleaseGPUTexture(device, source);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("GPU upload, blit, readback and presentation passed");
    return 0;
}
