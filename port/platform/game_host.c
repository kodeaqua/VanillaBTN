#include "platform/game_host.h"
#include "platform/audio_output.h"
#include "platform/input_replay.h"
#include "platform/user_data.h"
#include "psx/gpu_environment.h"
#include "psx/pad.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Window *hm_window;
static SDL_GPUDevice *hm_device;
static SDL_GPUTexture *hm_texture;
static SDL_GPUTransferBuffer *hm_upload;
static Uint64 hm_start;
static hm_input_replay hm_replay;
static int hm_replay_enabled;
static u64 hm_host_tick_limit;
enum { HM_SCAN_WIDTH = 1024, HM_SCAN_HEIGHT = 512, HM_SCAN_BYTES = 1024 * 512 * 4 };

static void hm_host_fail(void) {
    fprintf(stderr, "Native window: %s\n", SDL_GetError());
    hm_game_host_stop();
    exit(1);
}

static void hm_host_tick(void *context, u64 tick) {
    (void)context;
    if (hm_host_tick_limit && tick > hm_host_tick_limit) {
        printf("Native window completed %llu VBlanks\n", (unsigned long long)hm_host_tick_limit);
        hm_game_host_stop();
        exit(0);
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            hm_game_host_stop();
            exit(0);
        }
    }
    const bool *keys = SDL_GetKeyboardState(NULL);
    static const SDL_Scancode mapping[16] = {
        SDL_SCANCODE_Q,         SDL_SCANCODE_E,       SDL_SCANCODE_A,       SDL_SCANCODE_S,
        SDL_SCANCODE_W,         SDL_SCANCODE_D,       SDL_SCANCODE_X,       SDL_SCANCODE_Z,
        SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RETURN,
        SDL_SCANCODE_UP,        SDL_SCANCODE_RIGHT,   SDL_SCANCODE_DOWN,    SDL_SCANCODE_LEFT};
    u16 held = 0;
    for (u32 i = 0; i < 16; ++i)
        if (mapping[i] != SDL_SCANCODE_UNKNOWN && keys[mapping[i]])
            held |= (u16)(1u << i);
    if (hm_replay_enabled)
        held = hm_input_replay_sample(&hm_replay, tick);
    hm_pad_sample(0, 1, held);
    Uint64 deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;
    Uint64 now = SDL_GetTicksNS();
    if (now < deadline)
        SDL_DelayPrecise(deadline - now);

    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
    if (SDL_getenv("HMBTN_TRACE_SCANOUT") && tick % 60 == 0) {
        fprintf(stderr, "Scanout tick %llu: enabled=%d area=%d,%d %dx%d\n",
                (unsigned long long)tick, state->display_enabled, area->x, area->y, area->w,
                area->h);
    }
    if (!area->w || !area->h)
        return;
    if (area->w < 0 || area->w > HM_SCAN_WIDTH || area->h < 0 || area->h > HM_SCAN_HEIGHT) {
        SDL_SetError("unsupported display dimensions");
        hm_host_fail();
    }
    u8 *pixels = SDL_MapGPUTransferBuffer(hm_device, hm_upload, true);
    if (!pixels)
        hm_host_fail();
    for (u32 y = 0; y < (u32)area->h; ++y)
        for (u32 x = 0; x < (u32)area->w; ++x) {
            u16 color =
                state->display_enabled ? hm_vram_pixel((u32)area->x + x, (u32)area->y + y) : 0;
            u8 *out = pixels + (y * (u32)area->w + x) * 4;
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 component = (color >> (channel * 5)) & 31;
                out[channel] = (u8)((component << 3) | (component >> 2));
            }
            out[3] = 255;
        }
    const char *capture = SDL_getenv("HMBTN_CAPTURE_FRAME");
    if (capture) {
        SDL_Surface *surface =
            SDL_CreateSurfaceFrom(area->w, area->h, SDL_PIXELFORMAT_RGBA32, pixels, area->w * 4);
        if (!surface || !SDL_SaveBMP(surface, capture))
            hm_host_fail();
        SDL_DestroySurface(surface);
    }
    SDL_UnmapGPUTransferBuffer(hm_device, hm_upload);
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo source = {.transfer_buffer = hm_upload,
                                         .pixels_per_row = (u32)area->w,
                                         .rows_per_layer = (u32)area->h};
    SDL_GPUTextureRegion region = {
        .texture = hm_texture, .w = (u32)area->w, .h = (u32)area->h, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &region, true);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUTexture *swapchain = NULL;
    Uint32 width, height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, hm_window, &swapchain, &width, &height))
        hm_host_fail();
    if (swapchain) {
        SDL_GPUBlitInfo blit = {
            .source = {.texture = hm_texture, .w = (u32)area->w, .h = (u32)area->h},
            .destination = {.texture = swapchain, .w = width, .h = height},
            .load_op = SDL_GPU_LOADOP_DONT_CARE,
            .filter = SDL_GPU_FILTER_NEAREST};
        SDL_BlitGPUTexture(command, &blit);
    }
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
}

int hm_game_host_start(void) {
    const char *limit = SDL_getenv("HMBTN_BOOT_TICKS");
    if (limit) {
        char *end;
        hm_host_tick_limit = strtoull(limit, &end, 10);
        if (!*limit || *end || !hm_host_tick_limit || hm_host_tick_limit > 1000000) {
            fprintf(stderr, "Invalid diagnostic tick limit\n");
            return 0;
        }
    }
    const char *replay_path = SDL_getenv("HMBTN_INPUT_REPLAY");
    if (replay_path) {
        if (!hm_input_replay_load(&hm_replay, replay_path)) {
            fprintf(stderr, "Cannot load input replay\n");
            return 0;
        }
        hm_replay_enabled = 1;
    }
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
        return 0;
    if (!hm_user_data_start()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature",
                                 "Cannot open Documents/BackToNature. Check folder permissions and "
                                 "close any other game instance.",
                                 NULL);
        return 0;
    }
    hm_window = SDL_CreateWindow("Harvest Moon: Back to Nature - native bring-up", 640, 480, 0);
    hm_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                        SDL_GPU_SHADERFORMAT_MSL,
                                    false, NULL);
    if (!hm_window || !hm_device || !SDL_ClaimWindowForGPUDevice(hm_device, hm_window) ||
        !SDL_SetGPUAllowedFramesInFlight(hm_device, 2))
        return 0;
    SDL_GPUTextureCreateInfo texture = {.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        .width = HM_SCAN_WIDTH,
                                        .height = HM_SCAN_HEIGHT,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    hm_texture = SDL_CreateGPUTexture(hm_device, &texture);
    SDL_GPUTransferBufferCreateInfo transfer = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                .size = HM_SCAN_BYTES};
    hm_upload = SDL_CreateGPUTransferBuffer(hm_device, &transfer);
    if (!hm_texture || !hm_upload)
        return 0;
    if (!hm_audio_output_start()) {
        fprintf(stderr, "Native audio: %s\n", SDL_GetError());
        return 0;
    }
    hm_start = SDL_GetTicksNS();
    hm_timing_reset((hm_timing_host){NULL, hm_host_tick, NULL});
    printf("Native game window using SDL_GPU %s\n", SDL_GetGPUDeviceDriver(hm_device));
    fflush(stdout);
    return 1;
}

void hm_game_host_stop(void) {
    hm_audio_output_stop();
    if (hm_device) {
        SDL_WaitForGPUIdle(hm_device);
        if (hm_upload)
            SDL_ReleaseGPUTransferBuffer(hm_device, hm_upload);
        if (hm_texture)
            SDL_ReleaseGPUTexture(hm_device, hm_texture);
        if (hm_window)
            SDL_ReleaseWindowFromGPUDevice(hm_device, hm_window);
        SDL_DestroyGPUDevice(hm_device);
    }
    if (hm_window)
        SDL_DestroyWindow(hm_window);
    hm_device = NULL;
    hm_window = NULL;
    hm_upload = NULL;
    hm_texture = NULL;
    hm_user_data_stop();
    SDL_Quit();
}
