#include "platform/fxaa.h"
#include "enhance/antialiasing/fxaa_dxil.h"
#include "enhance/antialiasing/fxaa_spirv.h"
struct hm_fxaa {
    SDL_GPUDevice *device;
    SDL_GPUComputePipeline *pipeline;
    SDL_GPUSampler *sampler;
    SDL_GPUTexture *output;
    u32 width, height;
};
hm_fxaa *hm_fxaa_create(SDL_GPUDevice *device) {
    SDL_GPUComputePipelineCreateInfo info = {.entrypoint = "main", .num_samplers = 1,
        .num_readwrite_storage_textures = 1, .threadcount_x = 8, .threadcount_y = 8, .threadcount_z = 1};
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = hm_fxaa_spirv;
        info.code_size = sizeof(hm_fxaa_spirv);
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = hm_fxaa_dxil;
        info.code_size = sizeof(hm_fxaa_dxil);
    } else return NULL;
    hm_fxaa *f = SDL_calloc(1, sizeof(*f));
    if (!f) return NULL;
    f->device = device;
    f->pipeline = SDL_CreateGPUComputePipeline(device, &info);
    SDL_GPUSamplerCreateInfo sampler = {.min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR, .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
    f->sampler = SDL_CreateGPUSampler(device, &sampler);
    if (!f->pipeline || !f->sampler) { hm_fxaa_destroy(f); return NULL; }
    return f;
}
void hm_fxaa_destroy(hm_fxaa *f) {
    if (!f) return;
    if (f->output) SDL_ReleaseGPUTexture(f->device, f->output);
    if (f->sampler) SDL_ReleaseGPUSampler(f->device, f->sampler);
    if (f->pipeline) SDL_ReleaseGPUComputePipeline(f->device, f->pipeline);
    SDL_free(f);
}
SDL_GPUTexture *hm_fxaa_encode(hm_fxaa *f, SDL_GPUCommandBuffer *command,
                               SDL_GPUTexture *source, u32 width, u32 height) {
    if (!f || !source || !width || !height || width > 4096 || height > 4096) {
        SDL_SetError("Invalid FXAA canvas"); return NULL;
    }
    if (f->width != width || f->height != height) {
        SDL_GPUTextureCreateInfo info = {.type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE,
            .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1};
        SDL_GPUTexture *texture = SDL_CreateGPUTexture(f->device, &info);
        if (!texture) return NULL;
        if (f->output) SDL_ReleaseGPUTexture(f->device, f->output);
        f->output = texture; f->width = width; f->height = height;
    }
    SDL_GPUStorageTextureReadWriteBinding target = {.texture = f->output, .cycle = true};
    SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(command, &target, 1, NULL, 0);
    if (!pass) return NULL;
    SDL_BindGPUComputePipeline(pass, f->pipeline);
    SDL_GPUTextureSamplerBinding binding = {.texture = source, .sampler = f->sampler};
    SDL_BindGPUComputeSamplers(pass, 0, &binding, 1);
    SDL_DispatchGPUCompute(pass, (width + 7) / 8, (height + 7) / 8, 1);
    SDL_EndGPUComputePass(pass);
    return f->output;
}
