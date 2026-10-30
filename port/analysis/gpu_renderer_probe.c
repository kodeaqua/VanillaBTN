#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
/* Offscreen, synthetic GPU readback. Never opens a window or starts the game. */
#include "platform/gpu_present.h"
#include "platform/gpu_renderer.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include "enhance/dithering/dithering.h"
#ifdef HM_INTERPOLATION_REFERENCE
#include "enhance/interpolation/interpolation.h"
#include "gfx/packet_identity.h"
#endif
#include <stdlib.h>
#ifdef HM_SCALE_REFERENCE
#include "gfx/software_raster.h"
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
static int origin_y, display_test, texture_linear;
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
    for (u32 i = 0; i < 16; ++i)
        hm_vram_draw_pixel(896 + i, 490, i ? (u16)(0x8000 | i * 1057) : 0);
    for (u32 y = 0; y < 16; ++y)
        for (u32 x = 0; x < 8; ++x) {
            hm_vram_draw_pixel(x, 240 + y, 0x1234);
            hm_vram_draw_pixel(896 + x, y, 0x3211);
        }
}
static int download_image(SDL_GPUDevice *device, SDL_GPUCommandBuffer *command,
                          SDL_GPUTexture *image, u32 width, u32 height, u8 *pixels) {
    SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                            .size = width * height * 4};
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(device, &info);
    CHECK(download);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    CHECK(copy);
    SDL_GPUTextureRegion from = {.texture = image, .w = width, .h = height, .d = 1};
    SDL_GPUTextureTransferInfo to = {
        .transfer_buffer = download, .pixels_per_row = width, .rows_per_layer = height};
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
static int readback(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                    const hm_gpu_scene_data *scene, u8 *pixels) {
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    CHECK(command);
    SDL_GPUTexture *image = hm_gpu_renderer_encode(renderer, command, scene);
    CHECK(image);
    return download_image(device, command, image, scene->width, scene->height, pixels);
}
static int hm_probe_gradient(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                             hm_gpu_scene *builder) {
    const u32 gradient[] = {0x30000000, xy(0, 0), 0x00ffffff, xy(31, 0),
                            0x00000000, xy(0, 31),
                            0x600000a0, xy(4, 4), xy(4, 4),
                            0x62002020, xy(10, 4), xy(4, 4)};
    u8 native[32 * 32 * 4];
    for (int enhanced = 0; enhanced < 2; ++enhanced) {
        hm_dither_set_enabled(!enhanced);
        setup();
        RECT area = {0, 0, 32, 32};
        hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
        hm_replay_begin(&area);
        hm_gpu_execute_words(gradient, sizeof gradient / sizeof *gradient);
        hm_gpu_scene_data data;
        CHECK(hm_gpu_scene_end(builder, previous, &data));
        CHECK(!readback(device, renderer, &data, actual));
        if (!enhanced) {
            memcpy(native, actual, sizeof native);
            setup();
            hm_gpu_execute_words(gradient, sizeof gradient / sizeof *gradient);
            for (int x = 1; x < 29; ++x) {
                u16 pixel = hm_vram_pixel((u32)x, 1);
                u8 channel = (u8)(((pixel & 31) << 3) | ((pixel & 31) >> 2));
                /* Existing GPU/CPU interpolation can differ by one 5-bit step. */
                CHECK(abs((int)native[(32 + x) * 4] - channel) <= 9);
            }
        } else {
            int improved = 0;
            for (int x = 1; x < 29; ++x)
                if (actual[(32 + x) * 4] != native[(32 + x) * 4]) ++improved;
            CHECK(improved > 10);
            for (int y = 4; y < 8; ++y)
                for (int x = 4; x < 14; ++x)
                    if (x < 8 || x >= 10)
                        CHECK(!memcmp(actual + (y * 32 + x) * 4,
                                      native + (y * 32 + x) * 4, 4));
        }
    }
    hm_dither_set_enabled(1);
    puts("Opaque gradient precision and native reference passed.");
    return 0;
}
static int hm_probe_texture_precision(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                      hm_gpu_scene *builder) {
    const u32 triangle[] = {0x34000040, xy(0, 0), 0, 0x000000c0, xy(31, 0),
                            282u << 16, 0x00000040, xy(0, 31), 0};
    u8 native[32 * 32 * 4];
    for (int enhanced = 0; enhanced < 2; ++enhanced) {
        hm_dither_set_enabled(!enhanced);
        setup();
        hm_vram_draw_pixel(640, 256, 0x7fff);
        RECT area = {0, 0, 32, 32};
        hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
        hm_replay_begin(&area);
        hm_gpu_execute_words(triangle, sizeof triangle / sizeof *triangle);
        hm_gpu_scene_data data;
        CHECK(hm_gpu_scene_end(builder, previous, &data));
        CHECK(!readback(device, renderer, &data, actual));
        if (!enhanced) {
            memcpy(native, actual, sizeof native);
            setup();
            hm_vram_draw_pixel(640, 256, 0x7fff);
            hm_gpu_execute_words(triangle, sizeof triangle / sizeof *triangle);
            for (int x = 1; x < 29; ++x) {
                u16 pixel = hm_vram_pixel((u32)x, 1);
                u8 channel = (u8)(((pixel & 31) << 3) | ((pixel & 31) >> 2));
                CHECK(abs((int)native[(32 + x) * 4] - channel) <= 9);
            }
        } else {
            int improved = 0;
            for (int x = 1; x < 29; ++x)
                if (actual[(32 + x) * 4] != native[(32 + x) * 4]) ++improved;
            CHECK(improved > 10);
        }
    }
    hm_dither_set_enabled(1);
    puts("Opaque textured gradient precision passed.");
    return 0;
}
static int hm_probe_presentation(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                 hm_gpu_scene *builder) {
    setup();
    RECT area = {0, 0, 32, 32};
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
    hm_replay_begin(&area);
    u32 colors[] = {0x600000ff, 0,          0x00200020, 0x6000ff00, 8,
                    0x00200010, 0x60ff0000, 24,         0x00200008};
    hm_gpu_execute_words(colors, sizeof colors / sizeof *colors);
    hm_gpu_scene_data data;
    CHECK(hm_gpu_scene_end(builder, previous, &data));
    SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                     .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                     .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                              SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                     .width = 128,
                                     .height = 48,
                                     .layer_count_or_depth = 1,
                                     .num_levels = 1,
                                     .sample_count = SDL_GPU_SAMPLECOUNT_1};
    SDL_GPUTexture *target = SDL_CreateGPUTexture(device, &info);
    CHECK(target);
    for (int mode = 0; mode < 4; ++mode) {
        SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
        CHECK(command);
        SDL_GPUTexture *image = hm_gpu_renderer_encode(renderer, command, &data);
        CHECK(image);
        hm_gpu_present_image(command, image, 32, 32, target, 128, 48, 0, mode == 2, mode != 0);
        CHECK(!download_image(device, command, target, 128, 48, actual));
        u32 row = 24 * 128 * 4;
        /* Only an already expanded image may fill the wide destination. A
         * widescreen request alone must never stretch a native image's edges. */
        CHECK(actual[row] == (mode == 2 ? 255 : 0));
        CHECK(actual[row + 127 * 4 + 2] == (mode == 2 ? 255 : 0));
        CHECK(actual[row + 64 * 4 + 1] == 255);
        if (mode != 2) {
            CHECK(actual[row + 47 * 4] == 255);
            CHECK(actual[row + 48 * 4 + 1] == 255);
        }
    }
    SDL_ReleaseGPUTexture(device, target);
    puts("Whole-canvas presentation and GPU/native fallback aspect comparisons passed");
    return 0;
}
static int fixture(SDL_GPUDevice *device, hm_gpu_renderer *renderer, hm_gpu_scene *builder,
                   const u32 *words, u32 count, u32 width, u32 height, int tolerance) {
    setup();
    hm_gpu_scene_texture_filter(builder, texture_linear);
    hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
    RECT area = {0, (s16)origin_y, 32, 32};
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    setup();
    hm_gpu_execute_words(words, count);
    if (width == 32 && height == 32 && !texture_linear) {
        for (u32 y = 0; y < 32; ++y)
            for (u32 x = 0; x < 32; ++x)
                reference[y * 32 + x] = hm_vram_pixel(x, y + (u32)origin_y);
    }
#ifdef HM_SCALE_REFERENCE
    else {
        setup();
        hm_software_surface surface = {.pixels = reference,
                                       .width = width,
                                       .height = height,
                                       .texture_linear = texture_linear};
        previous = hm_replay_bind(hm_software_sink(&surface));
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
        if (n >= 1024 || fixture(device, renderer, builder, commands, n, 32, 32, 0)) {
            fprintf(stderr, "Random batch %d, filter %d\n", batch, texture_linear);
            return 1;
        }
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
            hm_software_surface surface = {
                .pixels = reference, .width = sizes[size][0], .height = sizes[size][1]};
            hm_replay_sink previous = hm_replay_bind(hm_software_sink(&surface));
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
#ifdef HMBTN_ENH_WIDESCREEN
static int wide_background;
static void hm_probe_wide_setup(int origin) {
    setup();
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, (s16)origin, 320, 240);
    draw.dtd = 0;
    draw.dfe = 1;
    draw.isbg = (u8)wide_background;
    draw.r0 = draw.g0 = draw.b0 = 255;
    PutDrawEnv(&draw);
}
static void hm_probe_wide_draw(void) {
    u32 world[] = {0xe1000000 | 282, 0x65000000,   xy(-40, 20), 0,          xy(28, 28),
                   0x65000000,       xy(330, 20),  0,           xy(28, 28), 0x200000ff,
                   xy(-50, 100),     xy(360, 100), xy(160, 200)};
    u32 ui[] = {0x6000ff00, xy(-40, 60), xy(28, 28), 0x6000ff00, xy(150, 10), xy(20, 20)};
    hm_replay_group(1);
    hm_gpu_execute_words(world, sizeof world / sizeof *world);
    hm_replay_group(0);
    hm_gpu_execute_words(ui, sizeof ui / sizeof *ui);
    u32 fade[] = {0xe1000040, 0x2a101010, 0, 320, 240u << 16, 320u | (240u << 16)};
    hm_gpu_execute_words(fade, sizeof fade / sizeof *fade);
    u32 rucksack[] = {0xe1000040, 0x62080808, 0, xy(320, 240)};
    hm_gpu_execute_words(rucksack, sizeof rucksack / sizeof *rucksack);
    u32 origin = (u32)hm_gpu_environment_state()->draw.ofs[1];
    u32 clip[] = {0xe3000000u | 16u | ((origin + 160) << 10),
                  0xe4000000u | 319u | ((origin + 255) << 10), 0xe100060e};
    hm_gpu_execute_words(clip, 3);
    for (int i = 0; i < 40; ++i) {
        u32 glyph[] = {0x64808080, xy(20 + (i % 20) * 8, 170 + (i / 20) * 12), 0x7ab80000,
                       xy(8, 12)};
        hm_gpu_execute_words(glyph, 4);
    }
    u32 restore[] = {0xe3000000u | (origin << 10), 0xe4000000u | 319u | ((origin + 239) << 10)};
    hm_gpu_execute_words(restore, 2);
}
static int hm_probe_wide_benchmark(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                   hm_gpu_scene *builder) {
    hm_widescreen_configure(1, 1920, 1080);
    hm_widescreen_scene(1);
    hm_probe_wide_setup(0);
    saved_vram = *hm_vram_state();
    hm_vram_context *original = hm_vram_bind(&work_vram);
    RECT area = {0, 0, 320, 240};
    for (int filter = 0; filter <
#ifdef HM_FILTER_REFERENCE
                         2
#else
                         1
#endif
         ;
         ++filter) {
        const int passes = 25;
        hm_software_surface surface = {
            .pixels = reference, .width = 1920, .height = 1080, .texture_linear = filter};
        u64 start = SDL_GetTicksNS();
        for (int i = 0; i < passes; ++i) {
            work_vram = saved_vram;
            hm_replay_sink previous = hm_replay_bind(hm_software_sink(&surface));
            hm_replay_begin(&area);
            hm_probe_wide_draw();
            hm_replay_bind(previous);
        }
        printf("Wide CPU 1920x1080 filter=%d: %.3f ms/render\n", filter,
               (double)(SDL_GetTicksNS() - start) / 1e6 / passes);
        surface.width = 428;
        surface.height = 240;
        start = SDL_GetTicksNS();
        for (int i = 0; i < passes; ++i) {
            work_vram = saved_vram;
            hm_replay_sink previous = hm_replay_bind(hm_software_sink(&surface));
            hm_replay_begin(&area);
            hm_probe_wide_draw();
            hm_replay_bind(previous);
        }
        printf("Auto native fallback CPU 428x240 filter=%d: %.3f ms/render\n", filter,
               (double)(SDL_GetTicksNS() - start) / 1e6 / passes);
        hm_gpu_scene_texture_filter(builder, filter);
        start = SDL_GetTicksNS();
        for (int i = 0; i < passes; ++i) {
            work_vram = saved_vram;
            hm_replay_sink previous = hm_gpu_scene_begin(builder, 1920, 1080);
            hm_replay_begin(&area);
            hm_probe_wide_draw();
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
        printf("Wide GPU capture/upload/compute/fence 1920x1080 filter=%d: %.3f ms/render\n",
               filter, (double)(SDL_GetTicksNS() - start) / 1e6 / passes);
    }
    hm_vram_bind(original);
    hm_widescreen_configure(0, 1920, 1080);
    hm_widescreen_scene(0);
    return 0;
}
static int hm_probe_widescreen(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                               hm_gpu_scene *builder) {
    hm_widescreen_configure(1, 1920, 1080);
    hm_widescreen_scene(1);
    const u32 sizes[][2] = {{428, 240}, {1280, 720}};
    for (wide_background = 0; wide_background < 2; ++wide_background)
        for (int origin = 0; origin <= 240; origin += 240)
            for (int filter = 0; filter <
#ifdef HM_FILTER_REFERENCE
                                 2
#else
                                 1
#endif
                 ;
                 ++filter)
                for (int n = 0; n < 2; ++n) {
                    u32 width = sizes[n][0], height = sizes[n][1];
                    hm_probe_wide_setup(origin);
                    hm_gpu_scene_texture_filter(builder, filter);
                    hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
                    RECT area = {0, (s16)origin, 320, 240};
                    hm_replay_begin(&area);
                    hm_probe_wide_draw();
                    hm_gpu_scene_data data;
                    /* Production replay restores the live context before end;
                     * its next-frame clear must not replace this scene's colour. */
                    hm_gpu_environment next_frame = *hm_gpu_environment_state();
                    next_frame.draw.isbg = (u8)!wide_background;
                    hm_gpu_environment *captured = hm_gpu_environment_bind(&next_frame);
                    CHECK(hm_gpu_scene_end(builder, previous, &data));
                    hm_gpu_environment_bind(captured);
                    hm_probe_wide_setup(origin);
                    hm_software_surface surface = {.pixels = reference,
                                                   .width = width,
                                                   .height = height,
                                                   .texture_linear = filter};
                    previous = hm_replay_bind(hm_software_sink(&surface));
                    hm_replay_begin(&area);
                    hm_probe_wide_draw();
                    hm_replay_bind(previous);
                    CHECK(!readback(device, renderer, &data, actual));
                    for (u32 i = 0; i < width * height; ++i)
                        for (u32 c = 0; c < 3; ++c) {
                            int v = (reference[i] >> (c * 5)) & 31;
                            int expected = (v << 3) | (v >> 2);
                            if (abs(actual[i * 4 + c] - expected) > (n ? 9 : 0)) {
                                fprintf(stderr,
                                        "Widescreen mismatch origin=%d filter=%d size=%u pixel=%u "
                                        "channel=%u expected=%d actual=%u\n",
                                        origin, filter, width, i, c, expected, actual[i * 4 + c]);
                                return 1;
                            }
                        }
                }
    wide_background = 0;
    hm_widescreen_configure(0, 1920, 1080);
    hm_widescreen_scene(0);
    puts("Expanded world/UI offscreen GPU comparisons passed");
    return 0;
}
#endif
#ifdef HM_INTERPOLATION_REFERENCE
static int hm_probe_scaled_rows(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                hm_gpu_scene *builder) {
    const u32 sizes[][2] = {{65, 97}, {1128, 634}};
    for (int filter = 0; filter <= 2; filter += 2)
        for (unsigned samples = 1; samples <= 4; samples *= 2)
            for (int shift = 0; shift < 4; ++shift)
                for (u32 size = 0; size < SDL_arraysize(sizes); ++size)
                    for (int polygon = 0; polygon < 2; ++polygon) {
                        setup();
                        for (u32 y = 0; y < 32; ++y)
                            for (u32 x = 0; x < 32; ++x)
                                hm_vram_draw_pixel(x, y, 0);
                        for (u32 y = 0; y < 16; ++y)
                            for (u32 x = 0; x < 16; ++x)
                                hm_vram_draw_pixel(640 + x, 256 + y, 0x7fff);
                        const u32 rows[] = {0xe1000400 | 282, 0x65808080, xy(2, 12 + shift), 0,
                                            xy(8, 4), 0x65808080, xy(2, 16 + shift), 0, xy(8, 4)};
                        const u32 quads[] = {
                            0xe1000400 | 282,
                            0x2d808080, xy(2, 12 + shift), 0, xy(10, 12 + shift), (282u << 16) | 8,
                            xy(2, 16 + shift), 4u << 8, xy(10, 16 + shift), (4u << 8) | 8,
                            0x2d808080, xy(2, 16 + shift), 0, xy(10, 16 + shift), (282u << 16) | 8,
                            xy(2, 20 + shift), 4u << 8, xy(10, 20 + shift), (4u << 8) | 8};
                        u32 width = sizes[size][0], height = sizes[size][1];
                        hm_gpu_scene_texture_filter(builder, filter);
                        hm_gpu_scene_multisamples(builder, samples);
                        hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
                        RECT area = {0, 0, 32, 32};
                        hm_replay_begin(&area);
                        hm_gpu_execute_words(polygon ? quads : rows,
                                             polygon ? sizeof quads / sizeof *quads
                                                     : sizeof rows / sizeof *rows);
                        hm_gpu_scene_data data;
                        CHECK(hm_gpu_scene_end(builder, previous, &data));
                        CHECK(!readback(device, renderer, &data, actual));
                        for (u32 y = 0; y < height; ++y)
                            for (u32 x = 0; x < width; ++x) {
                                double lx = (x + 0.5) * 32 / width;
                                double ly = (y + 0.5) * 32 / height;
                                if (lx <= 3 || lx >= 9 || ly <= 13 + shift || ly >= 19 + shift)
                                    continue;
                                const u8 *pixel = actual + (y * width + x) * 4;
                                if ((u32)pixel[0] + pixel[1] + pixel[2] <= 700)
                                    fprintf(stderr, "gap filter=%d samples=%u shift=%d size=%ux%u "
                                            "polygon=%d pixel=%u,%u logical=%.3f,%.3f rgb=%u,%u,%u\n",
                                            filter, samples, shift, width, height, polygon, x, y, lx, ly,
                                            pixel[0], pixel[1], pixel[2]);
                                CHECK((u32)pixel[0] + pixel[1] + pixel[2] > 700);
                            }
                    }
    hm_gpu_scene_texture_filter(builder, 0);
    hm_gpu_scene_multisamples(builder, 1);
    puts("Scaled moving sprite and quad rows passed with nearest/xBR and 1x/2x/4x MSAA.");
    return 0;
}
static int hm_probe_tile_seams(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                              hm_gpu_scene *builder) {
    u32 packets[2][6];
    hm_vram_reset();
    ResetGraph(0);
    CHECK(hm_interpolation_enable(1));
    hm_render_frame live = {0};
    for (int slot = 0; slot < 2; ++slot) {
        DRAWENV draw;
        SetDefDrawEnv(&draw, 0, 0, 32, 32);
        draw.dtd = 0;
        draw.dfe = 1;
        PutDrawEnv(&draw);
        live = (hm_render_frame){.key = 1, .tick = 2 + slot * 2, .eligible = 1,
            .arena = {packets[slot], NULL}, .arena_bytes = {sizeof(packets[slot]), 0}};
        hm_packet_identity_reset();
        for (int i = 0; i < 2; ++i) {
            u32 *p = packets[slot] + i * 3;
            p[0] = slot && i ? 0x6000ff00 : 0x600000ff;
            p[1] = xy(2, 2 + i * 4 + slot * 4);
            p[2] = xy(8, 4);
            CHECK(hm_packet_identity_span(p, p + 3, (u64)i + 1));
        }
        hm_render_begin(&live);
        hm_render_group(1);
        for (int i = 0; i < 2; ++i) {
            hm_gpu_execute_words(packets[slot] + i * 3, 3);
            hm_render_packet(packets[slot] + i * 3, 3);
        }
        hm_render_end();
    }
    const u32 sizes[][2] = {{32, 32}, {65, 97}, {1128, 634}};
    for (u32 size = 0; size < SDL_arraysize(sizes); ++size)
        for (u32 samples = 1; samples <= 4; samples *= 2) {
            u32 width = sizes[size][0], height = sizes[size][1];
            hm_gpu_scene_multisamples(builder, samples);
            hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
            CHECK(hm_interpolation_replay(83333334, &live));
            hm_gpu_scene_data data;
            CHECK(hm_gpu_scene_end(builder, previous, &data));
            CHECK(!readback(device, renderer, &data, actual));
            for (u32 y = 0; y < height; ++y)
                for (u32 x = 0; x < width; ++x) {
                    /* Exclude outer-edge AA, but check every pixel spanning
                     * the shared horizontal edge of the two rows. */
                    double lx = (x + 0.5) * 32 / width, ly = (y + 0.5) * 32 / height;
                    if (lx <= 3 || lx >= 9 || ly <= 7 || ly >= 13)
                        continue;
                    const u8 *p = actual + (y * width + x) * 4;
                    CHECK((u32)p[0] + p[1] + p[2] > 200);
                }
        }
    CHECK(hm_interpolation_enable(0));
    hm_packet_identity_reset();
    puts("Tile row seams: native and enlarged GPU readback passed at 1x/2x/4x MSAA.");
    return 0;
}
#endif
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
    if (argc > 2 && !SDL_strcmp(argv[2], "--display-area")) {
        const u32 display[] = {0xe1000000, 0x600000ff, 0, xy(30, 30),
                               0xe1000400, 0x6000ff00, xy(4, 4), xy(20, 20)};
        CHECK(builder);
        for (origin_y = 0; origin_y <= 240; origin_y += 240) {
            display_test = 1;
            CHECK(!fixture(device, renderer, builder, display,
                           sizeof(display) / sizeof(*display), 32, 32, 0));
        }
        hm_gpu_scene_destroy(builder);
        hm_gpu_renderer_destroy(renderer);
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        puts("Displayed-area write protection matches CPU reference.");
        return 0;
    }
    if (argc > 2 && !SDL_strcmp(argv[2], "--gradient")) {
        CHECK(builder && !hm_probe_gradient(device, renderer, builder) &&
              !hm_probe_texture_precision(device, renderer, builder));
        hm_gpu_scene_destroy(builder);
        hm_gpu_renderer_destroy(renderer);
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        return 0;
    }
#ifdef HM_INTERPOLATION_REFERENCE
    if (argc > 2 && !SDL_strcmp(argv[2], "--tile-seams")) {
        CHECK(builder && !hm_probe_tile_seams(device, renderer, builder) &&
              !hm_probe_scaled_rows(device, renderer, builder));
        hm_gpu_scene_destroy(builder);
        hm_gpu_renderer_destroy(renderer);
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        return 0;
    }
#endif
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
    for (int filtering = 0; filtering <
#ifdef HM_FILTER_REFERENCE
                            2
#else
                            1
#endif
         ;
         ++filtering) {
        texture_linear = filtering;
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
#ifdef HM_FILTER_REFERENCE
                if (filtering) {
                    CHECK(!fixture(device, renderer, builder, textures,
                                   sizeof(textures) / sizeof(*textures), 64, 64, 9));
                    CHECK(!fixture(device, renderer, builder, textures,
                                   sizeof(textures) / sizeof(*textures), 1440, 1080, 9));
                }
#endif
            }
    }
    texture_linear = 0;
    u32 dialog_clip[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, xy(2, 2), xy(4, 4)};
    CHECK(!fixture(device, renderer, builder, dialog_clip, 4, 32, 32, 0));
    CHECK(!fixture(device, renderer, builder, dialog_clip, 4, 128, 96, 9));
    u32 safe_page[] = {0xe1000600, 0x65808080, xy(2, 2), (0x7ab8u << 16) | 0xf000, xy(4, 4)};
    CHECK(!fixture(device, renderer, builder, safe_page, 5, 32, 32, 0));
    CHECK(!hm_probe_random_cases(device, renderer, builder));
#ifdef HM_FILTER_REFERENCE
    texture_linear = 1;
    CHECK(!hm_probe_random_cases(device, renderer, builder));
    texture_linear = 0;
#endif
    CHECK(!hm_probe_motion(device, renderer, builder));
#ifdef HM_SCALE_REFERENCE
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 64, 64, 0));
    CHECK(!fixture(device, renderer, builder, shapes, sizeof(shapes) / sizeof(*shapes), 1440, 1080,
                   9));
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    CHECK(!hm_probe_widescreen(device, renderer, builder));
#endif
    CHECK(!hm_probe_presentation(device, renderer, builder));
    if (argc > 2 && !SDL_strcmp(argv[2], "--benchmark")) {
        CHECK(!hm_probe_benchmark(device, renderer, builder));
#ifdef HMBTN_ENH_WIDESCREEN
        CHECK(!hm_probe_wide_benchmark(device, renderer, builder));
#endif
    }
    hm_gpu_scene_destroy(builder);
    SDL_WaitForGPUIdle(device);
    hm_gpu_renderer_destroy(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    puts("Synthetic native GPU readback comparisons passed; no game/window started.");
    return 0;
}
