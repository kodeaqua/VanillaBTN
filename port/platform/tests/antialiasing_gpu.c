/* Synthetic GPU checks only: no game, disc, window or owner files. */
#include "enhance/antialiasing/antialiasing.h"
#include "platform/fxaa.h"
#include "platform/gpu_renderer.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s (%s)\n", __LINE__, #x, SDL_GetError()); return 1; } } while (0)
static u8 pixels[1280 * 960 * 4];
static u32 xy(int x, int y) { return (u16)x | ((u32)(u16)y << 16); }
static int download(SDL_GPUDevice *device, SDL_GPUCommandBuffer *command, SDL_GPUTexture *image,
                    u32 width, u32 height) {
    SDL_GPUTransferBufferCreateInfo info = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                           .size = width * height * 4};
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &info);
    CHECK(transfer);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    CHECK(copy);
    SDL_GPUTextureRegion source = {.texture = image, .w = width, .h = height, .d = 1};
    SDL_GPUTextureTransferInfo target = {.transfer_buffer = transfer, .pixels_per_row = width,
                                        .rows_per_layer = height};
    SDL_DownloadFromGPUTexture(copy, &source, &target);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    CHECK(fence && SDL_WaitForGPUFences(device, true, &fence, 1));
    SDL_ReleaseGPUFence(device, fence);
    void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    CHECK(mapped);
    SDL_memcpy(pixels, mapped, info.size);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return 0;
}
static int encode(SDL_GPUDevice *device, hm_gpu_renderer *renderer, hm_fxaa *fxaa,
                  hm_gpu_scene_data *scene, int filter) {
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    CHECK(command);
    SDL_GPUTexture *image = hm_gpu_renderer_encode(renderer, command, scene);
    CHECK(image);
    if (filter) { image = hm_fxaa_encode(fxaa, command, image, scene->width, scene->height); CHECK(image); }
    return download(device, command, image, scene->width, scene->height);
}
static int scene(hm_gpu_scene *builder, const u32 *words, unsigned count, unsigned samples,
                 unsigned size, hm_gpu_scene_data *data) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    SetDefDrawEnv(&draw, 0, 0, 32, 32); draw.dtd = 0; draw.dfe = 1; draw.tpage = 0x20;
    PutDrawEnv(&draw);
    hm_gpu_scene_multisamples(builder, samples);
    hm_replay_sink previous = hm_gpu_scene_begin(builder, size, size);
    RECT area = {0, 0, 32, 32};
    hm_replay_begin(&area);
    hm_gpu_execute_words(words, count);
    CHECK(hm_gpu_scene_end(builder, previous, data));
    CHECK(data->words[15] == samples);
    return 0;
}
static int test(SDL_GPUDevice *device, hm_gpu_renderer *renderer, hm_fxaa *fxaa,
                hm_gpu_scene *builder) {
    u32 triangle[] = {0x200000ff, xy(2,2), xy(30,4), xy(5,29)};
    hm_gpu_scene_data data;
    for (unsigned size = 32; size <= 96; size *= 3)
        for (unsigned samples = 1; samples <= 4; samples *= 2) {
            CHECK(!scene(builder, triangle, 4, samples, size, &data));
            CHECK(!encode(device, renderer, fxaa, &data, 0));
            s32 v[] = {2*256*(int)size/32, 2*256*(int)size/32, 30*256*(int)size/32,
                       4*256*(int)size/32, 5*256*(int)size/32, 29*256*(int)size/32};
            for (unsigned y = 0; y < size; ++y)
                for (unsigned x = 0; x < size; ++x) {
                    unsigned mask = hm_antialiasing_coverage(v, (int)x, (int)y, samples), hits = 0;
                    for (unsigned i = 0; i < samples; ++i) hits += (mask >> i) & 1;
                    unsigned expected = (255 * hits + samples/2) / samples;
                    const u8 *pixel = pixels + (y*size+x)*4;
                    if (SDL_abs((int)pixel[0]-(int)expected)>1) {
                        fprintf(stderr, "Coverage mismatch %u samples, %u,%u: %u != %u\n", samples,x,y,pixel[0],expected); return 1;
                    }
                    CHECK(pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255);
                }
        }
    /* Two adjacent additive triangles: every sample belongs to exactly one. */
    u32 joined[] = {0x22000080,xy(8,8),xy(24,8),xy(8,24),
                   0x22000080,xy(24,24),xy(8,24),xy(24,8)};
    for (unsigned samples = 2; samples <= 4; samples *= 2) {
        CHECK(!scene(builder, joined, 8, samples, 32, &data));
        CHECK(!encode(device, renderer, fxaa, &data, 0));
        for (unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x)
            CHECK(pixels[(y*32+x)*4] == (x>=8&&x<24&&y>=8&&y<24?132:0));
    }
    /* Per-sample mask bits preserve red coverage when a green rectangle follows. */
    u32 masked[] = {0xe6000001,0x200000ff,xy(2,2),xy(30,4),xy(5,29),
                    0xe6000002,0x6000ff00,xy(0,0),xy(32,32)};
    CHECK(!scene(builder, masked, 9, 4, 32, &data));
    CHECK(!encode(device, renderer, fxaa, &data, 0));
    const s32 v[] = {512,512,7680,1024,1280,7424};
    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x) {
        unsigned mask=hm_antialiasing_coverage(v,(int)x,(int)y,4),hits=0;
        for(unsigned i=0;i<4;++i)hits+=(mask>>i)&1;
        CHECK(SDL_abs(pixels[(y*32+x)*4]-(int)((255*hits+2)/4))<=1);
        CHECK(SDL_abs(pixels[(y*32+x)*4+1]-(int)((255*(4-hits)+2)/4))<=1);
    }
    CHECK(!scene(builder, triangle, 4, 1, 32, &data));
    CHECK(!encode(device, renderer, fxaa, &data, 1));
    unsigned intermediate=0;
    for(unsigned i=0;i<32*32;++i) intermediate+=pixels[i*4]>0&&pixels[i*4]<255;
    CHECK(intermediate>10 && pixels[(10*32+10)*4]==255 && pixels[0]==0);
    u32 flat[] = {0x60808080,xy(0,0),xy(32,32)};
    for(unsigned size=32;size<=96;size*=3) {
        CHECK(!scene(builder,flat,3,1,size,&data));
        CHECK(!encode(device,renderer,fxaa,&data,1));
        for(unsigned i=0;i<size*size;++i)
            CHECK(pixels[i*4]==132&&pixels[i*4+1]==132&&pixels[i*4+2]==132&&pixels[i*4+3]==255);
    }
    return 0;
}
int main(int argc, char **argv) {
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_GPUDevice *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,argc>1?argv[1]:NULL);
    CHECK(device);
    hm_gpu_renderer *renderer=hm_gpu_renderer_create(device);
    hm_gpu_scene *builder=hm_gpu_scene_create();
    hm_fxaa *fxaa=hm_fxaa_create(device);
    CHECK(renderer&&builder&&fxaa);
    CHECK(!test(device,renderer,fxaa,builder));
    SDL_WaitForGPUIdle(device);
    hm_fxaa_destroy(fxaa);hm_gpu_scene_destroy(builder);hm_gpu_renderer_destroy(renderer);
    SDL_DestroyGPUDevice(device);SDL_Quit();
    puts("Offscreen FXAA/MSAA coverage, masks, seams and resizing passed; no gameplay started.");
    return 0;
}
