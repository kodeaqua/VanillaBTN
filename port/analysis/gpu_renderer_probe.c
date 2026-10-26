/* Offscreen, synthetic GPU readback. Never opens a window or starts the game. */
#include "platform/gpu_renderer.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#ifdef HM_SCALE_REFERENCE
#include "enhance/scaling/raster.h"
#endif
#include <SDL3/SDL_main.h>
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s (%s)\n", __LINE__, #x, SDL_GetError());              \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static u8 actual[4096 * 2160 * 4];
static u16 reference[4096 * 2160];
static int origin_y, display_test;
static hm_vram_context saved_vram, work_vram;
static u32 xy(int x, int y) { return (u16)x | ((u32)(u16)y << 16); }
static void setup(void) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, origin_y, 32, 32);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    if (display_test) {
        DISPENV disp;
        SetDefDispEnv(&disp, 0, origin_y, 32, 32);
        PutDispEnv(&disp);
        SetDispMask(1);
    }
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            hm_vram_draw_pixel(x, y + (u32)origin_y, 0x4210);
    for (u32 y = 0; y < 256; ++y)
        for (u32 x = 0; x < 256; ++x)
            hm_vram_draw_pixel(640 + x, 256 + y,
                               (u16)(((x + y) % 13)
                                         ? (1 + ((x * 7 + y * 3) % 32767)) | ((x & 1) ? 0x8000 : 0)
                                         : 0));
    for (u32 i = 0; i < 256; ++i)
        hm_vram_draw_pixel(896 + i, 252, i % 7 ? (u16)(i * 113 + 0x8001) : 0);
}
static int readback(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                    const hm_gpu_scene_data *scene, u8 *pixels) {
    SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                            .size = scene->width * scene->height * 4};
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(device, &info);
    CHECK(download);
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    CHECK(command);
    SDL_GPUTexture *image = hm_gpu_renderer_encode(renderer, command, scene);
    CHECK(image);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    CHECK(copy);
    SDL_GPUTextureRegion from = {.texture = image, .w = scene->width, .h = scene->height, .d = 1};
    SDL_GPUTextureTransferInfo to = {.transfer_buffer = download,
                                     .pixels_per_row = scene->width,
                                     .rows_per_layer = scene->height};
    SDL_DownloadFromGPUTexture(copy, &from, &to);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    CHECK(fence);
    CHECK(SDL_WaitForGPUFences(device, true, &fence, 1));
    SDL_ReleaseGPUFence(device, fence);
    const void *mapped = SDL_MapGPUTransferBuffer(device, download, false);
    CHECK(mapped);
    SDL_memcpy(pixels, mapped, info.size);
    SDL_UnmapGPUTransferBuffer(device, download);
    SDL_ReleaseGPUTransferBuffer(device, download);
    return 0;
}
static int fixture(SDL_GPUDevice *device, hm_gpu_renderer *renderer, hm_gpu_scene *builder,
                   const u32 *words, u32 count, u32 width, u32 height, int tolerance) {
    setup();
    hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
    RECT area = {0, (s16)origin_y, 32, 32};
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    setup();
    hm_gpu_execute_words(words, count);
    if (width == 32 && height == 32) {
        for (u32 y = 0; y < 32; ++y)
            for (u32 x = 0; x < 32; ++x)
                reference[y * 32 + x] = hm_vram_pixel(x, y + (u32)origin_y);
    }
#ifdef HM_SCALE_REFERENCE
    else {
        setup();
        hm_scaled_surface surface = {.pixels = reference, .width = width, .height = height};
        previous = hm_replay_bind(hm_scaled_sink(&surface));
        hm_replay_begin(&area);
        hm_gpu_execute_words(words, count);
        hm_replay_bind(previous);
    }
#endif
    CHECK(readback(device, renderer, &scene, actual) == 0);
    u32 differences = 0;
    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x) {
            u16 color = reference[y * width + x];
            for (u32 c = 0; c < 3; ++c) {
                u32 v = (color >> (c * 5)) & 31;
                int expected = (int)((v << 3) | (v >> 2));
                int got = actual[(y * width + x) * 4 + c];
                if (SDL_abs(got - expected) > tolerance) {
                    if (differences++ < 3)
                        fprintf(stderr, "Difference %u,%u channel %u: GPU %d CPU %d\n", x, y, c,
                                got, expected);
                }
            }
            CHECK(actual[(y * width + x) * 4 + 3] == 255);
        }
    CHECK(!differences);
    return 0;
}
static u32 hm_probe_random(void) {
    static u32 value = 0x98765432;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}
static int hm_probe_random_cases(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                 hm_gpu_scene *builder) {
    for (int batch = 0; batch < 32; ++batch) {
        u32 commands[1024], n = 0;
        origin_y = batch & 1 ? 240 : 0;
        for (int i = 0; i < 64; ++i) {
            u32 page = 282 | ((hm_probe_random() & 3) << 5);
            commands[n++] = 0xe1000600 | page;
            if (i % 8 == 0)
                commands[n++] = 0xe6000000 | (hm_probe_random() & 3);
            u32 type = hm_probe_random() % 4, op = type == 0   ? 0x20
                                                   : type == 1 ? 0x30
                                                   : type == 2 ? 0x62
                                                               : 0x27;
            commands[n++] = (op << 24) | (hm_probe_random() & 0xffffff);
            if (type == 2) {
                commands[n++] =
                    xy((int)(hm_probe_random() % 48) - 8, (int)(hm_probe_random() % 48) - 8);
                commands[n++] = xy(1 + (hm_probe_random() % 16), 1 + (hm_probe_random() % 16));
            } else
                for (int vertex = 0; vertex < 3; ++vertex) {
                    if (vertex && type == 1)
                        commands[n++] = hm_probe_random() & 0xffffff;
                    commands[n++] =
                        xy((int)(hm_probe_random() % 64) - 16, (int)(hm_probe_random() % 64) - 16);
                    if (type == 3)
                        commands[n++] =
                            (hm_probe_random() & 65535) | ((vertex == 0   ? ((252 << 6) | 56)
                                                            : vertex == 1 ? page
                                                                          : 0)
                                                           << 16);
                }
        }
        CHECK(n < 1024 && !fixture(device, renderer, builder, commands, n, 32, 32, 0));
    }
    origin_y = 0;
    return 0;
}
static int hm_probe_motion(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                           hm_gpu_scene *builder) {
    setup();
    u32 old[] = {0x600000ff, xy(15, 2), xy(4, 4)}, current[] = {0x600000ff, xy(16, 2), xy(4, 4)};
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 64, 64);
    RECT area = {0, 0, 32, 32};
    hm_replay_begin(&area);
    hm_replay_motion(old, current, 3, 32768);
    hm_gpu_execute_words(current, 3);
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    CHECK(!readback(device, renderer, &scene, actual));
    CHECK(actual[(4 * 64 + 31) * 4] == 255 && actual[(4 * 64 + 30) * 4] == 132);
    return 0;
}
static int hm_probe_benchmark(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                              hm_gpu_scene *builder) {
    const u32 sizes[][2] = {{320, 240}, {640, 480}, {1440, 1080}};
    u32 triangle[] = {0x200000ff, xy(2, 2), xy(28, 5), xy(6, 28)};
    RECT area = {0, 0, 32, 32};
    const int iterations = 50;
    setup();
    hm_vram_snapshot(&saved_vram);
    hm_vram_context *old = hm_vram_bind(&work_vram);
    for (int size = 0; size < 3; ++size) {
#ifdef HM_SCALE_REFERENCE
        u64 start = SDL_GetTicksNS();
        for (int frame = 0; frame < iterations; ++frame) {
            work_vram = saved_vram;
            hm_scaled_surface surface = {
                .pixels = reference, .width = sizes[size][0], .height = sizes[size][1]};
            hm_replay_sink previous = hm_replay_bind(hm_scaled_sink(&surface));
            hm_replay_begin(&area);
            hm_gpu_execute_words(triangle, 4);
            hm_replay_bind(previous);
        }
        printf("CPU %ux%u: %.3f ms/render\n", sizes[size][0], sizes[size][1],
               (double)(SDL_GetTicksNS() - start) / 1e6 / iterations);
#endif
        u64 gpu_start = SDL_GetTicksNS();
        for (int frame = 0; frame < iterations; ++frame) {
            work_vram = saved_vram;
            hm_replay_sink previous = hm_gpu_scene_begin(builder, sizes[size][0], sizes[size][1]);
            hm_replay_begin(&area);
            hm_gpu_execute_words(triangle, 4);
            hm_gpu_scene_data data;
            CHECK(hm_gpu_scene_end(builder, previous, &data));
            SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
            CHECK(command);
            CHECK(hm_gpu_renderer_encode(renderer, command, &data));
            SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            CHECK(fence);
            CHECK(SDL_WaitForGPUFences(device, true, &fence, 1));
            SDL_ReleaseGPUFence(device, fence);
        }
        printf("GPU capture/upload/compute/fence %ux%u: %.3f ms/render\n", sizes[size][0],
               sizes[size][1], (double)(SDL_GetTicksNS() - gpu_start) / 1e6 / iterations);
    }
    hm_vram_bind(old);
    return 0;
}
int main(int argc, char **argv) {
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    const char *driver = argc > 1 ? argv[1] : NULL;
    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, driver);
    CHECK(device);
    hm_gpu_renderer *renderer = hm_gpu_renderer_create(device);
    CHECK(renderer);
    hm_gpu_scene *builder = hm_gpu_scene_create();
    CHECK(builder);
    printf("Offscreen GPU renderer: %s\n", SDL_GetGPUDeviceDriver(device));
    u32 shapes[] = {0x200000ff, xy(1, 1),   xy(26, 4), xy(5, 26),  0x300000ff,
                    xy(3, 3),   0x0000ff00, xy(29, 7), 0x00ff0000, xy(8, 29),
                    0x2800ff00, xy(2, 2),   xy(12, 3), xy(3, 13),  xy(13, 14)};
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 32, 32, 0));
    origin_y = 240;
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 32, 32, 0));
    origin_y = 0;
    display_test = 1;
    u32 display[] = {0xe1000000, 0x600000ff, 0,        xy(30, 30),
                     0xe1000400, 0x6000ff00, xy(4, 4), xy(20, 20)};
    CHECK(!fixture(device, renderer, builder, display, sizeof(display) / sizeof(*display), 32, 32,
                   0));
    display_test = 0;
    for (u32 mode = 0; mode < 4; ++mode) {
        u32 blend[] = {0xe1000000 | (mode << 5),
                       0x600000ff,
                       xy(-2, -2),
                       xy(20, 20),
                       0x6200ff00,
                       xy(4, 4),
                       xy(20, 20),
                       0xe6000001,
                       0x60ff0000,
                       xy(7, 7),
                       xy(8, 8),
                       0xe6000002,
                       0x6000ff00,
                       xy(7, 7),
                       xy(8, 8)};
        CHECK(
            !fixture(device, renderer, builder, blend, sizeof(blend) / sizeof(*blend), 32, 32, 0));
    }
    u32 lines[] = {0xe1000200, 0x400000ff, xy(1, 1), xy(29, 29), 0x500000ff, xy(1, 28), 0x0000ff00,
                   xy(28, 1),  0x4800ff00, xy(2, 2), xy(28, 2),  xy(28, 28), 0x55555555};
    CHECK(!fixture(device, renderer, builder, lines, sizeof(lines) / sizeof(*lines), 32, 32, 0));
    u32 clipping[] = {0xe3000000 | 4 | (3 << 10),
                      0xe4000000 | 22 | (23 << 10),
                      0x200000ff,
                      xy(-8, -8),
                      xy(30, 0),
                      xy(0, 30)};
    CHECK(!fixture(device, renderer, builder, clipping, sizeof(clipping) / sizeof(*clipping), 32,
                   32, 0));
    for (u32 depth = 0; depth < 3; ++depth)
        for (u32 mode = 0; mode < 4; ++mode) {
            u32 page = 26 | (depth << 7) | (mode << 5), clut = (252 << 6) | 56;
            u32 textures[] = {0xe1000000 | page,
                              0xe2000000 | 1 | (3 << 5) | (4 << 10) | (5 << 15),
                              0x660000ff,
                              xy(1, 1),
                              (clut << 16),
                              xy(24, 24),
                              0x27808080,
                              xy(2, 2),
                              (clut << 16),
                              xy(28, 3),
                              (page << 16) | 60,
                              xy(3, 28),
                              0x3c00};
            CHECK(!fixture(device, renderer, builder, textures,
                           sizeof(textures) / sizeof(*textures), 32, 32, 0));
        }
    CHECK(!hm_probe_random_cases(device, renderer, builder));
    CHECK(!hm_probe_motion(device, renderer, builder));
#ifdef HM_SCALE_REFERENCE
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 64, 64, 0));
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 1440, 1080,
                   9));
#endif
    if (argc > 2 && !SDL_strcmp(argv[2], "--benchmark"))
        CHECK(!hm_probe_benchmark(device, renderer, builder));
    hm_gpu_scene_destroy(builder);
    SDL_WaitForGPUIdle(device);
    hm_gpu_renderer_destroy(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    puts("Synthetic native GPU readback comparisons passed; no game/window started.");
    return 0;
}
