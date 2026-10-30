#include "platform/gpu_renderer.h"
#include "gfx/shaders/scene_dxil.h"
#include "gfx/shaders/scene_spirv.h"
#include "psx/vram.h"
enum {
    HM_GPU_VRAM_TILE_SIZE = 16,
    HM_GPU_VRAM_TILES_X = HM_VRAM_WIDTH / HM_GPU_VRAM_TILE_SIZE,
    HM_GPU_VRAM_TILES_Y = HM_VRAM_HEIGHT / HM_GPU_VRAM_TILE_SIZE,
    HM_GPU_VRAM_TEXTURE_WIDTH = HM_VRAM_WIDTH / 2
};
#define HM_GPU_VRAM_BYTES ((size_t)HM_VRAM_WIDTH * HM_VRAM_HEIGHT * sizeof(u16))
typedef struct {
    u32 tile_y, first_tile, tile_count, offset;
} hm_gpu_vram_region;
struct hm_gpu_renderer {
    SDL_GPUDevice *device;
    SDL_GPUComputePipeline *pipeline;
    SDL_GPUBuffer *buffer;
    SDL_GPUTransferBuffer *upload;
    u32 scene_buffer_size, scene_upload_size;
    SDL_GPUTexture *vram_texture;
    SDL_GPUTransferBuffer *vram_upload;
    u32 vram_layers, vram_upload_size;
    u16 vram_shadow[HM_VRAM_HEIGHT][HM_VRAM_WIDTH];
    int vram_initialized;
    SDL_GPUTexture *output;
    u32 width, height;
    u64 revision;
};
static int hm_gpu_vram_tile_changed(const u16 *current, const u16 *shadow, u32 tile_x,
                                    u32 tile_y) {
    for (u32 row = 0; row < HM_GPU_VRAM_TILE_SIZE; ++row) {
        size_t offset = (size_t)(tile_y * HM_GPU_VRAM_TILE_SIZE + row) * HM_VRAM_WIDTH +
                        tile_x * HM_GPU_VRAM_TILE_SIZE;
        if (SDL_memcmp(current + offset, shadow + offset, HM_GPU_VRAM_TILE_SIZE * sizeof(u16)))
            return 1;
    }
    return 0;
}
static u32 hm_gpu_vram_regions(hm_gpu_renderer *r, const hm_gpu_scene_data *scene,
                               hm_gpu_vram_region regions[HM_GPU_VRAM_TILES_Y], int *full_upload) {
    const u16 *current = scene->vram_pixels;
    const u16 *shadow = &r->vram_shadow[0][0];
    /* Replay can select either retained frame snapshot; shared dirty flags
     * cannot describe a delta from the last GPU image in both directions. */
    *full_upload = !r->vram_initialized || scene->vram_version_count != 0;
    if (*full_upload || !SDL_memcmp(current, shadow, HM_GPU_VRAM_BYTES))
        return 0;

    u32 count = 0, total_bytes = 0;
    for (u32 tile_y = 0; tile_y < HM_GPU_VRAM_TILES_Y; ++tile_y) {
        size_t band_offset = (size_t)tile_y * HM_GPU_VRAM_TILE_SIZE * HM_VRAM_WIDTH;
        size_t band_bytes = (size_t)HM_GPU_VRAM_TILE_SIZE * HM_VRAM_WIDTH * sizeof(u16);
        if (!SDL_memcmp(current + band_offset, shadow + band_offset, band_bytes))
            continue;
        u32 first = HM_GPU_VRAM_TILES_X, last = 0;
        for (u32 tile_x = 0; tile_x < HM_GPU_VRAM_TILES_X; ++tile_x) {
            if (hm_gpu_vram_tile_changed(current, shadow, tile_x, tile_y)) {
                if (first == HM_GPU_VRAM_TILES_X)
                    first = tile_x;
                last = tile_x;
            }
        }
        if (first == HM_GPU_VRAM_TILES_X)
            continue;
        u32 tile_count = last - first + 1;
        regions[count++] = (hm_gpu_vram_region){tile_y, first, tile_count, total_bytes};
        total_bytes += tile_count * HM_GPU_VRAM_TILE_SIZE * HM_GPU_VRAM_TILE_SIZE * sizeof(u16);
    }
    if ((size_t)total_bytes * 4 >= HM_GPU_VRAM_BYTES * 3) {
        *full_upload = 1;
        return 0;
    }
    return count;
}
static int hm_gpu_vram_stage(hm_gpu_renderer *r, const hm_gpu_scene_data *scene,
                             const hm_gpu_vram_region *regions, u32 count, int full_upload) {
    void *mapped = SDL_MapGPUTransferBuffer(r->device, r->vram_upload, true);
    if (!mapped)
        return 0;
    if (full_upload) {
        SDL_memcpy(mapped, scene->vram_pixels, HM_GPU_VRAM_BYTES);
        if (scene->vram_version_count)
            SDL_memcpy((u8 *)mapped + HM_GPU_VRAM_BYTES, scene->vram_versions,
                       (size_t)scene->vram_version_count * HM_GPU_VRAM_BYTES);
    } else {
        u8 *destination = mapped;
        const u16 *source = scene->vram_pixels;
        for (u32 i = 0; i < count; ++i) {
            const hm_gpu_vram_region *region = &regions[i];
            u32 pixels = region->tile_count * HM_GPU_VRAM_TILE_SIZE;
            size_t row_bytes = (size_t)pixels * sizeof(u16);
            u32 source_x = region->first_tile * HM_GPU_VRAM_TILE_SIZE;
            for (u32 row = 0; row < HM_GPU_VRAM_TILE_SIZE; ++row) {
                size_t source_offset =
                    (size_t)(region->tile_y * HM_GPU_VRAM_TILE_SIZE + row) * HM_VRAM_WIDTH +
                    source_x;
                size_t destination_offset = region->offset + (size_t)row * row_bytes;
                SDL_memcpy(destination + destination_offset, source + source_offset, row_bytes);
            }
        }
    }
    SDL_UnmapGPUTransferBuffer(r->device, r->vram_upload);
    return 1;
}
static int hm_gpu_vram_reserve(hm_gpu_renderer *r, u32 layers) {
    u32 needed_layers = layers;
    if (!needed_layers) {
        SDL_SetError("GPU scene requires at least one VRAM layer");
        return 0;
    }
    u32 capacity = r->vram_layers;
    SDL_GPUTexture *texture = NULL;
    if (needed_layers > capacity) {
        capacity = capacity ? capacity : 1;
        while (capacity < needed_layers) {
            if (capacity > UINT32_MAX / 2) {
                capacity = needed_layers;
                break;
            }
            capacity *= 2;
        }
        SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                         .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
                                         .usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ,
                                         .width = HM_GPU_VRAM_TEXTURE_WIDTH,
                                         .height = HM_VRAM_HEIGHT,
                                         .layer_count_or_depth = capacity,
                                         .num_levels = 1};
        texture = SDL_CreateGPUTexture(r->device, &info);
        if (!texture)
            return 0;
    }
    size_t upload_size = (size_t)needed_layers * HM_GPU_VRAM_BYTES;
    if (upload_size > UINT32_MAX) {
        if (texture)
            SDL_ReleaseGPUTexture(r->device, texture);
        SDL_SetError("GPU VRAM snapshot upload exceeds transfer limits");
        return 0;
    }
    SDL_GPUTransferBuffer *upload = NULL;
    if (r->vram_upload_size < upload_size) {
        SDL_GPUTransferBufferCreateInfo transfer = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = (u32)upload_size};
        upload = SDL_CreateGPUTransferBuffer(r->device, &transfer);
        if (!upload) {
            if (texture)
                SDL_ReleaseGPUTexture(r->device, texture);
            return 0;
        }
    }
    if (texture) {
        SDL_ReleaseGPUTexture(r->device, r->vram_texture);
        r->vram_texture = texture;
        r->vram_layers = capacity;
        r->vram_initialized = 0;
    }
    if (upload) {
        SDL_ReleaseGPUTransferBuffer(r->device, r->vram_upload);
        r->vram_upload = upload;
        r->vram_upload_size = (u32)upload_size;
    }
    return 1;
}
static void hm_gpu_vram_commit(hm_gpu_renderer *r, const hm_gpu_scene_data *scene,
                               const hm_gpu_vram_region *regions, u32 count, int full_upload) {
    u16 *destination = &r->vram_shadow[0][0];
    const u16 *source = scene->vram_pixels;
    if (full_upload) {
        SDL_memcpy(destination, source, HM_GPU_VRAM_BYTES);
    } else {
        for (u32 i = 0; i < count; ++i) {
            const hm_gpu_vram_region *region = &regions[i];
            u32 pixels = region->tile_count * HM_GPU_VRAM_TILE_SIZE;
            size_t row_bytes = (size_t)pixels * sizeof(u16);
            u32 source_x = region->first_tile * HM_GPU_VRAM_TILE_SIZE;
            for (u32 row = 0; row < HM_GPU_VRAM_TILE_SIZE; ++row) {
                size_t source_offset =
                    (size_t)(region->tile_y * HM_GPU_VRAM_TILE_SIZE + row) * HM_VRAM_WIDTH +
                    source_x;
                SDL_memcpy(destination + source_offset, source + source_offset, row_bytes);
            }
        }
    }
    r->vram_initialized = 1;
}
static int hm_gpu_scene_buffers_reserve(hm_gpu_renderer *r, u32 needed) {
    u32 buffer_size = r->scene_buffer_size;
    u32 upload_size = r->scene_upload_size;
    if (buffer_size < needed) {
        buffer_size = buffer_size ? buffer_size : 65536;
        while (buffer_size < needed) {
            if (buffer_size > UINT32_MAX / 2) {
                buffer_size = needed;
                break;
            }
            buffer_size *= 2;
        }
    }
    if (upload_size < needed) {
        upload_size = upload_size ? upload_size : 65536;
        while (upload_size < needed) {
            if (upload_size > UINT32_MAX / 2) {
                upload_size = needed;
                break;
            }
            upload_size *= 2;
        }
    }
    SDL_GPUBuffer *buffer = NULL;
    SDL_GPUTransferBuffer *upload = NULL;
    if (buffer_size != r->scene_buffer_size) {
        SDL_GPUBufferCreateInfo info = {
            .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = buffer_size};
        buffer = SDL_CreateGPUBuffer(r->device, &info);
        if (!buffer)
            return 0;
    }
    if (upload_size != r->scene_upload_size) {
        SDL_GPUTransferBufferCreateInfo info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = upload_size};
        upload = SDL_CreateGPUTransferBuffer(r->device, &info);
        if (!upload) {
            if (buffer)
                SDL_ReleaseGPUBuffer(r->device, buffer);
            return 0;
        }
    }
    if (buffer) {
        SDL_ReleaseGPUBuffer(r->device, r->buffer);
        r->buffer = buffer;
        r->scene_buffer_size = buffer_size;
    }
    if (upload) {
        SDL_ReleaseGPUTransferBuffer(r->device, r->upload);
        r->upload = upload;
        r->scene_upload_size = upload_size;
    }
    return 1;
}
hm_gpu_renderer *hm_gpu_renderer_create(SDL_GPUDevice *device) {
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUComputePipelineCreateInfo info = {.entrypoint = "main",
                                             .num_readonly_storage_textures = 1,
                                             .num_readonly_storage_buffers = 1,
                                             .num_readwrite_storage_textures = 1,
                                             .threadcount_x = 8,
                                             .threadcount_y = 8,
                                             .threadcount_z = 1};
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = hm_scene_spirv;
        info.code_size = sizeof(hm_scene_spirv);
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = hm_scene_dxil;
        info.code_size = sizeof(hm_scene_dxil);
    } else {
        SDL_SetError("GPU scene shader format unavailable");
        return NULL;
    }
    if (!SDL_GPUTextureSupportsFormat(
            device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE))
        return NULL;
    if (!SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R32_UINT,
                                      SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                      SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ)) {
        SDL_SetError("GPU VRAM cache format unavailable");
        return NULL;
    }
    hm_gpu_renderer *r = SDL_calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->device = device;
    r->pipeline = SDL_CreateGPUComputePipeline(device, &info);
    SDL_GPUBufferCreateInfo buffer = {.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
                                      .size = 65536};
    SDL_GPUTransferBufferCreateInfo upload = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                              .size = buffer.size};
    SDL_GPUTextureCreateInfo vram = {.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                     .format = SDL_GPU_TEXTUREFORMAT_R32_UINT,
                                     .usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ,
                                     .width = HM_GPU_VRAM_TEXTURE_WIDTH,
                                     .height = HM_VRAM_HEIGHT,
                                     .layer_count_or_depth = 2,
                                     .num_levels = 1};
    SDL_GPUTransferBufferCreateInfo vram_upload = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = (u32)HM_GPU_VRAM_BYTES};
    r->buffer = SDL_CreateGPUBuffer(device, &buffer);
    r->upload = SDL_CreateGPUTransferBuffer(device, &upload);
    r->vram_texture = SDL_CreateGPUTexture(device, &vram);
    r->vram_upload = SDL_CreateGPUTransferBuffer(device, &vram_upload);
    r->vram_layers = 2;
    r->vram_upload_size = (u32)HM_GPU_VRAM_BYTES;
    r->scene_buffer_size = buffer.size;
    r->scene_upload_size = upload.size;
    if (!r->pipeline || !r->buffer || !r->upload || !r->vram_texture || !r->vram_upload) {
        hm_gpu_renderer_destroy(r);
        return NULL;
    }
    return r;
}
void hm_gpu_renderer_destroy(hm_gpu_renderer *r) {
    if (!r)
        return;
    if (r->output)
        SDL_ReleaseGPUTexture(r->device, r->output);
    if (r->upload)
        SDL_ReleaseGPUTransferBuffer(r->device, r->upload);
    if (r->vram_upload)
        SDL_ReleaseGPUTransferBuffer(r->device, r->vram_upload);
    if (r->vram_texture)
        SDL_ReleaseGPUTexture(r->device, r->vram_texture);
    if (r->buffer)
        SDL_ReleaseGPUBuffer(r->device, r->buffer);
    if (r->pipeline)
        SDL_ReleaseGPUComputePipeline(r->device, r->pipeline);
    SDL_free(r);
}
SDL_GPUTexture *hm_gpu_renderer_encode(hm_gpu_renderer *r, SDL_GPUCommandBuffer *command,
                                       const hm_gpu_scene_data *scene) {
    if (!r || !scene->words || !scene->vram_pixels ||
        !scene->bytes || scene->bytes > UINT32_MAX || !scene->width || !scene->height ||
        scene->width > 4096 || scene->height > 4096) {
        SDL_SetError("Invalid GPU scene payload");
        return NULL;
    }
    if (!hm_gpu_scene_buffers_reserve(r, (u32)scene->bytes))
        return NULL;
    u32 scene_layers = scene->vram_version_count + 1;
    if (!hm_gpu_vram_reserve(r, scene_layers))
        return NULL;
    if (r->width != scene->width || r->height != scene->height) {
        SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
                                         .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                         .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                                  SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
                                         .width = scene->width,
                                         .height = scene->height,
                                         .layer_count_or_depth = 1,
                                         .num_levels = 1};
        SDL_GPUTexture *output = SDL_CreateGPUTexture(r->device, &info);
        if (!output)
            return NULL;
        if (r->output)
            SDL_ReleaseGPUTexture(r->device, r->output);
        r->output = output;
        r->width = scene->width;
        r->height = scene->height;
        r->revision = 0;
    }
    if (r->revision == scene->revision)
        return r->output;
    hm_gpu_vram_region regions[HM_GPU_VRAM_TILES_Y];
    int full_vram_upload = 0;
    u32 vram_region_count = hm_gpu_vram_regions(r, scene, regions, &full_vram_upload);
    void *mapped = SDL_MapGPUTransferBuffer(r->device, r->upload, true);
    if (!mapped)
        return NULL;
    SDL_memcpy(mapped, scene->words, scene->bytes);
    SDL_UnmapGPUTransferBuffer(r->device, r->upload);
    if ((full_vram_upload || vram_region_count) &&
        !hm_gpu_vram_stage(r, scene, regions, vram_region_count, full_vram_upload))
        return NULL;
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    if (!copy)
        return NULL;
    SDL_GPUTransferBufferLocation source = {.transfer_buffer = r->upload};
    SDL_GPUBufferRegion destination = {.buffer = r->buffer, .size = (u32)scene->bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    if (full_vram_upload) {
        for (u32 layer = 0; layer < scene_layers; ++layer) {
            SDL_GPUTextureTransferInfo vram_source = {
                .transfer_buffer = r->vram_upload,
                .offset = layer * (u32)HM_GPU_VRAM_BYTES,
                .pixels_per_row = HM_GPU_VRAM_TEXTURE_WIDTH,
                .rows_per_layer = HM_VRAM_HEIGHT};
            SDL_GPUTextureRegion vram_destination = {.texture = r->vram_texture,
                                                     .layer = layer,
                                                     .w = HM_GPU_VRAM_TEXTURE_WIDTH,
                                                     .h = HM_VRAM_HEIGHT,
                                                     .d = 1};
            SDL_UploadToGPUTexture(copy, &vram_source, &vram_destination, false);
        }
    } else {
        for (u32 i = 0; i < vram_region_count; ++i) {
            const hm_gpu_vram_region *region = &regions[i];
            u32 width = region->tile_count * (HM_GPU_VRAM_TILE_SIZE / 2);
            SDL_GPUTextureTransferInfo vram_source = {.transfer_buffer = r->vram_upload,
                                                      .offset = region->offset,
                                                      .pixels_per_row = width,
                                                      .rows_per_layer = HM_GPU_VRAM_TILE_SIZE};
            SDL_GPUTextureRegion vram_destination = {
                .texture = r->vram_texture,
                .x = region->first_tile * (HM_GPU_VRAM_TILE_SIZE / 2),
                .y = region->tile_y * HM_GPU_VRAM_TILE_SIZE,
                .w = width,
                .h = HM_GPU_VRAM_TILE_SIZE,
                .d = 1};
            SDL_UploadToGPUTexture(copy, &vram_source, &vram_destination, false);
        }
    }
    SDL_EndGPUCopyPass(copy);
    SDL_GPUStorageTextureReadWriteBinding output = {.texture = r->output, .cycle = true};
    SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(command, &output, 1, NULL, 0);
    if (!compute)
        return NULL;
    SDL_BindGPUComputePipeline(compute, r->pipeline);
    SDL_BindGPUComputeStorageBuffers(compute, 0, &r->buffer, 1);
    SDL_BindGPUComputeStorageTextures(compute, 0, &r->vram_texture, 1);
    SDL_DispatchGPUCompute(compute, (scene->width + 7) / 8, (scene->height + 7) / 8, 1);
    SDL_EndGPUComputePass(compute);
    if (full_vram_upload || vram_region_count)
        hm_gpu_vram_commit(r, scene, regions, vram_region_count, full_vram_upload);
    r->revision = scene->revision;
    return r->output;
}
