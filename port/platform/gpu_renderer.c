#include "platform/gpu_renderer.h"
#include "gfx/shaders/scene_dxil.h"
#include "gfx/shaders/scene_spirv.h"
struct hm_gpu_renderer {
    SDL_GPUDevice *device;
    SDL_GPUComputePipeline *pipeline;
    SDL_GPUBuffer *buffer;
    SDL_GPUTransferBuffer *upload;
    SDL_GPUTexture *output;
    u32 width, height;
    u64 revision;
};
hm_gpu_renderer *hm_gpu_renderer_create(SDL_GPUDevice *device) {
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUComputePipelineCreateInfo info = {.entrypoint = "main",
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
    hm_gpu_renderer *r = SDL_calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->device = device;
    r->pipeline = SDL_CreateGPUComputePipeline(device, &info);
    SDL_GPUBufferCreateInfo buffer = {.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
                                      .size = HM_SCENE_MAX_WORDS * sizeof(u32)};
    SDL_GPUTransferBufferCreateInfo upload = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                              .size = buffer.size};
    r->buffer = SDL_CreateGPUBuffer(device, &buffer);
    r->upload = SDL_CreateGPUTransferBuffer(device, &upload);
    if (!r->pipeline || !r->buffer || !r->upload) {
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
    if (r->buffer)
        SDL_ReleaseGPUBuffer(r->device, r->buffer);
    if (r->pipeline)
        SDL_ReleaseGPUComputePipeline(r->device, r->pipeline);
    SDL_free(r);
}
SDL_GPUTexture *hm_gpu_renderer_encode(hm_gpu_renderer *r, SDL_GPUCommandBuffer *command,
                                       const hm_gpu_scene_data *scene) {
    if (!r || !scene->words || scene->bytes > HM_SCENE_MAX_WORDS * sizeof(u32) || !scene->width ||
        !scene->height || scene->width > 4096 || scene->height > 4096) {
        SDL_SetError("Invalid GPU scene payload");
        return NULL;
    }
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
    void *mapped = SDL_MapGPUTransferBuffer(r->device, r->upload, true);
    if (!mapped)
        return NULL;
    SDL_memcpy(mapped, scene->words, scene->bytes);
    SDL_UnmapGPUTransferBuffer(r->device, r->upload);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    if (!copy)
        return NULL;
    SDL_GPUTransferBufferLocation source = {.transfer_buffer = r->upload};
    SDL_GPUBufferRegion destination = {.buffer = r->buffer, .size = (u32)scene->bytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUStorageTextureReadWriteBinding output = {.texture = r->output, .cycle = true};
    SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(command, &output, 1, NULL, 0);
    if (!compute)
        return NULL;
    SDL_BindGPUComputePipeline(compute, r->pipeline);
    SDL_BindGPUComputeStorageBuffers(compute, 0, &r->buffer, 1);
    SDL_DispatchGPUCompute(compute, (scene->width + 7) / 8, (scene->height + 7) / 8, 1);
    SDL_EndGPUComputePass(compute);
    r->revision = scene->revision;
    return r->output;
}
