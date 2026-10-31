#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
/* Offscreen, synthetic GPU readback. Never opens a window or starts the game. */
#include "platform/gpu_present.h"
#include "platform/gpu_renderer.h"
#include "gfx/frame_events.h"
#include "gfx/frame_replay.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_links.h"
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
static int origin_y, display_test, texture_filter;
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
    hm_gpu_scene_texture_filter(builder, texture_filter);
    hm_replay_sink previous = hm_gpu_scene_begin(builder, width, height);
    RECT area = {0, (s16)origin_y, 32, 32};
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    if (hm_gpu_scene_needs_materialization(builder)) {
        hm_gpu_scene_materialize_vram(builder, 1);
        setup();
        (void)hm_gpu_scene_begin(builder, width, height);
        hm_replay_begin(&area);
        hm_gpu_execute_words(words, count);
        hm_gpu_scene_materialize_vram(builder, 0);
    }
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    setup();
    hm_gpu_execute_words(words, count);
    if (width == 32 && height == 32 && !texture_filter) {
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
                                       .texture_filter = texture_filter};
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
static int hm_probe_reference_has_drawn_pixel(u32 width, u32 height) {
    for (u32 i = 0; i < width * height; ++i)
        if (reference[i] != 0x4210)
            return 1;
    return 0;
}
static int hm_probe_primitive_opcode_sweep(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                           hm_gpu_scene *builder) {
    static const u32 polygon_xy[] = {0x00020002, 0x0003001a, 0x001a0004, 0x001b001b};
    static const u32 line_xy[] = {0x00020002, 0x0003001a, 0x001a001b};
    const u32 clut = (252u << 6) | 56u;
    for (u32 opcode = 0x20; opcode <= 0x3f; ++opcode) {
        int textured = (opcode & 4) != 0, semi = (opcode & 2) != 0;
        u32 depths = textured ? 3 : 1, modes = textured || semi ? 4 : 1;
        for (u32 depth = 0; depth < depths; ++depth)
            for (u32 mode = 0; mode < modes; ++mode) {
                u32 words[16] = {0}, count = 0;
                u32 vertices = (opcode & 8) ? 4 : 3;
                u32 page = 26u | (depth << 7) | (mode << 5);
                words[count++] = (opcode << 24) | 0x00e08040;
                for (u32 vertex = 0; vertex < vertices; ++vertex) {
                    if (vertex && (opcode & 16))
                        words[count++] = 0x0020c0f0 - vertex * 0x00001020;
                    words[count++] = polygon_xy[vertex];
                    if (textured) {
                        u32 uv_page = vertex == 0 ? clut : vertex == 1 ? page : 0;
                        words[count++] = (uv_page << 16) | ((vertex * 7) << 8) | (vertex * 5);
                    }
                }
                if (fixture(device, renderer, builder, words, count, 32, 32, 0)) {
                    fprintf(stderr, "Primitive opcode 0x%02x depth %u mode %u GPU fixture failed\n",
                            opcode, depth, mode);
                    return 1;
                }
                if (!hm_probe_reference_has_drawn_pixel(32, 32)) {
                    fprintf(stderr, "Primitive opcode 0x%02x depth %u mode %u drew no CPU pixels\n",
                            opcode, depth, mode);
                    return 1;
                }
            }
    }
    for (u32 opcode = 0x40; opcode <= 0x5f; ++opcode) {
        int semi = (opcode & 2) != 0;
        u32 modes = semi ? 4 : 1;
        for (u32 mode = 0; mode < modes; ++mode) {
            u32 words[12] = {0}, count = 0;
            words[count++] = 0xe1000000 | (mode << 5);
            words[count++] = (opcode << 24) | 0x00e08040;
            words[count++] = line_xy[0];
            if (opcode & 8) {
                if (opcode & 16)
                    words[count++] = 0x0020d0f0;
                words[count++] = line_xy[1];
                if (opcode & 16)
                    words[count++] = 0x00f020c0;
                words[count++] = line_xy[2];
                words[count++] = 0x50005000;
            } else {
                if (opcode & 16)
                    words[count++] = 0x0020d0f0;
                words[count++] = line_xy[2];
            }
            if (fixture(device, renderer, builder, words, count, 32, 32, 0)) {
                fprintf(stderr, "Primitive opcode 0x%02x mode %u GPU fixture failed\n", opcode,
                        mode);
                return 1;
            }
            if (!hm_probe_reference_has_drawn_pixel(32, 32)) {
                fprintf(stderr, "Primitive opcode 0x%02x mode %u drew no CPU pixels\n", opcode,
                        mode);
                return 1;
            }
        }
    }
    for (u32 opcode = 0x60; opcode <= 0x7f; ++opcode) {
        int textured = (opcode & 4) != 0, semi = (opcode & 2) != 0;
        u32 depths = textured ? 3 : 1, modes = textured || semi ? 4 : 1;
        for (u32 depth = 0; depth < depths; ++depth)
            for (u32 mode = 0; mode < modes; ++mode) {
                u32 words[8] = {0}, count = 0;
                u32 size = (opcode >> 3) & 3;
                u32 page = 26u | (depth << 7) | (mode << 5);
                words[count++] = 0xe1000000 | page;
                words[count++] = (opcode << 24) | 0x00e08040;
                words[count++] = 0x00020002;
                if (textured)
                    words[count++] = (clut << 16) | 0x00000104;
                if (size == 0)
                    words[count++] = 7 | (6 << 16);
                if (fixture(device, renderer, builder, words, count, 32, 32, 0)) {
                    fprintf(stderr, "Primitive opcode 0x%02x depth %u mode %u GPU fixture failed\n",
                            opcode, depth, mode);
                    return 1;
                }
                if (!hm_probe_reference_has_drawn_pixel(32, 32)) {
                    fprintf(stderr, "Primitive opcode 0x%02x depth %u mode %u drew no CPU pixels\n",
                            opcode, depth, mode);
                    return 1;
                }
            }
    }
    puts("Primitive opcode sweep: every polygon, line and rectangle encoding matched CPU readback.");
    return 0;
}
static int hm_probe_dynamic_scene(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                  hm_gpu_scene *builder) {
    const u32 primitive_count = HM_SCENE_INITIAL_PRIMITIVES + 1;
    const u32 packet[] = {0x600000ff, 0x00020002, 0x00040004};
    u32 *packets = malloc((size_t)primitive_count * sizeof(packet));
    CHECK(packets);
    for (u32 i = 0; i < primitive_count; ++i)
        SDL_memcpy(packets + (size_t)i * SDL_arraysize(packet), packet, sizeof(packet));
    setup();
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
    RECT area = {0, (s16)origin_y, 32, 32};
    hm_replay_begin(&area);
    for (u32 i = 0; i < primitive_count; ++i)
        hm_gpu_execute_words(packets + (size_t)i * SDL_arraysize(packet),
                             SDL_arraysize(packet));
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    u32 max_bin = 0;
    for (u32 bin = 0; bin < scene.words[7]; ++bin) {
        u32 list = scene.words[scene.words[9] + bin];
        if (scene.words[list] > max_bin)
            max_bin = scene.words[list];
    }
    CHECK(scene.words[10] == primitive_count && max_bin > HM_SCENE_BIN_WORDS &&
          scene.bytes > 65536);
    setup();
    for (u32 i = 0; i < primitive_count; ++i)
        hm_gpu_execute_words(packets + (size_t)i * SDL_arraysize(packet),
                             SDL_arraysize(packet));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            reference[y * 32 + x] = hm_vram_pixel(x, y + (u32)origin_y);
    CHECK(!readback(device, renderer, &scene, actual));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 expected = reference[y * 32 + x];
            const u8 *pixel = actual + (y * 32 + x) * 4;
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (expected >> (channel * 5)) & 31;
                CHECK(pixel[channel] == (u8)((value << 3) | (value >> 2)));
            }
        }
    free(packets);
    puts("Dynamic GPU scene buffers and high-density tile bins match CPU reference.");
    return 0;
}
static int hm_probe_texture_feedback(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                     hm_gpu_scene *builder) {
    const u32 commands[] = {0x600000ff, 0x00020002, 0x00010001,
                            0x6000ff00, 0x00140014, 0x00040004, 0xe1000100,
                            0x65000001, 0x00040004, 0x00020002, 0x00010001};
    setup();
    RECT area = {0, 0, 32, 32};
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(commands, sizeof commands / sizeof *commands);
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    CHECK(hm_gpu_scene_needs_materialization(builder));
    u64 materialization_tiles[HM_VRAM_DIRTY_ROWS];
    CHECK(hm_gpu_scene_materialization_mask(builder, materialization_tiles) > 0);
    hm_vram_snapshot(&work_vram);
    hm_vram_context *native = hm_vram_bind(&work_vram);
    hm_vram_write_filter_begin(materialization_tiles);
    hm_gpu_scene_materialize_vram(builder, 1);
    previous = hm_gpu_scene_begin(builder, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(commands, sizeof commands / sizeof *commands);
    hm_vram_write_filter_end();
    hm_vram_bind(native);
    hm_gpu_scene_materialize_vram(builder, 0);
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    CHECK(scene.vram_version_count == 1);
    CHECK(scene.words[scene.words[8] + HM_SCENE_STRIDE * 2 + 40] == 1);
    CHECK(!readback(device, renderer, &scene, actual));

    setup();
    hm_gpu_execute_words(commands, sizeof commands / sizeof *commands);
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 color = hm_vram_pixel(x, y);
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 32 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr, "Feedback mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }
    puts("Ordered VRAM texture feedback matches the CPU raster reference.");
    return 0;
}
static void hm_probe_copy_seed(void) {
    setup();
    const u16 colors[4][4] = {{0x001f, 0x03e0, 0x7c00, 0x4210},
                              {0x1234, 0x2345, 0x3456, 0x4567},
                              {0x7fff, 0x03ff, 0x7c1f, 0x2108},
                              {0x0010, 0x0200, 0x4000, 0x6318}};
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x)
            hm_vram_draw_pixel((1022 + x) & 1023, (510 + y) & 511, colors[y][x]);
    hm_vram_draw_pixel(12, 8, 0x8001);
    hm_vram_draw_pixel(13, 8, 0x8002);
    hm_vram_draw_pixel(16, 8, 0x8003);
    hm_vram_draw_pixel(17, 8, 0x0004);
}
static int hm_probe_vram_transfers(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                   hm_gpu_scene *builder) {
    const u32 commands[] = {
        0x9f000000, xy(1022, 510), xy(8, 8), xy(4, 4),
        0xe6000001, 0x8a000000, xy(8, 8), xy(10, 8), xy(4, 4),
        0xe6000002, 0x81000000, xy(8, 8), xy(12, 8), xy(4, 4),
        0xbf000000, xy(16, 8), xy(2, 1), 0x45671234,
        0xe6000001, 0xa5000000, xy(18, 8), xy(2, 1), 0x45671234};
    hm_probe_copy_seed();
    RECT area = {0, 0, 32, 32};
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
    hm_replay_begin(&area);
    hm_gpu_execute_words(commands, SDL_arraysize(commands));
    if (hm_gpu_scene_needs_materialization(builder)) {
        hm_gpu_scene_materialize_vram(builder, 1);
        hm_probe_copy_seed();
        (void)hm_gpu_scene_begin(builder, 32, 32);
        hm_replay_begin(&area);
        hm_gpu_execute_words(commands, SDL_arraysize(commands));
        hm_gpu_scene_materialize_vram(builder, 0);
    }
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    CHECK(scene.vram_version_count >= 2);

    hm_probe_copy_seed();
    hm_gpu_execute_words(commands, SDL_arraysize(commands));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            reference[y * 32 + x] = hm_vram_pixel(x, y);
    CHECK(!readback(device, renderer, &scene, actual));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 color = reference[y * 32 + x];
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 32 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr, "VRAM transfer mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }

    /* A destination crossing both VRAM edges splits into four wrapped pieces;
     * compare the visible bottom-right quadrant against the CPU state. */
    const u32 destination_wrap[] = {0x80000000, xy(1018, 506), xy(1022, 510), xy(4, 4)};
    setup();
    const u16 edge_colors[4][4] = {{0x001f, 0x03e0, 0x7c00, 0x4210},
                                   {0x1234, 0x2345, 0x3456, 0x4567},
                                   {0x7fff, 0x03ff, 0x7c1f, 0x2108},
                                   {0x0010, 0x0200, 0x4000, 0x6318}};
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x)
            hm_vram_draw_pixel(1018 + x, 506 + y, edge_colors[y][x]);
    RECT edge_area = {992, 480, 32, 32};
    DRAWENV edge_draw;
    SetDefDrawEnv(&edge_draw, edge_area.x, edge_area.y, edge_area.w, edge_area.h);
    edge_draw.dtd = 0;
    edge_draw.dfe = 1;
    PutDrawEnv(&edge_draw);
    previous = hm_gpu_scene_begin(builder, 32, 32);
    hm_replay_begin(&edge_area);
    hm_gpu_execute_words(destination_wrap, SDL_arraysize(destination_wrap));
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    setup();
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x)
            hm_vram_draw_pixel(1018 + x, 506 + y, edge_colors[y][x]);
    hm_gpu_execute_words(destination_wrap, SDL_arraysize(destination_wrap));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            reference[y * 32 + x] = hm_vram_pixel(992 + x, 480 + y);
    CHECK(!readback(device, renderer, &scene, actual));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 color = reference[y * 32 + x];
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 32 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr, "Wrapped destination mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }
    puts("GPU VRAM copy wrap/overlap and masked copy/load transfers match the CPU reference.");
    return 0;
}
static int hm_probe_direct_transfer_capture(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                           hm_gpu_scene *builder) {
    setup();
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, 32, 32);
    PutDispEnv(&display);
    const u32 pixel[] = {0x00007c1f};
    const u32 tile[] = {0x600000ff, xy(2, 2), xy(4, 4)};
    RECT load = {20, 20, 1, 1}, move = {20, 20, 1, 1}, clear = {0, 0, 16, 16};
    hm_frame_replay *replay = hm_frame_replay_acquire();
    CHECK(replay);
    hm_render_frame live = {.tick = 2};
    hm_render_begin(&live);
    CHECK(LoadImage(&load, pixel) == 0);
    CHECK(MoveImage(&move, 30, 30) == 0);
    CHECK(ClearImage(&clear, 0, 255, 0) == 0);
    hm_gpu_execute_words(tile, SDL_arraysize(tile));
    hm_render_packet(tile, SDL_arraysize(tile));
    hm_render_end();
    CHECK(hm_frame_replay_serial(replay, &live));
    hm_replay_sink previous = hm_gpu_scene_begin(builder, 32, 32);
    CHECK(hm_frame_replay_draw(replay, &live));
    if (hm_gpu_scene_needs_materialization(builder)) {
        hm_gpu_scene_materialize_vram(builder, 1);
        (void)hm_gpu_scene_begin(builder, 32, 32);
        CHECK(hm_frame_replay_draw(replay, &live));
        hm_gpu_scene_materialize_vram(builder, 0);
    }
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    CHECK(!readback(device, renderer, &scene, actual));

    setup();
    SetDefDispEnv(&display, 0, 0, 32, 32);
    PutDispEnv(&display);
    CHECK(LoadImage(&load, pixel) == 0);
    CHECK(MoveImage(&move, 30, 30) == 0);
    CHECK(ClearImage(&clear, 0, 255, 0) == 0);
    hm_gpu_execute_words(tile, SDL_arraysize(tile));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            reference[y * 32 + x] = hm_vram_pixel(x, y);
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 color = reference[y * 32 + x];
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 32 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr,
                            "Direct transfer capture mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }

    /* A native DrawOTag call without the main-loop wrapper auto-captures as a
     * bypass frame, covering the loading-screen call site. */
    setup();
    SetDefDispEnv(&display, 0, 0, 32, 32);
    PutDispEnv(&display);
    const u32 ot[] = {3u << 24 | HM_GPU_LINK_END, tile[0], tile[1], tile[2]};
    DrawOTag((u32 *)ot);
    live = (hm_render_frame){.tick = 2};
    CHECK(hm_frame_replay_serial(replay, &live));
    previous = hm_gpu_scene_begin(builder, 32, 32);
    CHECK(hm_frame_replay_draw(replay, &live));
    CHECK(hm_gpu_scene_end(builder, previous, &scene));
    hm_frame_replay_release(replay);
    CHECK(!readback(device, renderer, &scene, actual));
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            reference[y * 32 + x] = hm_vram_pixel(x, y);
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x) {
            u16 color = reference[y * 32 + x];
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 32 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr,
                            "Direct DrawOTag capture mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }
    puts("Captured direct LoadImage, MoveImage and ClearImage match CPU readback.");
    puts("Unwrapped DrawOTag capture matches CPU readback.");
    return 0;
}
static int hm_probe_vram_scanout(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                 hm_gpu_scene *builder) {
    origin_y = 240;
    display_test = 1;
    setup();
    RECT area = {0, 240, 32, 32};
    for (u32 y = 0; y < 32; ++y)
        for (u32 x = 0; x < 32; ++x)
            hm_vram_draw_pixel(x, y + 240, (u16)((x * 317 + y * 911) & 0x7fff));
    hm_gpu_scene_data scene;
    CHECK(hm_gpu_scene_scanout(builder, &area, 64, 64, &scene));
    CHECK(scene.words[10] == 0);
    CHECK(!readback(device, renderer, &scene, actual));
    for (u32 y = 0; y < 64; ++y)
        for (u32 x = 0; x < 64; ++x) {
            u16 color = hm_vram_pixel(x / 2, 240 + y / 2);
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 value = (color >> (channel * 5)) & 31;
                u8 expected = (u8)((value << 3) | (value >> 2));
                u8 got = actual[(y * 64 + x) * 4 + channel];
                if (got != expected) {
                    fprintf(stderr,
                            "GPU VRAM scanout mismatch %u,%u channel %u: GPU %u CPU %u\n",
                            x, y, channel, got, expected);
                    return 1;
                }
            }
        }
    puts("GPU VRAM scanout at 2x matches the visible 15-bit VRAM page.");
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
            fprintf(stderr, "Random batch %d, filter %d\n", batch, texture_filter);
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
static int hm_probe_farm_benchmark(SDL_GPUDevice *device, hm_gpu_renderer *renderer,
                                   hm_gpu_scene *builder) {
    enum { PRIMITIVES = 8192, ITERATIONS = 8 };
    (void)builder;
    hm_gpu_scene *farm_builder = hm_gpu_scene_create();
    CHECK(farm_builder);
    u32 *commands = malloc((size_t)PRIMITIVES * 3 * sizeof(*commands));
    CHECK(commands);
    for (u32 i = 0; i < PRIMITIVES; ++i) {
        u32 x = (i % 40) * 8;
        u32 y = ((i / 40) % 30) * 8;
        commands[i * 3] = 0x60000000u | ((i * 97u) & 0xffffffu);
        commands[i * 3 + 1] = x | (y << 16);
        commands[i * 3 + 2] = 8u | (8u << 16);
    }

    setup();
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    draw.dtd = 0;
    draw.dfe = 1;
    PutDrawEnv(&draw);
    DISPENV display;
    SetDefDispEnv(&display, 0, 0, 320, 240);
    PutDispEnv(&display);
    SetDispMask(1);
    RECT area = {0, 0, 320, 240};
    hm_vram_snapshot(&saved_vram);
    hm_vram_context *old = hm_vram_bind(&work_vram);
    const u32 sizes[][2] = {{320, 240}, {960, 720}};
    for (size_t size = 0; size < SDL_arraysize(sizes); ++size) {
        hm_gpu_scene_data scene = {0};
        u64 start = 0;
        for (int frame = -1; frame < ITERATIONS; ++frame) {
            work_vram = saved_vram;
            hm_replay_sink previous =
                hm_gpu_scene_begin(farm_builder, sizes[size][0], sizes[size][1]);
            hm_replay_begin(&area);
            hm_gpu_execute_words(commands, PRIMITIVES * 3);
            CHECK(hm_gpu_scene_end(farm_builder, previous, &scene));
            SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
            CHECK(command_buffer);
            CHECK(hm_gpu_renderer_encode(renderer, command_buffer, &scene));
            SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
            CHECK(fence);
            CHECK(SDL_WaitForGPUFences(device, true, &fence, 1));
            SDL_ReleaseGPUFence(device, fence);
            if (frame == -1)
                start = SDL_GetTicksNS();
        }
        u32 max_bin = 0, nonempty_bins = 0;
        u32 tile_size = 8;
        u32 tiles_x = (scene.words[4] + tile_size - 1) / tile_size;
        u32 tile_count = tiles_x * ((scene.words[5] + tile_size - 1) / tile_size);
        int packed_bins = scene.words[7] == tile_count;
        if (!packed_bins) {
            tile_size = 16;
            tiles_x = (scene.words[4] + tile_size - 1) / tile_size;
            tile_count = tiles_x * ((scene.words[5] + tile_size - 1) / tile_size);
        }
        for (u32 bin = 0; bin < tile_count; ++bin) {
            u32 count = packed_bins
                            ? scene.words[scene.words[scene.words[9] + bin]]
                            : scene.words[scene.words[9] + bin * scene.words[7]];
            if (count > max_bin)
                max_bin = count;
            nonempty_bins += count != 0;
        }
        printf("Farm-like GPU %ux%u: %u primitives, %zu-byte scene, tiles=%u "
               "max-bin=%u occupied-bins=%u, %.3f ms/render\n",
               sizes[size][0], sizes[size][1], scene.words[10], scene.bytes, tile_count,
               max_bin, nonempty_bins,
               (double)(SDL_GetTicksNS() - start) / 1e6 / ITERATIONS);
    }
    hm_vram_bind(old);
    hm_gpu_scene_destroy(farm_builder);
    free(commands);
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
            .pixels = reference, .width = 1920, .height = 1080, .texture_filter = filter};
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
                                                   .texture_filter = filter};
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
    if (argc > 2 && !SDL_strcmp(argv[2], "--vram-scanout")) {
        CHECK(builder && !hm_probe_vram_scanout(device, renderer, builder));
        hm_gpu_scene_destroy(builder);
        hm_gpu_renderer_destroy(renderer);
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        return 0;
    }
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
    if (argc > 2 && !SDL_strcmp(argv[2], "--opcode-sweep")) {
        CHECK(builder && !hm_probe_primitive_opcode_sweep(device, renderer, builder));
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
    CHECK(!hm_probe_texture_feedback(device, renderer, builder));
    CHECK(!hm_probe_vram_transfers(device, renderer, builder));
    CHECK(!hm_probe_direct_transfer_capture(device, renderer, builder));
    CHECK(!hm_probe_dynamic_scene(device, renderer, builder));
    CHECK(!hm_probe_primitive_opcode_sweep(device, renderer, builder));
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
    u32 clear_image[] = {0x020000ff, xy(2, 2), xy(12, 4)};
    CHECK(!fixture(device, renderer, builder, clear_image,
                   sizeof(clear_image) / sizeof(*clear_image), 32, 32, 0));
    u32 wrapped_clear[] = {0x0200ffff, xy(1008, 4), xy(32, 4)};
    CHECK(!fixture(device, renderer, builder, wrapped_clear,
                   sizeof(wrapped_clear) / sizeof(*wrapped_clear), 32, 32, 0));
    u32 load_image_and_sample[] = {0x600000ff, xy(2, 2), xy(4, 4),
                                   0xa0000000, xy(4, 4), xy(4, 1), 0x7fff03e0, 0x001f4210,
                                   0xe1000100, 0x6d000000, xy(12, 12), xy(4, 4)};
    CHECK(!fixture(device, renderer, builder, load_image_and_sample,
                   sizeof(load_image_and_sample) / sizeof(*load_image_and_sample), 32, 32, 0));
    u32 wrapped_load_and_sample[] = {
        0xa0000000, xy(1022, 510), xy(4, 2), 0x03e07c00, 0x42107fff,
        0x001f5ad6, 0x000f0001, 0xe100011f, 0x6d000000, xy(12, 12), xy(62, 254)};
    CHECK(!fixture(device, renderer, builder, wrapped_load_and_sample,
                   sizeof(wrapped_load_and_sample) / sizeof(*wrapped_load_and_sample), 32, 32,
                   0));
    u32 clipping[] = {0xe3000000 | 4 | (3 << 10),
                      0xe4000000 | 22 | (23 << 10),
                      0x200000ff,
                      xy(-8, -8),
                      xy(30, 0),
                      xy(0, 30)};
    CHECK(!fixture(device, renderer, builder, clipping, sizeof(clipping) / sizeof(*clipping), 32,
                   32, 0));
    u32 wide_triangle[] = {0x200000ff, xy(-1024, 16), xy(1023, 16), xy(0, 511)};
    CHECK(!fixture(device, renderer, builder, wide_triangle,
                   sizeof(wide_triangle) / sizeof(*wide_triangle), 32, 32, 0));
    u32 wide_line[] = {0x400000ff, xy(-1024, 16), xy(1023, 16)};
    CHECK(!fixture(device, renderer, builder, wide_line,
                   sizeof(wide_line) / sizeof(*wide_line), 32, 32, 0));
    for (int filtering = 0; filtering <
#ifdef HM_FILTER_REFERENCE
                            2
#else
                            1
#endif
         ;
         ++filtering) {
        texture_filter = filtering;
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
    texture_filter = 0;
    u32 dialog_clip[] = {0xe4000000u | 31u | (63u << 10), 0x600000ff, xy(2, 2), xy(4, 4)};
    CHECK(!fixture(device, renderer, builder, dialog_clip, 4, 32, 32, 0));
    CHECK(!fixture(device, renderer, builder, dialog_clip, 4, 128, 96, 9));
    u32 safe_page[] = {0xe1000600, 0x65808080, xy(2, 2), (0x7ab8u << 16) | 0xf000, xy(4, 4)};
    CHECK(!fixture(device, renderer, builder, safe_page, 5, 32, 32, 0));
    CHECK(!hm_probe_random_cases(device, renderer, builder));
#ifdef HM_FILTER_REFERENCE
    texture_filter = 1;
    CHECK(!hm_probe_random_cases(device, renderer, builder));
    texture_filter = 0;
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
    if (argc > 2 && !SDL_strcmp(argv[2], "--farm-benchmark"))
        CHECK(!hm_probe_farm_benchmark(device, renderer, builder));
    hm_gpu_scene_destroy(builder);
    SDL_WaitForGPUIdle(device);
    hm_gpu_renderer_destroy(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    puts("Synthetic native GPU readback comparisons passed; no game/window started.");
    return 0;
}
